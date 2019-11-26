/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                                                             *
 *                       Balagan -- Hash functions library                     *
 *                                                                             *
 *  Copyright (C) 2014 Synarete                                                *
 *                                                                             *
 *  Balagan hash-functions library is a free software; you can redistribute it *
 *  and/or modify it under the terms of the GNU Lesser General Public License  *
 *  as published by the Free Software Foundation; either version 3 of the      *
 *  License, or (at your option) any later version.                            *
 *                                                                             *
 *  Balagan is distributed in the hope that it will be useful, but WITHOUT ANY *
 *  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS  *
 *  FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for    *
 *  more details.                                                              *
 *                                                                             *
 *  You should have received a copy of GNU Lesser General Public License       *
 *  along with the library. If not, see <http://www.gnu.org/licenses/>         *
 *                                                                             *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <error.h>

#include "fnxhash.h"

#define assert_eq(a, b) \
	if ((a) != (b)) \
		error_at_line(1, 0, __FILE__, __LINE__, "%s != %s", #a, #b)


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * See: http://www.md5calc.com/
 */
static void utest_adler32(void)
{
	size_t len;
	uint32_t hash;
	const char *str;

	str  = "0123456789ABCDEF";
	len  = strlen(str);
	blgn_adler32(str, len, &hash);
	assert_eq(hash, 0x1ccb03a3);

	str  = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	len  = strlen(str);
	blgn_adler32(str, len, &hash);
	assert_eq(hash, 0x64a607e0);

	str  = "0123456789abcdefghijklmnopqrstuvwxyz";
	len  = strlen(str);
	blgn_adler32(str, len, &hash);
	assert_eq(hash, 0xd0d70d2d);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void utest_crc32c(void)
{
	size_t len;
	uint32_t hash;
	const char *str;

	str  = "123456789";
	len  = strlen(str);
	blgn_crc32c(str, len, &hash);
	assert_eq(hash, 0xe3069283);

	str  = "0123456789ABCDEF";
	len  = strlen(str);
	blgn_crc32c(str, len, &hash);
	assert_eq(hash, 0xb5d83007);

	str  = "0123456789abcdefghijklmnopqrstuvwxyz";
	len  = strlen(str);
	blgn_crc32c(str, len, &hash);
	assert_eq(hash, 0xb0e42986);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * See: http://www.isthe.com/chongo/tech/comp/fnv/
 */
static void utest_fnv32(void)
{
	uint32_t hash;
	size_t len;
	const char *str;

	str = "0123456789";
	len = strlen(str);
	blgn_fnv32a(str, len, &hash);
	assert_eq(hash, 0xf9808ff2);

	str = "d25f9916-9120-4247-9805-1813c05211bd";
	len = strlen(str);
	blgn_fnv32a(str, len, &hash);
	assert_eq(hash, 0x2c6f9fa5);

	str = "ooGhaeNgie1fi3iphe0aeF9lair5aihadohg7phoo3thoquaer9ohf6upohnie8B";
	len = strlen(str);
	blgn_fnv32a(str, len, &hash);
	assert_eq(hash, 0x5c692960);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * See: https://131002.net/siphash/
 *      https://github.com/floodyberry/siphash
 */
static const uint64_t siphash_test_vectors[64] = {
	0x726fdb47dd0e0e31ull, 0x74f839c593dc67fdull,
	0x0d6c8009d9a94f5aull, 0x85676696d7fb7e2dull,
	0xcf2794e0277187b7ull, 0x18765564cd99a68dull,
	0xcbc9466e58fee3ceull, 0xab0200f58b01d137ull,
	0x93f5f5799a932462ull, 0x9e0082df0ba9e4b0ull,
	0x7a5dbbc594ddb9f3ull, 0xf4b32f46226bada7ull,
	0x751e8fbc860ee5fbull, 0x14ea5627c0843d90ull,
	0xf723ca908e7af2eeull, 0xa129ca6149be45e5ull,
	0x3f2acc7f57c29bdbull, 0x699ae9f52cbe4794ull,
	0x4bc1b3f0968dd39cull, 0xbb6dc91da77961bdull,
	0xbed65cf21aa2ee98ull, 0xd0f2cbb02e3b67c7ull,
	0x93536795e3a33e88ull, 0xa80c038ccd5ccec8ull,
	0xb8ad50c6f649af94ull, 0xbce192de8a85b8eaull,
	0x17d835b85bbb15f3ull, 0x2f2e6163076bcfadull,
	0xde4daaaca71dc9a5ull, 0xa6a2506687956571ull,
	0xad87a3535c49ef28ull, 0x32d892fad841c342ull,
	0x7127512f72f27cceull, 0xa7f32346f95978e3ull,
	0x12e0b01abb051238ull, 0x15e034d40fa197aeull,
	0x314dffbe0815a3b4ull, 0x027990f029623981ull,
	0xcadcd4e59ef40c4dull, 0x9abfd8766a33735cull,
	0x0e3ea96b5304a7d0ull, 0xad0c42d6fc585992ull,
	0x187306c89bc215a9ull, 0xd4a60abcf3792b95ull,
	0xf935451de4f21df2ull, 0xa9538f0419755787ull,
	0xdb9acddff56ca510ull, 0xd06c98cd5c0975ebull,
	0xe612a3cb9ecba951ull, 0xc766e62cfcadaf96ull,
	0xee64435a9752fe72ull, 0xa192d576b245165aull,
	0x0a8787bf8ecb74b2ull, 0x81b3e73d20b49b6full,
	0x7fa8220ba3b2eceaull, 0x245731c13ca42499ull,
	0xb78dbfaf3a8d83bdull, 0xea1ad565322a1a0bull,
	0x60e61c23a3795013ull, 0x6606d7e446282b93ull,
	0x6ca4ecb15c5f91e1ull, 0x9f626da15c9625f3ull,
	0xe51b38608ef25f57ull, 0x958a324ceb064572ull
};

static void utest_siphash(void)
{
	size_t i;
	uint8_t key[16], msg[1024];
	uint64_t tst, res;

	for (i = 0; i < 16; i++) {
		key[i] = (uint8_t)i;
	}

	for (i = 0; i < 64; i++) {
		msg[i] = (uint8_t)i;
		tst = siphash_test_vectors[i];
		blgn_siphash64(key, msg, i, &res);
		assert_eq(tst, res);
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void utest_murmur3(void)
{
	const char *str;
	uint64_t res[2];

	str = "";
	blgn_murmur3(str, strlen(str), 0, res);
	assert_eq(res[0], 0);
	assert_eq(res[1], 0);

	blgn_murmur3(str, strlen(str), 1, res);
	assert_eq(res[0], 0xb55cff6ee5ab1046);
	assert_eq(res[1], 0x8335f878aa2d6251);

	str = "The quick brown fox jumps over the lazy dog";
	blgn_murmur3(str, strlen(str), 0, res);
	assert_eq(res[0], 0x6c1b07bc7bbc4be3);
	assert_eq(res[1], 0x47939ac4a93c437a);

	str = "xxxx-xxxx-xxxx";
	blgn_murmur3(str, strlen(str), 0, res);
	assert_eq(res[0], 0xf5b9aa29d34d7a18);
	assert_eq(res[1], 0x5cba0c29e56c5d50);

	str = "Xxxx-xxxx-xxxx";
	blgn_murmur3(str, strlen(str), 0, res);
	assert_eq(res[0], 0x3ea6a12324911c13);
	assert_eq(res[1], 0xaa4b9732315fb2e1);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/*
 * Base on test vectors on Wikipedia's page:
 * http://en.wikipedia.org/wiki/CubeHash
 */
static void utest_cubehash(void)
{
	const char *str;
	uint64_t res[8];

	str = "Hello";
	blgn_cubehash512(str, strlen(str), res);
	assert_eq(res[0], 0x7ce309a25e2e1603);
	assert_eq(res[1], 0xca0fc369267b4d43);
	assert_eq(res[2], 0xf0b1b744ac45d621);
	assert_eq(res[3], 0x3ca08e7567566444);
	assert_eq(res[4], 0x8e2f62fdbf7bbd63);
	assert_eq(res[5], 0x7ce40fc293286d75);
	assert_eq(res[6], 0xb9d09e8dda31bd02);
	assert_eq(res[7], 0x9113e02ecccfd39b);

	str = "hello";
	blgn_cubehash512(str, strlen(str), res);
	assert_eq(res[0], 0x01ee7f4eb0e0ebfd);
	assert_eq(res[1], 0xb8bf77460f64993f);
	assert_eq(res[2], 0xaf13afce01b55b0d);
	assert_eq(res[3], 0x3d2a63690d25010f);
	assert_eq(res[4], 0x7127109455a7c143);
	assert_eq(res[5], 0xef12254183e762b1);
	assert_eq(res[6], 0x5575e0fcc49c79a0);
	assert_eq(res[7], 0x471a970ba8a66638);

	str = "The quick brown fox jumps over the lazy dog";
	blgn_cubehash512(str, strlen(str), res);
	assert_eq(res[0], 0xca942b088ed91037);
	assert_eq(res[1], 0x26af1fa87b4deb59);
	assert_eq(res[2], 0xe50cf3b5c6dcfbce);
	assert_eq(res[3], 0xbf5bba22fb39a6be);
	assert_eq(res[4], 0x9936c87bfdd7c52f);
	assert_eq(res[5], 0xc5e71700993958fa);
	assert_eq(res[6], 0x4e7b5e6e2a367212);
	assert_eq(res[7], 0x2475c40f9ec816ba);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int main(void)
{
	utest_adler32();
	utest_crc32c();
	utest_fnv32();
	utest_siphash();
	utest_murmur3();
	utest_cubehash();

	return EXIT_SUCCESS;
}
