/*
 * Argon2d reference source code - reference C implementation
 * Retrieved from  <https://github.com/P-H-C/phc-winner-argon2>
 *
 * Copyright (C) 2015:
 * - Daniel Dinu
 * - Dmitry Khovratovich
 * - Jean-Philippe Aumasson
 * - Samuel Neves
 *
 * You may use this work under the terms of a Creative Commons CC0 1.0
 * License/Waiver or the Apache Public License 2.0, at your option.
 *
 * The terms of these licenses can be found at:
 * - CC0 1.0 Universal  <http://creativecommons.org/publicdomain/zero/1.0>
 * - Apache 2.0         <http://www.apache.org/licenses/LICENSE-2.0>
 *
 * Heavily modified for use as a password hashing module in
 * Atheme IRC Services  <https://github.com/atheme/atheme>
 * by Aaron M. D. Jones  <aaronmdjones@gmail.com>  (2017)
 */

#include "atheme.h"

#define ARGON2D_MEMCOST_MIN     8
#define ARGON2D_MEMCOST_DEF     14
#define ARGON2D_MEMCOST_MAX     20

#define ARGON2D_TIMECOST_MIN    4
#define ARGON2D_TIMECOST_DEF    32
#define ARGON2D_TIMECOST_MAX    16384

#define ATHEME_ARGON2D_LOADB64  "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"

// Format strings for (s)scanf(3)
#define ATHEME_ARGON2D_LOADSALT "$argon2d$v=19$m=%" SCNu32 ",t=%" SCNu32 ",p=1$%[" ATHEME_ARGON2D_LOADB64 "]$"
#define ATHEME_ARGON2D_LOADHASH ATHEME_ARGON2D_LOADSALT "%[" ATHEME_ARGON2D_LOADB64 "]"

// Format strings for (sn)printf(3)
#define ATHEME_ARGON2D_SAVESALT "$argon2d$v=19$m=%" PRIu32 ",t=%" PRIu32 ",p=1$%s$"
#define ATHEME_ARGON2D_SAVEHASH ATHEME_ARGON2D_SAVESALT "%s"

#define ATHEME_ARGON2D_AUTHLEN  0x00
#define ATHEME_ARGON2D_HASHLEN  0x40
#define ATHEME_ARGON2D_LANECNT  0x01
#define ATHEME_ARGON2D_PRIVLEN  0x00
#define ATHEME_ARGON2D_SALTLEN  0x20
#define ATHEME_ARGON2D_TYPEVAL  0x00
#define ATHEME_ARGON2D_VERSION  0x13

#define ARGON2_BLKSZ            0x400
#define ARGON2_PREHASH_LEN      0x40
#define ARGON2_PRESEED_LEN      0x48
#define ARGON2_BLK_QWORDS       (ARGON2_BLKSZ / 0x08)
#define ARGON2_SYNC_POINTS      0x04

#define BLAKE2B_BLOCKLEN        0x80
#define BLAKE2B_HASHLEN         0x40
#define BLAKE2B_HASHLEN_HALF    0x20
#define BLAKE2B_PERSLEN         0x10
#define BLAKE2B_SALTLEN         0x10

#pragma pack(push, 1)

struct blake2b_param
{
	uint8_t                 hash_len;
	uint8_t                 key_len;
	uint8_t                 fanout;
	uint8_t                 depth;
	uint32_t                leaf_len;
	uint64_t                node_off;
	uint8_t                 node_depth;
	uint8_t                 inner_len;
	uint8_t                 reserved[0x0E];
	uint8_t                 salt[BLAKE2B_SALTLEN];
	uint8_t                 personal[BLAKE2B_PERSLEN];
} __attribute__((packed));

#pragma pack(pop)

struct blake2b_state
{
	uint64_t                h[0x08];
	uint64_t                t[0x02];
	uint64_t                f[0x02];
	size_t                  buflen;
	size_t                  outlen;
	uint8_t                 last_node;
	uint8_t                 buf[BLAKE2B_BLOCKLEN];
};

struct argon2d_block
{
	uint64_t                v[ARGON2_BLK_QWORDS];
};

struct argon2d_context
{
	const uint8_t          *pass;
	uint8_t                 salt[ATHEME_ARGON2D_SALTLEN];
	uint8_t                 hash[ATHEME_ARGON2D_HASHLEN];
	uint32_t                passlen;
	uint32_t                m_cost;
	uint32_t                t_cost;
	uint32_t                lane_len;
	uint32_t                seg_len;
	uint32_t                index;
};

enum BLAKE2B_SZCHK_STATIC_ASSERT
{
	// Ensure `struct blake2b_param' has been properly packed & padded (a poor man's `static_assert')
	// This will generate a compile-time division-by-zero error if this is not the case
	// This is critical to the correct functioning of blake2b_init() below
	// INTEROPERABILITY WITH OTHER ARGON2D IMPLMEMENTATIONS MAY BE JEOPARDISED IF THIS IS REMOVED
	BLAKE2B_SZCHK_0 = (0x01 / !!(CHAR_BIT == 0x08)),
	BLAKE2B_SZCHK_1 = (0x01 / !!(sizeof(struct blake2b_param) == (sizeof(uint64_t) * CHAR_BIT)))
};

static const uint64_t blake2b_iv[0x08] = {

	UINT64_C(0x6A09E667F3BCC908), UINT64_C(0xBB67AE8584CAA73B), UINT64_C(0x3C6EF372FE94F82B),
	UINT64_C(0xA54FF53A5F1D36F1), UINT64_C(0x510E527FADE682D1), UINT64_C(0x9B05688C2B3E6C1F),
	UINT64_C(0x1F83D9ABFB41BD6B), UINT64_C(0x5BE0CD19137E2179)
};

static const uint64_t blake2b_sigma[0x0C][0x10] = {

	{ 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F },
	{ 0x0E, 0x0A, 0x04, 0x08, 0x09, 0x0F, 0x0D, 0x06, 0x01, 0x0C, 0x00, 0x02, 0x0B, 0x07, 0x05, 0x03 },
	{ 0x0B, 0x08, 0x0C, 0x00, 0x05, 0x02, 0x0F, 0x0D, 0x0A, 0x0E, 0x03, 0x06, 0x07, 0x01, 0x09, 0x04 },
	{ 0x07, 0x09, 0x03, 0x01, 0x0D, 0x0C, 0x0B, 0x0E, 0x02, 0x06, 0x05, 0x0A, 0x04, 0x00, 0x0F, 0x08 },
	{ 0x09, 0x00, 0x05, 0x07, 0x02, 0x04, 0x0A, 0x0F, 0x0E, 0x01, 0x0B, 0x0C, 0x06, 0x08, 0x03, 0x0D },
	{ 0x02, 0x0C, 0x06, 0x0A, 0x00, 0x0B, 0x08, 0x03, 0x04, 0x0D, 0x07, 0x05, 0x0F, 0x0E, 0x01, 0x09 },
	{ 0x0C, 0x05, 0x01, 0x0F, 0x0E, 0x0D, 0x04, 0x0A, 0x00, 0x07, 0x06, 0x03, 0x09, 0x02, 0x08, 0x0B },
	{ 0x0D, 0x0B, 0x07, 0x0E, 0x0C, 0x01, 0x03, 0x09, 0x05, 0x00, 0x0F, 0x04, 0x08, 0x06, 0x02, 0x0A },
	{ 0x06, 0x0F, 0x0E, 0x09, 0x0B, 0x03, 0x00, 0x08, 0x0C, 0x02, 0x0D, 0x07, 0x01, 0x04, 0x0A, 0x05 },
	{ 0x0A, 0x02, 0x08, 0x04, 0x07, 0x06, 0x01, 0x05, 0x0F, 0x0B, 0x09, 0x0E, 0x03, 0x0C, 0x0D, 0x00 },
	{ 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F },
	{ 0x0E, 0x0A, 0x04, 0x08, 0x09, 0x0F, 0x0D, 0x06, 0x01, 0x0C, 0x00, 0x02, 0x0B, 0x07, 0x05, 0x03 }
};

/*
 * This is reallocated on-demand to save allocating and freeing every time we
 * digest a password. The mempoolsz variable tracks how large (in blocks) the
 * currently-allocated memory pool is.
 */
static struct argon2d_block *argon2d_mempool = NULL;
static uint32_t argon2d_mempoolsz = 0;

static inline bool __attribute__((warn_unused_result))
atheme_argon2d_mempool_realloc(const uint32_t mem_blocks)
{
	if (argon2d_mempool != NULL && argon2d_mempoolsz >= mem_blocks)
		return true;

	struct argon2d_block *mempool_tmp;
	const size_t required_sz = mem_blocks * sizeof(struct argon2d_block);

	if (!(mempool_tmp = realloc(argon2d_mempool, required_sz)))
	{
		(void) slog(LG_ERROR, "%s: memory allocation failure", __func__);
		return false;
	}

	argon2d_mempool = mempool_tmp;
	argon2d_mempoolsz = mem_blocks;
	return true;
}

#if (defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)) || \
    defined(__LITTLE_ENDIAN__) || defined(__ARMEL__) || defined(__MIPSEL__) || defined(__AARCH64EL__) || \
    defined(__amd64__) || defined(__i386__) || defined(_M_IX86) || defined(_M_X64) || defined(_M_AMD64)

#define NATIVE_LITTLE_ENDIAN
#endif

static inline void
blake2b_store32(uint8_t *const restrict dst, const uint32_t w)
{
#if defined(NATIVE_LITTLE_ENDIAN)
	(void) memcpy(dst, &w, sizeof w);
#else
	for (size_t x = 0x00; x < 0x04; x++)
		dst[x] = (uint8_t)(w >> (x * 0x08));
#endif
}

static inline void
blake2b_store64(uint8_t *const restrict dst, const uint64_t w)
{
#if defined(NATIVE_LITTLE_ENDIAN)
	(void) memcpy(dst, &w, sizeof w);
#else
	for (size_t x = 0x00; x < 0x08; x++)
		dst[x] = (uint8_t)(w >> (x * 0x08));
#endif
}

static inline uint64_t
blake2b_load64(const uint8_t *const restrict src)
{
	uint64_t w = 0;
#if defined(NATIVE_LITTLE_ENDIAN)
	(void) memcpy(&w, src, sizeof w);
#else
	for (size_t x = 0x00; x < 0x08; x++)
		w |= (((uint64_t) src[x]) << (x * 0x08));
#endif
	return w;
}

static inline uint64_t
blake2b_rotr64(const uint64_t w, const unsigned c)
{
	return ((w >> c) | (w << (0x40 - c)));
}

static inline uint64_t
blake2b_fBlaMka(const uint64_t x, const uint64_t y)
{
	const uint64_t m = UINT64_C(0xFFFFFFFF);
	const uint64_t v = ((x & m) * (y & m));
	return (x + y + (v * 0x02));
}

static inline void
blake2b_set_lastblock(struct blake2b_state *const restrict state)
{
	state->f[0x00] = (uint64_t) -1;

	if (state->last_node)
		state->f[0x01] = (uint64_t) -1;
}

static inline void
blake2b_counter_inc(struct blake2b_state *const restrict state, const uint64_t inc)
{
	state->t[0x00] += inc;
	state->t[0x01] += (state->t[0x00] < inc);
}

static inline void
blake2b_init(struct blake2b_state *const restrict state, const size_t outlen)
{
	struct blake2b_param params;
	const uint8_t *const params_byte = (const uint8_t *) &params;

	(void) memset(&params, 0x00, sizeof params);
	(void) memset(state, 0x00, sizeof *state);
	(void) memcpy(state->h, blake2b_iv, sizeof blake2b_iv);

	params.depth = 0x01;
	params.fanout = 0x01;
	params.hash_len = (uint8_t) outlen;
	state->outlen = outlen;

	for (size_t x = 0x00; x < 0x08; x++)
		state->h[x] ^= blake2b_load64(&params_byte[(x * sizeof state->h[x])]);
}

static void
blake2b_compress(struct blake2b_state *const restrict state, const uint8_t *const restrict block)
{
	uint64_t m[0x10];
	uint64_t v[0x10];

	for (size_t x = 0x00; x < 0x10; x++)
		m[x] = blake2b_load64(block + (x * sizeof m[x]));

	for (size_t x = 0x00; x < 0x08; x++)
	{
		v[x] = state->h[x];
		v[x + 0x08] = blake2b_iv[x];
	}
	for (size_t x = 0x00; x < 0x02; x++)
	{
		v[x + 0x0C] ^= state->t[x];
		v[x + 0x0E] ^= state->f[x];
	}

#define G(r, i, a, b, c, d) do {                                      \
	    (a) = (a) + (b) + m[blake2b_sigma[r][(i * 0x02) + 0x00]]; \
	    (d) = blake2b_rotr64(((d) ^ (a)), 0x20); (c) += (d);      \
	    (b) = blake2b_rotr64(((b) ^ (c)), 0x18);                  \
	    (a) = (a) + (b) + m[blake2b_sigma[r][(i * 0x02) + 0x01]]; \
	    (d) = blake2b_rotr64(((d) ^ (a)), 0x10); (c) += (d);      \
	    (b) = blake2b_rotr64(((b) ^ (c)), 0x3F);                  \
	} while (0)

	for (size_t x = 0x00; x < 0x0C; x++)
	{
		G(x, 0x00, v[0x00], v[0x04], v[0x08], v[0x0C]);
		G(x, 0x01, v[0x01], v[0x05], v[0x09], v[0x0D]);
		G(x, 0x02, v[0x02], v[0x06], v[0x0A], v[0x0E]);
		G(x, 0x03, v[0x03], v[0x07], v[0x0B], v[0x0F]);
		G(x, 0x04, v[0x00], v[0x05], v[0x0A], v[0x0F]);
		G(x, 0x05, v[0x01], v[0x06], v[0x0B], v[0x0C]);
		G(x, 0x06, v[0x02], v[0x07], v[0x08], v[0x0D]);
		G(x, 0x07, v[0x03], v[0x04], v[0x09], v[0x0E]);
	}

#undef G

	for (size_t x = 0x00; x < 0x08; x++)
		state->h[x] ^= (v[x] ^ v[x + 0x08]);
}

static bool __attribute__((warn_unused_result))
blake2b_update(struct blake2b_state *const restrict state, const uint8_t *restrict in, size_t inlen)
{
	if (!inlen)
		return true;

	if (state->f[0x00] != 0x00)
		return false;

	if ((state->buflen + inlen) > BLAKE2B_BLOCKLEN)
	{
		const size_t left = state->buflen;
		const size_t fill = BLAKE2B_BLOCKLEN - left;

		(void) memcpy(&state->buf[left], in, fill);
		(void) blake2b_counter_inc(state, BLAKE2B_BLOCKLEN);
		(void) blake2b_compress(state, state->buf);

		in += fill;
		inlen -= fill;
		state->buflen = 0x00;

		while (inlen > BLAKE2B_BLOCKLEN)
		{
			(void) blake2b_counter_inc(state, BLAKE2B_BLOCKLEN);
			(void) blake2b_compress(state, in);

			in += BLAKE2B_BLOCKLEN;
			inlen -= BLAKE2B_BLOCKLEN;
		}
	}

	(void) memcpy(&state->buf[state->buflen], in, inlen);
	state->buflen += inlen;
	return true;
}

static inline bool __attribute__((warn_unused_result))
blake2b_final(struct blake2b_state *const restrict state, uint8_t *const restrict out)
{
	if (state->f[0x00] != 0x00)
		return false;

	(void) blake2b_counter_inc(state, state->buflen);
	(void) blake2b_set_lastblock(state);
	(void) memset(&state->buf[state->buflen], 0x00, (BLAKE2B_BLOCKLEN - state->buflen));
	(void) blake2b_compress(state, state->buf);

	uint8_t buffer[BLAKE2B_HASHLEN];
	(void) memset(buffer, 0x00, sizeof buffer);

	for (size_t x = 0x00; x < 0x08; x++)
		(void) blake2b_store64(&buffer[(x * sizeof state->h[x])], state->h[x]);

	(void) memcpy(out, buffer, state->outlen);
	return true;
}

static inline bool __attribute__((warn_unused_result))
blake2b_full(const uint8_t *const restrict in, const size_t inlen, uint8_t *const restrict out, const size_t outlen)
{
	struct blake2b_state state;
	(void) blake2b_init(&state, outlen);

	if (!blake2b_update(&state, in, inlen))
		return false;

	return blake2b_final(&state, out);
}

static bool __attribute__((warn_unused_result))
blake2b_long(const uint8_t *const restrict in, const size_t inlen, uint8_t *restrict out, const size_t outlen)
{
	uint8_t outlen_buf[4] = { 0x00, 0x00, 0x00, 0x00 };
	struct blake2b_state blake_state;

	if ((sizeof(outlen) > sizeof(uint32_t)) && (outlen > UINT32_MAX))
		return false;

	(void) blake2b_store32(outlen_buf, (uint32_t) outlen);

	if (outlen <= BLAKE2B_HASHLEN)
	{
		(void) blake2b_init(&blake_state, outlen);

		if (!blake2b_update(&blake_state, outlen_buf, sizeof outlen_buf))
			return false;
		if (!blake2b_update(&blake_state, in, inlen))
			return false;
		if (!blake2b_final(&blake_state, out))
			return false;

		return true;
	}

	uint8_t ibuf[BLAKE2B_HASHLEN];
	uint8_t obuf[BLAKE2B_HASHLEN];

	(void) blake2b_init(&blake_state, BLAKE2B_HASHLEN);

	if (!blake2b_update(&blake_state, outlen_buf, sizeof outlen_buf))
		return false;
	if (!blake2b_update(&blake_state, in, inlen))
		return false;
	if (!blake2b_final(&blake_state, obuf))
		return false;

	(void) memcpy(out, obuf, BLAKE2B_HASHLEN_HALF);
	out += BLAKE2B_HASHLEN_HALF;

	uint32_t remain = (((uint32_t) outlen) - BLAKE2B_HASHLEN_HALF);

	while (remain > BLAKE2B_HASHLEN)
	{
		(void) memcpy(ibuf, obuf, BLAKE2B_HASHLEN);

		if (!blake2b_full(ibuf, BLAKE2B_HASHLEN, obuf, BLAKE2B_HASHLEN))
			return false;

		(void) memcpy(out, obuf, BLAKE2B_HASHLEN_HALF);

		out += BLAKE2B_HASHLEN_HALF;
		remain -= BLAKE2B_HASHLEN_HALF;
	}

	(void) memcpy(ibuf, obuf, BLAKE2B_HASHLEN);

	if (!blake2b_full(ibuf, BLAKE2B_HASHLEN, obuf, remain))
		return false;

	(void) memcpy(out, obuf, remain);
	return true;
}

static inline void
argon2d_copy_block(struct argon2d_block *const restrict dst, const struct argon2d_block *const restrict src)
{
	(void) memcpy(dst->v, src->v, (0x08 * ARGON2_BLK_QWORDS));
}

static inline void
argon2d_xor_block(struct argon2d_block *const restrict dst, const struct argon2d_block *const restrict src)
{
	for (size_t x = 0x00; x < ARGON2_BLK_QWORDS; x++)
		dst->v[x] ^= src->v[x];
}

static inline void
argon2d_load_block(struct argon2d_block *const restrict dst, const uint8_t *const restrict input)
{
	for (size_t x = 0x00; x < ARGON2_BLK_QWORDS; x++)
		dst->v[x] = blake2b_load64(&input[(x * sizeof dst->v[x])]);
}

static inline void
argon2d_store_block(uint8_t *const restrict output, const struct argon2d_block *const restrict src)
{
	for (size_t x = 0x00; x < ARGON2_BLK_QWORDS; x++)
		(void) blake2b_store64(&output[(x * sizeof src->v[x])], src->v[x]);
}

static inline bool __attribute__((warn_unused_result))
argon2d_hash_init(struct argon2d_context *const restrict ctx, uint8_t *const restrict bhash)
{
	struct blake2b_state state;
	uint8_t value[4];

	(void) blake2b_init(&state, ARGON2_PREHASH_LEN);
	(void) blake2b_store32(value, ATHEME_ARGON2D_LANECNT);

	if (!blake2b_update(&state, value, sizeof value))
		return false;

	(void) blake2b_store32(value, ATHEME_ARGON2D_HASHLEN);

	if (!blake2b_update(&state, value, sizeof value))
		return false;

	(void) blake2b_store32(value, ctx->m_cost);

	if (!blake2b_update(&state, value, sizeof value))
		return false;

	(void) blake2b_store32(value, ctx->t_cost);

	if (!blake2b_update(&state, value, sizeof value))
		return false;

	(void) blake2b_store32(value, ATHEME_ARGON2D_VERSION);

	if (!blake2b_update(&state, value, sizeof value))
		return false;

	(void) blake2b_store32(value, ATHEME_ARGON2D_TYPEVAL);

	if (!blake2b_update(&state, value, sizeof value))
		return false;

	(void) blake2b_store32(value, ctx->passlen);

	if (!blake2b_update(&state, value, sizeof value))
		return false;

	if (!blake2b_update(&state, ctx->pass, ctx->passlen))
		return false;

	(void) blake2b_store32(value, ATHEME_ARGON2D_SALTLEN);

	if (!blake2b_update(&state, value, sizeof value))
		return false;

	if (!blake2b_update(&state, ctx->salt, ATHEME_ARGON2D_SALTLEN))
		return false;

	(void) blake2b_store32(value, ATHEME_ARGON2D_PRIVLEN);

	if (!blake2b_update(&state, value, sizeof value))
		return false;

	(void) blake2b_store32(value, ATHEME_ARGON2D_AUTHLEN);

	if (!blake2b_update(&state, value, sizeof value))
		return false;

	return blake2b_final(&state, bhash);
}

static uint32_t
argon2d_idx(const struct argon2d_context *const restrict ctx, const uint32_t pass, const uint8_t slice,
            const uint64_t rand_p)
{
	uint32_t ra_size = (ctx->index - 0x01);

	if (pass)
		ra_size += (ctx->lane_len - ctx->seg_len);
	else if (slice)
		ra_size += (slice * ctx->seg_len);

	uint64_t relative_pos = (uint64_t)(rand_p & 0xFFFFFFFF);
	relative_pos = ((relative_pos * relative_pos) >> 0x20U);
	relative_pos = (ra_size - 0x01 - ((ra_size * relative_pos) >> 0x20U));

	uint32_t start_pos = 0x00;
	if (pass && slice != (ARGON2_SYNC_POINTS - 0x01))
		start_pos = ((slice + 0x01) * ctx->seg_len);

	return ((start_pos + ((uint32_t) relative_pos)) % ctx->lane_len);
}

static void
argon2d_fill_block(const struct argon2d_block *const prev, const struct argon2d_block *const ref,
                   struct argon2d_block *const next, const uint32_t pass)
{
	struct argon2d_block block_r;
	struct argon2d_block block_x;

	(void) argon2d_copy_block(&block_r, ref);
	(void) argon2d_xor_block(&block_r, prev);
	(void) argon2d_copy_block(&block_x, &block_r);

	if (pass != 0x00)
		(void) argon2d_xor_block(&block_x, next);

	uint64_t *const v = block_r.v;

#define F(a, b, c, d) do {                              \
	    (a) = blake2b_fBlaMka((a), (b));            \
	    (d) = blake2b_rotr64(((d) ^ (a)), 0x20);    \
	    (c) = blake2b_fBlaMka((c), (d));            \
	    (b) = blake2b_rotr64(((b) ^ (c)), 0x18);    \
	    (a) = blake2b_fBlaMka((a), (b));            \
	    (d) = blake2b_rotr64(((d) ^ (a)), 0x10);    \
	    (c) = blake2b_fBlaMka((c), (d));            \
	    (b) = blake2b_rotr64(((b) ^ (c)), 0x3F);    \
	} while (0)

	for (size_t x = 0x00; x < 0x08; x++)
	{
		F(v[(0x10 * x) + 0x00], v[(0x10 * x) + 0x04], v[(0x10 * x) + 0x08], v[(0x10 * x) + 0x0C]);
		F(v[(0x10 * x) + 0x01], v[(0x10 * x) + 0x05], v[(0x10 * x) + 0x09], v[(0x10 * x) + 0x0D]);
		F(v[(0x10 * x) + 0x02], v[(0x10 * x) + 0x06], v[(0x10 * x) + 0x0A], v[(0x10 * x) + 0x0E]);
		F(v[(0x10 * x) + 0x03], v[(0x10 * x) + 0x07], v[(0x10 * x) + 0x0B], v[(0x10 * x) + 0x0F]);
		F(v[(0x10 * x) + 0x00], v[(0x10 * x) + 0x05], v[(0x10 * x) + 0x0A], v[(0x10 * x) + 0x0F]);
		F(v[(0x10 * x) + 0x01], v[(0x10 * x) + 0x06], v[(0x10 * x) + 0x0B], v[(0x10 * x) + 0x0C]);
		F(v[(0x10 * x) + 0x02], v[(0x10 * x) + 0x07], v[(0x10 * x) + 0x08], v[(0x10 * x) + 0x0D]);
		F(v[(0x10 * x) + 0x03], v[(0x10 * x) + 0x04], v[(0x10 * x) + 0x09], v[(0x10 * x) + 0x0E]);
	}
	for (size_t x = 0x00; x < 0x08; x++)
	{
		F(v[(0x02 * x) + 0x00], v[(0x02 * x) + 0x20], v[(0x02 * x) + 0x40], v[(0x02 * x) + 0x60]);
		F(v[(0x02 * x) + 0x01], v[(0x02 * x) + 0x21], v[(0x02 * x) + 0x41], v[(0x02 * x) + 0x61]);
		F(v[(0x02 * x) + 0x10], v[(0x02 * x) + 0x30], v[(0x02 * x) + 0x50], v[(0x02 * x) + 0x70]);
		F(v[(0x02 * x) + 0x11], v[(0x02 * x) + 0x31], v[(0x02 * x) + 0x51], v[(0x02 * x) + 0x71]);
		F(v[(0x02 * x) + 0x00], v[(0x02 * x) + 0x21], v[(0x02 * x) + 0x50], v[(0x02 * x) + 0x71]);
		F(v[(0x02 * x) + 0x01], v[(0x02 * x) + 0x30], v[(0x02 * x) + 0x51], v[(0x02 * x) + 0x60]);
		F(v[(0x02 * x) + 0x10], v[(0x02 * x) + 0x31], v[(0x02 * x) + 0x40], v[(0x02 * x) + 0x61]);
		F(v[(0x02 * x) + 0x11], v[(0x02 * x) + 0x20], v[(0x02 * x) + 0x41], v[(0x02 * x) + 0x70]);
	}

#undef F

	(void) argon2d_copy_block(next, &block_x);
	(void) argon2d_xor_block(next, &block_r);
}

static inline void
argon2d_segment_fill(struct argon2d_context *const restrict ctx, const uint32_t pass, const uint8_t slice)
{
	const uint32_t start_idx = ((!pass && !slice) ? 0x02 : 0x00);
	uint32_t cur_off = (slice * ctx->seg_len) + start_idx;
	uint32_t prv_off = (cur_off - 0x01);

	if ((cur_off % ctx->lane_len) == 0x00)
		prv_off += ctx->lane_len;

	for (uint32_t i = start_idx; i < ctx->seg_len; i++, cur_off++, prv_off++)
	{
		if ((cur_off % ctx->lane_len) == 0x01)
			prv_off = (cur_off - 0x01);

		ctx->index = i;

		const uint64_t rand_p = argon2d_mempool[prv_off].v[0x00];
		const uint32_t ref_idx = argon2d_idx(ctx, pass, slice, rand_p);

		const struct argon2d_block *const prv = &argon2d_mempool[prv_off];
		const struct argon2d_block *const ref = &argon2d_mempool[ref_idx];
		struct argon2d_block *const cur = &argon2d_mempool[cur_off];

		(void) argon2d_fill_block(prv, ref, cur, pass);
	}
}

static bool __attribute__((warn_unused_result))
argon2d_hash_raw(struct argon2d_context *const restrict ctx)
{
	ctx->seg_len = (ctx->m_cost / ARGON2_SYNC_POINTS);

	const uint32_t mem_blocks = (ctx->seg_len * ARGON2_SYNC_POINTS);

	ctx->lane_len = mem_blocks;

	if (!atheme_argon2d_mempool_realloc(mem_blocks))
		return false;

	uint8_t bhash_init[ARGON2_PRESEED_LEN];

	if (!argon2d_hash_init(ctx, bhash_init))
		return false;

	(void) blake2b_store32(bhash_init + ARGON2_PREHASH_LEN, 0x00);
	(void) blake2b_store32(bhash_init + ARGON2_PREHASH_LEN + 0x04, 0x00);

	uint8_t bhash_bytes[ARGON2_BLKSZ];

	if (!blake2b_long(bhash_init, ARGON2_PRESEED_LEN, bhash_bytes, ARGON2_BLKSZ))
		return false;

	(void) argon2d_load_block(&argon2d_mempool[0x00], bhash_bytes);
	(void) blake2b_store32(bhash_init + ARGON2_PREHASH_LEN, 0x01);

	if (!blake2b_long(bhash_init, ARGON2_PRESEED_LEN, bhash_bytes, ARGON2_BLKSZ))
		return false;

	(void) argon2d_load_block(&argon2d_mempool[0x01], bhash_bytes);

	for (uint32_t pass = 0x00; pass < ctx->t_cost; pass++)
	{
		for (uint8_t slice = 0x00; slice < ARGON2_SYNC_POINTS; slice++)
		{
			ctx->index = 0x00;
			(void) argon2d_segment_fill(ctx, pass, slice);
		}
	}

	struct argon2d_block bhash_final;
	(void) argon2d_copy_block(&bhash_final, &argon2d_mempool[ctx->lane_len - 0x01]);
	(void) argon2d_store_block(bhash_bytes, &bhash_final);

	return blake2b_long(bhash_bytes, ARGON2_BLKSZ, ctx->hash, ATHEME_ARGON2D_HASHLEN);
}



/*
 * The default memory and time cost variables
 * These can be adjusted in the configuration file
 */
static unsigned int atheme_argon2d_mcost = ARGON2D_MEMCOST_DEF;
static unsigned int atheme_argon2d_tcost = ARGON2D_TIMECOST_DEF;

static const char *
atheme_argon2d_crypt(const char *const restrict password,
                     const char __attribute__((unused)) *const restrict parameters)
{
	struct argon2d_context ctx;
	(void) memset(&ctx, 0x00, sizeof ctx);
	(void) arc4random_buf(ctx.salt, sizeof ctx.salt);

	ctx.pass = (const uint8_t *) password;
	ctx.passlen = (uint32_t) strlen(password);
	ctx.m_cost = (uint32_t) (0x01U << atheme_argon2d_mcost);
	ctx.t_cost = (uint32_t) atheme_argon2d_tcost;

	if (! argon2d_hash_raw(&ctx))
		return NULL;

	char salt_b64[0x1000];
	if (base64_encode_raw(ctx.salt, sizeof ctx.salt, salt_b64, sizeof salt_b64) == (size_t) -1)
		return NULL;

	char hash_b64[0x1000];
	if (base64_encode_raw(ctx.hash, sizeof ctx.hash, hash_b64, sizeof hash_b64) == (size_t) -1)
		return NULL;

	static char res[PASSLEN + 1];
	if (snprintf(res, sizeof res, ATHEME_ARGON2D_SAVEHASH, ctx.m_cost, ctx.t_cost, salt_b64, hash_b64) > PASSLEN)
		return NULL;

	return res;
}

static bool
atheme_argon2d_recrypt(const struct argon2d_context *const restrict ctx)
{
	const uint32_t m_cost_def = (uint32_t) (0x01U << atheme_argon2d_mcost);
	const uint32_t t_cost_def = (uint32_t) atheme_argon2d_tcost;

	if (ctx->m_cost != m_cost_def)
	{
		(void) slog(LG_DEBUG, "%s: memory cost (%" PRIu32 ") != default (%" PRIu32 ")", __func__,
		                      ctx->m_cost, m_cost_def);
		return true;
	}

	if (ctx->t_cost != t_cost_def)
	{
		(void) slog(LG_DEBUG, "%s: time cost (%" PRIu32 ") != default (%" PRIu32 ")", __func__,
		                      ctx->t_cost, t_cost_def);
		return true;
	}

	return false;
}

static bool
atheme_argon2d_verify(const char *const restrict password, const char *const restrict parameters,
                      unsigned int *const restrict flags)
{
	struct argon2d_context ctx;
	(void) memset(&ctx, 0x00, sizeof ctx);

	char salt_b64[0x1000];
	char hash_b64[0x1000];
	uint8_t dec_hash[ATHEME_ARGON2D_HASHLEN];

	if (sscanf(parameters, ATHEME_ARGON2D_LOADHASH, &ctx.m_cost, &ctx.t_cost, salt_b64, hash_b64) != 4)
		return false;

	if (ctx.m_cost < (0x01U << ARGON2D_MEMCOST_MIN) || ctx.m_cost > (0x01U << ARGON2D_MEMCOST_MAX))
		return false;

	if (ctx.t_cost < ARGON2D_TIMECOST_MIN || ctx.t_cost > ARGON2D_TIMECOST_MAX)
		return false;

	if (base64_decode(salt_b64, ctx.salt, sizeof ctx.salt) != sizeof ctx.salt)
		return false;

	if (base64_decode(hash_b64, dec_hash, sizeof dec_hash) != sizeof dec_hash)
		return false;

	*flags |= PWVERIFY_FLAG_MYMODULE;

	ctx.pass = (const uint8_t *) password;
	ctx.passlen = (uint32_t) strlen(password);

	if (! argon2d_hash_raw(&ctx))
		return false;

	if (memcmp(ctx.hash, dec_hash, ATHEME_ARGON2D_HASHLEN) != 0)
		return false;

	if (atheme_argon2d_recrypt(&ctx))
		*flags |= PWVERIFY_FLAG_RECRYPT;

	return true;
}

static crypt_impl_t crypto_argon2d_impl = {

	.id         = "argon2d",
	.crypt      = &atheme_argon2d_crypt,
	.verify     = &atheme_argon2d_verify,
};

static mowgli_list_t atheme_argon2d_conf_table;

static void
mod_init(module_t __attribute__((unused)) *const restrict m)
{
	(void) crypt_register(&crypto_argon2d_impl);

	(void) add_subblock_top_conf("ARGON2D", &atheme_argon2d_conf_table);

	(void) add_uint_conf_item("MEMORY", &atheme_argon2d_conf_table, 0, &atheme_argon2d_mcost,
	                          ARGON2D_MEMCOST_MIN, ARGON2D_MEMCOST_MAX, ARGON2D_MEMCOST_DEF);

	(void) add_uint_conf_item("TIME", &atheme_argon2d_conf_table, 0, &atheme_argon2d_tcost,
	                          ARGON2D_TIMECOST_MIN, ARGON2D_TIMECOST_MAX, ARGON2D_TIMECOST_DEF);
}

static void
mod_deinit(const module_unload_intent_t __attribute__((unused)) intent)
{
	(void) del_conf_item("TIME", &atheme_argon2d_conf_table);
	(void) del_conf_item("MEMORY", &atheme_argon2d_conf_table);
	(void) del_top_conf("ARGON2D");

	(void) crypt_unregister(&crypto_argon2d_impl);

	(void) free(argon2d_mempool);
}

SIMPLE_DECLARE_MODULE_V1("crypto/argon2d", MODULE_UNLOAD_CAPABILITY_OK)
