#include <linux/module.h>
#include "nat64/unit/types.h"
#include "nat64/unit/unit_test.h"
#include "bib/port_allocator.c"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Alberto Leiva");
MODULE_DESCRIPTION("Port allocator module test.");

static bool test_md5(void)
{
	struct tuple tuple6;
	unsigned int result;

	memset(&tuple6, 0, sizeof(tuple6));

	tuple6.src.addr6.l3.s6_addr[0] = 'a';
	tuple6.src.addr6.l3.s6_addr[1] = 'b';
	tuple6.src.addr6.l3.s6_addr[2] = 'c';
	tuple6.src.addr6.l3.s6_addr[3] = 'd';
	tuple6.src.addr6.l3.s6_addr[4] = 'e';
	tuple6.src.addr6.l3.s6_addr[5] = 'f';
	tuple6.src.addr6.l3.s6_addr[6] = 'g';
	tuple6.src.addr6.l3.s6_addr[7] = 'h';
	tuple6.src.addr6.l3.s6_addr[8] = 'i';
	tuple6.src.addr6.l3.s6_addr[9] = 'j';
	tuple6.src.addr6.l3.s6_addr[10] = 'k';
	tuple6.src.addr6.l3.s6_addr[11] = 'l';
	tuple6.src.addr6.l3.s6_addr[12] = 'm';
	tuple6.src.addr6.l3.s6_addr[13] = 'n';
	tuple6.src.addr6.l3.s6_addr[14] = 'o';
	tuple6.src.addr6.l3.s6_addr[15] = 'p';
	tuple6.dst.addr6.l3.s6_addr[0] = 'q';
	tuple6.dst.addr6.l3.s6_addr[1] = 'r';
	tuple6.dst.addr6.l3.s6_addr[2] = 's';
	tuple6.dst.addr6.l3.s6_addr[3] = 't';
	tuple6.dst.addr6.l3.s6_addr[4] = 'u';
	tuple6.dst.addr6.l3.s6_addr[5] = 'v';
	tuple6.dst.addr6.l3.s6_addr[6] = 'w';
	tuple6.dst.addr6.l3.s6_addr[7] = 'x';
	tuple6.dst.addr6.l3.s6_addr[8] = 'y';
	tuple6.dst.addr6.l3.s6_addr[9] = 'z';
	tuple6.dst.addr6.l3.s6_addr[10] = 'A';
	tuple6.dst.addr6.l3.s6_addr[11] = 'B';
	tuple6.dst.addr6.l3.s6_addr[12] = 'C';
	tuple6.dst.addr6.l3.s6_addr[13] = 'D';
	tuple6.dst.addr6.l3.s6_addr[14] = 'E';
	tuple6.dst.addr6.l3.s6_addr[15] = 'F';
	tuple6.dst.addr6.l4 = cpu_to_be16(('G' << 8) | 'H');

	secret_key[0] = 'I';
	secret_key[1] = 'J';
	secret_key_len = 2;

	/* Expected value gotten from DuckDuckGo. Look up "md5 abcdefg...". */
	return ASSERT_INT(0, f(&tuple6, &result), "errcode")
			&& ASSERT_BE32(0xb6a824a9u, result, "hash");
}

static int set_f_args(unsigned int args)
{
	struct global_config *cfg;
	int error;

	cfg = kmalloc(sizeof(*cfg), GFP_KERNEL);
	if (!cfg)
		return -ENOMEM;
	cfg->mtu_plateaus = NULL;

	error = config_clone(cfg);
	if (error)
		return error;

	cfg->nat64.f_args = args;

	config_replace(cfg);
	return 0;
}

static bool f_args_test(void)
{
	struct tuple tuple6;
	bool success = true;
	unsigned int result1;
	unsigned int result2;

	if (set_f_args(0b1111))
		return false;
	if (init_tuple6(&tuple6, "1::1", 1111, "2::2", 2222, L4PROTO_TCP))
		return false;

	success &= ASSERT_INT(0, f(&tuple6, &result1), "result 1");
	success &= ASSERT_INT(0, f(&tuple6, &result2), "result 2");
	success &= ASSERT_UINT(result1, result2,
			"Same arguments, result has to be the same");

	tuple6.src.addr6.l4 = 0;

	/*
	 * All fields matter, so a small change should yield a different result.
	 * Since this is a hash and the secret key is secret, there is a very
	 * small change this test will spit a false negative.
	 * But the chance is small enough that it shouldn't matter.
	 */
	success &= ASSERT_INT(0, f(&tuple6, &result2), "result 3");
	success &= ASSERT_BOOL(true, result1 != result2,
			"Small change on all fields matter");

	if (set_f_args(0b0010))
		return false;
	if (init_tuple6(&tuple6, "1::1", 1111, "2::2", 2222, L4PROTO_TCP))
		return false;

	success &= ASSERT_INT(0, f(&tuple6, &result1), "result 4");
	success &= ASSERT_INT(0, f(&tuple6, &result2), "result 5");
	success &= ASSERT_UINT(result1, result2,
			"Same arguments, fewer arguments than first test");

	memset(&tuple6.src, 3, sizeof(tuple6.src));
	tuple6.dst.addr6.l4 = 3333;

	success &= ASSERT_INT(0, f(&tuple6, &result2), "result 6");
	success &= ASSERT_UINT(result1, result2,
			"All fields that don't matter changed");

	memset(&tuple6.dst.addr6.l3, 3, sizeof(tuple6.dst.addr6.l3));

	success &= ASSERT_INT(0, f(&tuple6, &result2), "result 7");
	success &= ASSERT_BOOL(true, result1 != result2,
			"The one field that matters changed");

	return success;
}

static int init(void)
{
	int error;

	error = config_init(false);
	if (error)
		return error;

	return palloc_init();
}

static void destroy(void)
{
	palloc_destroy();
	config_destroy();
}

int init_module(void)
{
	int error;
	START_TESTS("Port Allocator");

	error = init();
	if (error)
		return error;

	CALL_TEST(test_md5(), "MD5 Test");
	CALL_TEST(f_args_test(), "F() arguments test");

	destroy();

	END_TESTS;
}

void cleanup_module(void)
{
	/* No code. */
}
