#include "blake2s.h"
#include "utils.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void G(uint32_t v[16], int a, int b,int c,int d, int64_t x, int64_t y) {                      
 
    v[a] = v[a] + v[b] + x;
    v[d] = rotr32(v[d] ^ v[a] , 16);
    v[c] = v[c] + v[d];
    v[b] = rotr32(v[b] ^ v[c] , 12);

    v[a] = v[a] + v[b] + y;
    v[d] = rotr32(v[d] ^ v[a] , 8);
    v[c] = v[c] + v[d];
    v[b] = rotr32(v[b] ^ v[c] , 7);         

}

static void F(blake2s_state* S, uint8_t block[BLAKE2S_BLOCKBYTES])
{
  size_t i, j;
  uint32_t v[16], s[16], m[16];

  for( i = 0; i < 16; ++i ) {
    m[i] = load32( block + i * sizeof( m[i] ) );
  }

  for (i = 0; i < 8; ++i) {
    v[i] = S->h[i];
    v[i + 8] = blake2s_IV[i];
  }

  v[12] ^= S->t[0];
  v[13] ^= S->t[1];
  v[14] ^= S->f[0];
  v[15] ^= S->f[1];

  for (i = 0; i < 12; i++) {
    for (j = 0; j < 16; j++) {
      s[j] = blake2s_sigma[i][j];
    }
    G(v, 0, 4, 8,  12, m[s[0]],  m[s[1]]);
    G(v, 1, 5, 9,  13, m[s[2]],  m[s[3]]);
    G(v, 2, 6, 10, 14, m[s[4]],  m[s[5]]);
    G(v, 3, 7, 11, 15, m[s[6]],  m[s[7]]);
    G(v, 0, 5, 10, 15, m[s[8]],  m[s[9]]);
    G(v, 1, 6, 11, 12, m[s[10]], m[s[11]]);
    G(v, 2, 7, 8,  13, m[s[12]], m[s[13]]);
    G(v, 3, 4, 9,  14, m[s[14]], m[s[15]]);
  }

  for (i = 0; i < 8; i++) {
    S->h[i] = S->h[i] ^ v[i] ^ v[i + 8];
  }
}

int blake2s_init(blake2s_state* S, size_t outlen, const void* key, size_t keylen)
{
  /*initialize key*/
  blake2s_param P[1];
  P->digest_length = (uint8_t)outlen;
  P->key_length = 0;
  P->fanout = 1;
  P->depth = 1;
  store32(&P->leaf_length, 0);
  store32(&P->node_offset, 0);
  P->node_depth = 0;
  P->inner_length = 0;
  memset(P->reserved, 0, sizeof(P->reserved));
  memset(P->salt, 0, sizeof(P->salt));
  memset(P->personal, 0, sizeof(P->personal));

  /*initialize param*/

  const uint8_t* p = (const uint8_t*)(P);
  size_t i;
  memset(S, 0, sizeof(blake2s_state));
  for (i = 0; i < 8; ++i)
    S->h[i] = blake2s_IV[i];
  for (i = 0; i < 8; ++i)
    S->h[i] ^= load32(p + sizeof(S->h[i]) * i);
  S->outlen = P->digest_length;

  if (keylen > 0) {
    uint8_t block[BLAKE2S_BLOCKBYTES];
    memset(block, 0, BLAKE2S_BLOCKBYTES);
    memcpy(block, key, keylen);
    blake2s_update(S, block, BLAKE2S_BLOCKBYTES);
  }
  return 0;
}


int blake2s_update(blake2s_state* S, const void* input_buffer, size_t inlen)
{
  unsigned char* in = (unsigned char*)input_buffer;

  while (inlen > BLAKE2S_BLOCKBYTES) {
    blake2s_increment_counter(S, BLAKE2S_BLOCKBYTES);
    F(S, in);
    in += BLAKE2S_BLOCKBYTES;
    inlen -= BLAKE2S_BLOCKBYTES;
  }
  memcpy(S->buf + S->buflen, in, inlen);
  S->buflen += inlen;
  return 0;
}

int blake2s_final(blake2s_state* S, void* out, size_t outlen)
{
  uint8_t buffer[BLAKE2S_OUTBYTES] = { 0 };
  size_t i;
  blake2s_increment_counter(S, S->buflen);
  S->f[0] = (uint32_t)-1;
  memset(S->buf + S->buflen, 0, BLAKE2S_BLOCKBYTES - S->buflen);
  F(S, S->buf);
  for (i = 0; i < 8; ++i)
    store32(buffer + sizeof(S->h[i]) * i, S->h[i]);
  memcpy(out, buffer, S->outlen);
  return 0;
}


int blake2s(void* output, size_t outlen, const void* input, size_t inlen,
        const void* key, size_t keylen)
{
  blake2s_state S[1];
  if (blake2s_init(S, outlen, key, keylen) < 0)
    return -1;
  if (blake2s_update(S, (const uint8_t*)input, inlen) < 0)
    return -1;
  if (blake2s_final(S, output, outlen) < 0)
    return -1;
  return 0;
}