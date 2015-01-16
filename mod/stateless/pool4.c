/*
 * TODO
 *
 *
 *In RFC 792, the "original datagram" field includes the IP
   header plus the next eight octets of the original datagram.
   [RFC1812] extends the "original datagram" field to contain as many
   octets as possible without causing the ICMP message to exceed the
   minimum IPv4 reassembly buffer size (i.e., 576 octets).

In RFC
   4443, the "original datagram" field always contained as many octets
   as possible without causing the ICMP message to exceed the minimum
   IPv6 MTU (i.e., 1280 octets)


      - ICMPv6 Packet Too Big (type = 2)

      - ICMPv6 Parameter Problem (type = 4)
 */

#include "nat64/mod/stateless/pool4.h"

#include <linux/rculist.h>
#include <linux/inet.h>

#include "nat64/comm/str_utils.h"
#include "nat64/mod/common/random.h"

struct pool_entry {
	struct ipv4_prefix prefix;
	struct list_head list_hook;
};

static struct list_head pool;

static int parse_prefix4(const char *str, struct ipv4_prefix *prefix)
{
	const char *slash_pos;
	int error = 0;

	if (strchr(str, '/') != 0) {
		if (in4_pton(str, -1, (u8 *) &prefix->address, '/', &slash_pos) != 1)
			error = -EINVAL;
		if (kstrtou8(slash_pos + 1, 0, &prefix->len) != 0)
			error = -EINVAL;
	} else {
		error = str_to_addr4(str, &prefix->address);
		prefix->len = 32;
	}

	if (error)
		log_err("Address is malformed: %s.", str);

	return error;
}

int pool4_init(char *pref_strs[], int pref_count)
{
	struct pool_entry *entry;
	unsigned int i;
	int error;

	INIT_LIST_HEAD(&pool);

	for (i = 0; i < pref_count; i++) {
		entry = kmalloc(sizeof(*entry), GFP_KERNEL);
		if (!entry) {
			error = -ENOMEM;
			goto revert;
		}

		error = parse_prefix4(pref_strs[i], &entry->prefix);
		if (error) {
			kfree(entry);
			goto revert;
		}

		list_add_tail(&entry->list_hook, &pool);
	}

	return 0;

revert:
	pool4_destroy();
	return error;
}

void pool4_destroy(void)
{
	struct pool_entry *entry;

	while (!list_empty(&pool)) {
		entry = list_first_entry(&pool, struct pool_entry, list_hook);
		list_del(&entry->list_hook);
		kfree(entry);
	}
}

int pool4_add(struct ipv4_prefix *prefix)
{
	struct pool_entry *entry;

	entry = kmalloc(sizeof(*entry), GFP_KERNEL);
	if (!entry)
		return -ENOMEM;
	entry->prefix = *prefix;

	list_add_tail_rcu(&entry->list_hook, &pool);
	return 0;
}

int pool4_remove(struct ipv4_prefix *prefix)
{
	struct pool_entry *entry;

	list_for_each_entry(entry, &pool, list_hook) {
		if (ipv4_prefix_equals(prefix, &entry->prefix)) {
			list_del_rcu(&entry->list_hook);
			kfree(entry);
			return 0;
		}
	}

	return -ENOENT;
}

static unsigned int get_addr_count(struct ipv4_prefix *prefix)
{
	return 1 << (32 - prefix->len);
}

static unsigned int get_prefix_count(void)
{
	struct pool_entry *entry;
	unsigned int result = 0;

	list_for_each_entry_rcu(entry, &pool, list_hook) {
		result += get_addr_count(&entry->prefix);
	}

	return result;
}

int pool4_get(struct in_addr *result)
{
	struct pool_entry *entry;
	unsigned int count;
	unsigned int rand;

	rcu_read_lock();

	/*
	 * I'm counting the list elements instead of using an algorithm like reservoir sampling
	 * (http://stackoverflow.com/questions/54059) because the random function can be really
	 * expensive. Reservoir sampling requires one random per iteration, this way requires one
	 * random period.
	 */
	count = get_prefix_count();
	if (count == 0) {
		rcu_read_unlock();
		log_warn_once("The IPv4 pool is empty.");
		return -EEXIST;
	}

	rand = get_random_u32() % count;

	list_for_each_entry_rcu(entry, &pool, list_hook) {
		count = get_addr_count(&entry->prefix);
		if (count >= rand)
			break;
		rand -= count;
	}

	result->s_addr = htonl(ntohl(entry->prefix.address.s_addr) | rand);

	rcu_read_unlock();
	return 0;
}

int pool4_for_each(int (*func)(struct ipv4_prefix *, void *), void *arg)
{
	struct pool_entry *entry;
	int error;

	list_for_each_entry_rcu(entry, &pool, list_hook) {
		error = func(&entry->prefix, arg);
		if (error)
			return error;
	}

	return 0;
}

int pool4_count(__u64 *result)
{
	rcu_read_lock();
	*result = get_prefix_count();
	rcu_read_unlock();
	return 0;
}
