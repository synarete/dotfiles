/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                                                             *
 *                 Balagan library -- Hash functions collection                *
 *                                                                             *
 *  Copyright (C) 2014 Synarete                                                *
 *                                                                             *
 *  Balagan is a free software; you can redistribute it and/or modify it under *
 *  the terms of the GNU Lesser General Public License as published by the     *
 *  Free Software Foundation; either version 3 of the License, or (at your     *
 *  option) any later version.                                                 *
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
/* http://131002.net/blake/blake256_light.c */

#include "balagan.h"


#define U8TO32(p)                   \
	(((uint32_t)((p)[0]) << 24) | ((uint32_t)((p)[1]) << 16) |    \
	 ((uint32_t)((p)[2]) <<  8) | ((uint32_t)((p)[3])      ))
#define U32TO8(p, v)                    \
	(p)[0] = (uint8_t)((v) >> 24); (p)[1] = (uint8_t)((v) >> 16); \
	(p)[2] = (uint8_t)((v) >>  8); (p)[3] = (uint8_t)((v)      );

struct blake_state {
	uint32_t h[8], s[4], t[2];
	size_t  buflen, nullt;
	uint8_t buf[64];
};

typedef struct blake_state  blake_state_t;


static const uint8_t
s_blake_sigma[][16] = {
	{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 },
	{14, 10, 4, 8, 9, 15, 13, 6, 1, 12, 0, 2, 11, 7, 5, 3 },
	{11, 8, 12, 0, 5, 2, 15, 13, 10, 14, 3, 6, 7, 1, 9, 4 },
	{ 7, 9, 3, 1, 13, 12, 11, 14, 2, 6, 5, 10, 4, 0, 15, 8 },
	{ 9, 0, 5, 7, 2, 4, 10, 15, 14, 1, 11, 12, 6, 8, 3, 13 },
	{ 2, 12, 6, 10, 0, 11, 8, 3, 4, 13, 7, 5, 15, 14, 1, 9 },
	{12, 5, 1, 15, 14, 13, 4, 10, 0, 7, 6, 3, 9, 2, 8, 11 },
	{13, 11, 7, 14, 12, 1, 3, 9, 5, 0, 15, 4, 8, 6, 2, 10 },
	{ 6, 15, 14, 9, 11, 3, 0, 8, 12, 2, 13, 7, 1, 4, 10, 5 },
	{10, 2, 8, 4, 7, 6, 1, 5, 15, 11, 9, 14, 3, 12, 13 , 0 },
	{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 },
	{14, 10, 4, 8, 9, 15, 13, 6, 1, 12, 0, 2, 11, 7, 5, 3 },
	{11, 8, 12, 0, 5, 2, 15, 13, 10, 14, 3, 6, 7, 1, 9, 4 },
	{ 7, 9, 3, 1, 13, 12, 11, 14, 2, 6, 5, 10, 4, 0, 15, 8 }
};

static const uint32_t
s_blake_cst[16] = {
	0x243F6A88, 0x85A308D3, 0x13198A2E, 0x03707344,
	0xA4093822, 0x299F31D0, 0x082EFA98, 0xEC4E6C89,
	0x452821E6, 0x38D01377, 0xBE5466CF, 0x34E90C6C,
	0xC0AC29B7, 0xC97C50DD, 0x3F84D5B5, 0xB5470917
};

static const uint8_t
s_blake_padding[] = {
	0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};


static void
blake256_compress(blake_state_t *s, const uint8_t *block)
{
	uint32_t v[16], m[16], i;
#define ROT(x,n) (((x)<<(32-n))|( (x)>>(n)))
#define G(a,b,c,d,e)                                       \
	v[a] += (m[s_blake_sigma[i][e]] ^                      \
	         s_blake_cst[s_blake_sigma[i][e+1]]) + v[b];   \
	v[d] = ROT( v[d] ^ v[a],16);                           \
	v[c] += v[d];                                          \
	v[b] = ROT( v[b] ^ v[c],12);                           \
	v[a] += (m[s_blake_sigma[i][e+1]] ^                    \
	         s_blake_cst[s_blake_sigma[i][e]])+v[b];       \
	v[d] = ROT( v[d] ^ v[a], 8);                           \
	v[c] += v[d];                                          \
	v[b] = ROT( v[b] ^ v[c], 7);

	for (i = 0; i < 16; ++i) {
		m[i] = U8TO32(block + i * 4);
	}
	for (i = 0; i < 8; ++i) {
		v[i] = s->h[i];
	}
	v[ 8] = s->s[0] ^ 0x243F6A88;
	v[ 9] = s->s[1] ^ 0x85A308D3;
	v[10] = s->s[2] ^ 0x13198A2E;
	v[11] = s->s[3] ^ 0x03707344;
	v[12] =  0xA4093822;
	v[13] =  0x299F31D0;
	v[14] =  0x082EFA98;
	v[15] =  0xEC4E6C89;
	if (s->nullt == 0) {
		v[12] ^= s->t[0];
		v[13] ^= s->t[0];
		v[14] ^= s->t[1];
		v[15] ^= s->t[1];
	}

	for (i = 0; i < 14; ++i) {
		G(0, 4, 8, 12, 0);
		G(1, 5, 9, 13, 2);
		G(2, 6, 10, 14, 4);
		G(3, 7, 11, 15, 6);
		G(3, 4, 9, 14, 14);
		G(2, 7, 8, 13, 12);
		G(0, 5, 10, 15, 8);
		G(1, 6, 11, 12, 10);
	}

	for (i = 0; i < 16; ++i) {
		s->h[i % 8] ^= v[i];
	}
	for (i = 0; i < 8 ; ++i) {
		s->h[i] ^= s->s[i % 4];
	}
}


static void
blake256_init(blake_state_t *s)
{
	s->h[0]     = 0x6A09E667;
	s->h[1]     = 0xBB67AE85;
	s->h[2]     = 0x3C6EF372;
	s->h[3]     = 0xA54FF53A;
	s->h[4]     = 0x510E527F;
	s->h[5]     = 0x9B05688C;
	s->h[6]     = 0x1F83D9AB;
	s->h[7]     = 0x5BE0CD19;
	s->t[0]     = 0;
	s->t[1]     = 0;
	s->buflen   = 0;
	s->nullt    = 0;
	s->s[0]     = 0;
	s->s[1]     = 0;
	s->s[2]     = 0;
	s->s[3]     = 0;
}


static void
blake256_update(blake_state_t *s, const uint8_t *data, size_t datalen)
{
	size_t left, fill;

	left = s->buflen >> 3;
	fill = 64 - left;

	if (left && (((datalen >> 3) & 0x3F) >= (unsigned)fill)) {
		memcpy((void *)(s->buf + left), (void *) data, fill);
		s->t[0] += 512;
		if (s->t[0] == 0) {
			s->t[1]++;
		}
		blake256_compress(s, s->buf);
		data += fill;
		datalen  -= (fill << 3);
		left = 0;
	}

	while (datalen >= 512) {
		s->t[0] += 512;
		if (s->t[0] == 0) {
			s->t[1]++;
		}
		blake256_compress(s, data);
		data += 64;
		datalen  -= 512;
	}

	if (datalen > 0) {
		memcpy((void *)(s->buf + left), (void *) data, datalen >> 3);
		s->buflen = (left << 3) + datalen;
	} else {
		s->buflen = 0;
	}
}


static void
blake256_final(blake_state_t *s, uint8_t *digest)
{
	uint8_t msglen[8], zo = 0x01, oo = 0x81;
	size_t lo = s->t[0] + s->buflen, hi = s->t[1];
	if (lo < (unsigned)s->buflen) {
		hi++;
	}
	U32TO8(msglen + 0, hi);
	U32TO8(msglen + 4, lo);

	if (s->buflen == 440) {   /* one padding byte */
		s->t[0] -= 8;
		blake256_update(s, &oo, 8);
	} else {
		if (s->buflen < 440) {   /* enough space to fill the block  */
			if (!s->buflen) {
				s->nullt = 1;
			}
			s->t[0] -= (uint32_t)(440 - s->buflen);
			blake256_update(s, s_blake_padding, 440 - s->buflen);
		} else { /* need 2 compressions */
			s->t[0] -= (uint32_t)(512 - s->buflen);
			blake256_update(s, s_blake_padding, 512 - s->buflen);
			s->t[0] -= 440;
			blake256_update(s, s_blake_padding + 1, 440);
			s->nullt = 1;
		}
		blake256_update(s, &zo, 8);
		s->t[0] -= 8;
	}
	s->t[0] -= 64;
	blake256_update(s, msglen, 64);

	U32TO8(digest + 0, s->h[0]);
	U32TO8(digest + 4, s->h[1]);
	U32TO8(digest + 8, s->h[2]);
	U32TO8(digest + 12, s->h[3]);
	U32TO8(digest + 16, s->h[4]);
	U32TO8(digest + 20, s->h[5]);
	U32TO8(digest + 24, s->h[6]);
	U32TO8(digest + 28, s->h[7]);
}


static void
blake256_hash(uint8_t *out, const uint8_t *in, size_t inlen)
{
	blake_state_t s;

	blake256_init(&s);
	blake256_update(&s, in, inlen * 8);
	blake256_final(&s, out);
}


void blgn_blake128(const void *in, size_t len, uint64_t rc[2])
{
	uint64_t digest[4];

	blake256_hash((uint8_t *)digest, in, len);

	rc[0] = digest[0] ^ digest[2];
	rc[1] = digest[1] ^ digest[3];
}


