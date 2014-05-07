#include "hash.h"
#include <intrin.h>
#include <stdlib.h>
#include "ilua/ilua.h"

static void memflip(void* ptr, uint32 length, uint32 word)
{
  if (word == 2)
  {
    uint16* val = (uint16*) ptr;
    for (int pos = 0; pos + word <= length; pos += word)
      *val = _byteswap_ushort(*val), val++;
  }
  else if (word == 4)
  {
    uint32* val = (uint32*) ptr;
    for (uint32 pos = 0; pos + word <= length; pos += word)
      *val = _byteswap_ulong(*val), val++;
  }
  else if (word == 8)
  {
    uint64* val = (uint64*) ptr;
    for (uint32 pos = 0; pos + word <= length; pos += word)
      *val = _byteswap_uint64(*val), val++;
  }
}

uint32 hash_space(int algorithm)
{
  HashAlgorithm* alg = hash_algorithm(algorithm);
  if (alg == NULL) return 0;
  return sizeof(HashState) + alg->stateSize + alg->blockSize;
}
void hash_init_inplace(HashState* state, int algorithm)
{
  state->algorithm = hash_algorithm(algorithm);
  state->state = (uint8*) (state + 1);
  state->buffer = state->state + state->algorithm->stateSize;
  hash_start(state);
}
HashState* hash_init(lua_State* L, int algorithm)
{
  uint32 space = hash_space(algorithm);
  if (space == 0) return NULL;
  HashState* state;
  if (L)
    state = ilua::newstruct<HashState>(L, "hash", space);
  else
    state = (HashState*) malloc(space);
  hash_init_inplace(state, algorithm);
  return state;
}
void hash_free(HashState* state)
{
  free(state);
}

void hash_start(HashState* state)
{
  state->length = 0;
  state->count = 0;
  if (state->algorithm->fn_init)
    state->algorithm->fn_init(state->state);
  else
    memset(state->state, 0, state->algorithm->stateSize);
}

static void hash_run(HashState* state)
{
  if (state->algorithm->bigWordSize)
    memflip(state->buffer, state->algorithm->blockSize, state->algorithm->bigWordSize);
  state->algorithm->fn_update(state->state, state->buffer, state->algorithm->blockSize);
  state->count = 0;
}
void hash_update(HashState* state, void const* message, uint32 length)
{
  if (state->algorithm->blockSize)
  {
    uint8 const* buf = (uint8*) message;
    while (length)
    {
      uint32 cur = state->algorithm->blockSize - state->count;
      if (cur > length) cur = length;
      memcpy(state->buffer + state->count, buf, cur);
      state->count += cur;
      state->length += cur;
      buf += cur;
      length -= cur;
      if (state->count == state->algorithm->blockSize)
        hash_run(state);
    }
  }
  else
    state->algorithm->fn_update(state->state, message, length);
}
void hash_finish(HashState* state, void* output)
{
  if (state->algorithm->blockSize)
  {
    state->buffer[state->count++] = (state->algorithm->padLength ? 0x80 : 0x01);
    if (state->count > state->algorithm->blockSize - state->algorithm->padLength)
    {
      memset(state->buffer + state->count, 0, state->algorithm->blockSize - state->count);
      hash_run(state);
    }
    memset(state->buffer + state->count, 0, state->algorithm->blockSize - state->count);
    if (state->algorithm->padLength)
    {
      state->length *= 8;
      if (state->algorithm->bigWordSize == 0)
        memcpy(state->buffer + state->algorithm->blockSize - state->algorithm->padLength, &state->length,
          state->algorithm->padLength < 8 ? state->algorithm->padLength : 8);
      else
      {
        uint64 len = _byteswap_uint64(state->length);
        if (state->algorithm->padLength < 8)
          memcpy(state->buffer + state->algorithm->blockSize - state->algorithm->padLength,
            ((uint8*) &len) + 8 - state->algorithm->padLength, state->algorithm->padLength);
        else
          memcpy(state->buffer + state->algorithm->blockSize - 8, &len, 8);
      }
    }
    else
      state->buffer[state->algorithm->blockSize - 1] |= 0x80;
    hash_run(state);
  }
  if (state->algorithm->fn_finish)
    state->algorithm->fn_finish(state->state, output);
  else
    memcpy(output, state->state, state->algorithm->outputSize);
  if (state->algorithm->bigWordSize)
    memflip(output, state->algorithm->outputSize, state->algorithm->bigWordSize);
}

static int hs_update(lua_State* L)
{
  HashState* state = ilua::checkobject<HashState>(L, 1, "hash");
  size_t length;
  char const* str = luaL_checklstring(L, 2, &length);
  hash_update(state, str, length);
  return 0;
}
static int hs_finish(lua_State* L)
{
  HashState* state = ilua::checkobject<HashState>(L, 1, "hash");
  luaL_Buffer b;
  char* ptr = luaL_buffinitsize(L, &b, state->algorithm->outputSize);
  hash_finish(state, ptr);
  luaL_pushresultsize(&b, state->algorithm->outputSize);
  return 1;
}
int hash_algorithm_by_name(char const* name)
{
  if (!stricmp(name, "CRC32")) return HashAlgorithm::CRC32;
  if (!stricmp(name, "MD5")) return HashAlgorithm::MD5;
  if (!stricmp(name, "SHA0")) return HashAlgorithm::SHA0;
  if (!stricmp(name, "SHA-0")) return HashAlgorithm::SHA0;
  if (!stricmp(name, "SHA1")) return HashAlgorithm::SHA1;
  if (!stricmp(name, "SHA-1")) return HashAlgorithm::SHA1;
  if (!stricmp(name, "SHA2")) return HashAlgorithm::SHA256;
  if (!stricmp(name, "SHA-2")) return HashAlgorithm::SHA256;
  if (!stricmp(name, "SHA-224")) return HashAlgorithm::SHA224;
  if (!stricmp(name, "SHA-256")) return HashAlgorithm::SHA256;
  if (!stricmp(name, "SHA-384")) return HashAlgorithm::SHA384;
  if (!stricmp(name, "SHA-512")) return HashAlgorithm::SHA512;
  if (!stricmp(name, "SHA-512/224")) return HashAlgorithm::SHA512_224;
  if (!stricmp(name, "SHA-512/256")) return HashAlgorithm::SHA512_256;
  if (!stricmp(name, "Keccak-224")) return HashAlgorithm::KECCAK224;
  if (!stricmp(name, "Keccak-256")) return HashAlgorithm::KECCAK256;
  if (!stricmp(name, "Keccak-384")) return HashAlgorithm::KECCAK384;
  if (!stricmp(name, "Keccak-512")) return HashAlgorithm::KECCAK512;
  return 0;
}
static int hf_init(lua_State* L)
{
  char const* alg = luaL_checkstring(L, 1);
  int id = hash_algorithm_by_name(alg);
  if (id == 0) luaL_argerror(L, 1, "unknown algorithm");

  hash_init(L, id);
  return 1;
}
static int hf_digest(lua_State* L)
{
  size_t length;
  char const* msg = luaL_checklstring(L, 1, &length);
  char const* alg = luaL_optstring(L, 2, "MD5");
  int id = hash_algorithm_by_name(alg);
  if (id == 0) luaL_argerror(L, 2, "unknown algorithm");

  HashState* state = hash_init(NULL, id);
  hash_update(state, msg, length);
  luaL_Buffer b;
  char* ptr = luaL_buffinitsize(L, &b, state->algorithm->outputSize);
  hash_finish(state, ptr);
  luaL_pushresultsize(&b, state->algorithm->outputSize);
  hash_free(state);

  return 1; 
}
static int hf_digest_up(lua_State* L)
{
  size_t length;
  char const* msg = luaL_checklstring(L, 1, &length);
  int id = lua_tointeger(L, lua_upvalueindex(1));

  HashState* state = hash_init(NULL, id);
  hash_update(state, msg, length);
  luaL_Buffer b;
  char* ptr = luaL_buffinitsize(L, &b, state->algorithm->outputSize);
  hash_finish(state, ptr);
  luaL_pushresultsize(&b, state->algorithm->outputSize);
  hash_free(state);

  return 1; 
}
static void hf_regalg(lua_State* L, char const* name, int id)
{
  lua_pushinteger(L, id);
  lua_pushcclosure(L, hf_digest_up, 1);
  lua_setfield(L, -2, name);
}
void hash_bind(lua_State* L)
{
  ilua::newtype<HashState>(L, "hash");
  ilua::bindmethod(L, "update", hs_update);
  ilua::bindmethod(L, "finish", hs_finish);
  lua_pop(L, 2);

  ilua::openlib(L, "hash");
  ilua::bindmethod(L, "init", hf_init);
  ilua::bindmethod(L, "digest", hf_digest);
  hf_regalg(L, "crc32", HashAlgorithm::CRC32);
  hf_regalg(L, "md5", HashAlgorithm::MD5);
  hf_regalg(L, "sha0", HashAlgorithm::SHA0);
  hf_regalg(L, "sha1", HashAlgorithm::SHA1);
  hf_regalg(L, "sha2", HashAlgorithm::SHA256);
  hf_regalg(L, "sha224", HashAlgorithm::SHA224);
  hf_regalg(L, "sha256", HashAlgorithm::SHA256);
  hf_regalg(L, "sha384", HashAlgorithm::SHA384);
  hf_regalg(L, "sha512", HashAlgorithm::SHA512);
  lua_pop(L, 1);
}

/////////////////////////////////////////////////////////////////////////

#define rotl32      _lrotl
#define rotr32      _lrotr
#define rotl64      _rotl64
#define rotr64      _rotr64

/////////////////////////////////////////////////////////////////////////

static const uint32 crc_table[256] = {
  0x00000000, 0x04C11DB7, 0x09823B6E, 0x0D4326D9, 0x130476DC, 0x17C56B6B, 0x1A864DB2, 0x1E475005,
  0x2608EDB8, 0x22C9F00F, 0x2F8AD6D6, 0x2B4BCB61, 0x350C9B64, 0x31CD86D3, 0x3C8EA00A, 0x384FBDBD,
  0x4C11DB70, 0x48D0C6C7, 0x4593E01E, 0x4152FDA9, 0x5F15ADAC, 0x5BD4B01B, 0x569796C2, 0x52568B75,
  0x6A1936C8, 0x6ED82B7F, 0x639B0DA6, 0x675A1011, 0x791D4014, 0x7DDC5DA3, 0x709F7B7A, 0x745E66CD,
  0x9823B6E0, 0x9CE2AB57, 0x91A18D8E, 0x95609039, 0x8B27C03C, 0x8FE6DD8B, 0x82A5FB52, 0x8664E6E5,
  0xBE2B5B58, 0xBAEA46EF, 0xB7A96036, 0xB3687D81, 0xAD2F2D84, 0xA9EE3033, 0xA4AD16EA, 0xA06C0B5D,
  0xD4326D90, 0xD0F37027, 0xDDB056FE, 0xD9714B49, 0xC7361B4C, 0xC3F706FB, 0xCEB42022, 0xCA753D95,
  0xF23A8028, 0xF6FB9D9F, 0xFBB8BB46, 0xFF79A6F1, 0xE13EF6F4, 0xE5FFEB43, 0xE8BCCD9A, 0xEC7DD02D,
  0x34867077, 0x30476DC0, 0x3D044B19, 0x39C556AE, 0x278206AB, 0x23431B1C, 0x2E003DC5, 0x2AC12072,
  0x128E9DCF, 0x164F8078, 0x1B0CA6A1, 0x1FCDBB16, 0x018AEB13, 0x054BF6A4, 0x0808D07D, 0x0CC9CDCA,
  0x7897AB07, 0x7C56B6B0, 0x71159069, 0x75D48DDE, 0x6B93DDDB, 0x6F52C06C, 0x6211E6B5, 0x66D0FB02,
  0x5E9F46BF, 0x5A5E5B08, 0x571D7DD1, 0x53DC6066, 0x4D9B3063, 0x495A2DD4, 0x44190B0D, 0x40D816BA,
  0xACA5C697, 0xA864DB20, 0xA527FDF9, 0xA1E6E04E, 0xBFA1B04B, 0xBB60ADFC, 0xB6238B25, 0xB2E29692,
  0x8AAD2B2F, 0x8E6C3698, 0x832F1041, 0x87EE0DF6, 0x99A95DF3, 0x9D684044, 0x902B669D, 0x94EA7B2A,
  0xE0B41DE7, 0xE4750050, 0xE9362689, 0xEDF73B3E, 0xF3B06B3B, 0xF771768C, 0xFA325055, 0xFEF34DE2,
  0xC6BCF05F, 0xC27DEDE8, 0xCF3ECB31, 0xCBFFD686, 0xD5B88683, 0xD1799B34, 0xDC3ABDED, 0xD8FBA05A,
  0x690CE0EE, 0x6DCDFD59, 0x608EDB80, 0x644FC637, 0x7A089632, 0x7EC98B85, 0x738AAD5C, 0x774BB0EB,
  0x4F040D56, 0x4BC510E1, 0x46863638, 0x42472B8F, 0x5C007B8A, 0x58C1663D, 0x558240E4, 0x51435D53,
  0x251D3B9E, 0x21DC2629, 0x2C9F00F0, 0x285E1D47, 0x36194D42, 0x32D850F5, 0x3F9B762C, 0x3B5A6B9B,
  0x0315D626, 0x07D4CB91, 0x0A97ED48, 0x0E56F0FF, 0x1011A0FA, 0x14D0BD4D, 0x19939B94, 0x1D528623,
  0xF12F560E, 0xF5EE4BB9, 0xF8AD6D60, 0xFC6C70D7, 0xE22B20D2, 0xE6EA3D65, 0xEBA91BBC, 0xEF68060B,
  0xD727BBB6, 0xD3E6A601, 0xDEA580D8, 0xDA649D6F, 0xC423CD6A, 0xC0E2D0DD, 0xCDA1F604, 0xC960EBB3,
  0xBD3E8D7E, 0xB9FF90C9, 0xB4BCB610, 0xB07DABA7, 0xAE3AFBA2, 0xAAFBE615, 0xA7B8C0CC, 0xA379DD7B,
  0x9B3660C6, 0x9FF77D71, 0x92B45BA8, 0x9675461F, 0x8832161A, 0x8CF30BAD, 0x81B02D74, 0x857130C3,
  0x5D8A9099, 0x594B8D2E, 0x5408ABF7, 0x50C9B640, 0x4E8EE645, 0x4A4FFBF2, 0x470CDD2B, 0x43CDC09C,
  0x7B827D21, 0x7F436096, 0x7200464F, 0x76C15BF8, 0x68860BFD, 0x6C47164A, 0x61043093, 0x65C52D24,
  0x119B4BE9, 0x155A565E, 0x18197087, 0x1CD86D30, 0x029F3D35, 0x065E2082, 0x0B1D065B, 0x0FDC1BEC,
  0x3793A651, 0x3352BBE6, 0x3E119D3F, 0x3AD08088, 0x2497D08D, 0x2056CD3A, 0x2D15EBE3, 0x29D4F654,
  0xC5A92679, 0xC1683BCE, 0xCC2B1D17, 0xC8EA00A0, 0xD6AD50A5, 0xD26C4D12, 0xDF2F6BCB, 0xDBEE767C,
  0xE3A1CBC1, 0xE760D676, 0xEA23F0AF, 0xEEE2ED18, 0xF0A5BD1D, 0xF464A0AA, 0xF9278673, 0xFDE69BC4,
  0x89B8FD09, 0x8D79E0BE, 0x803AC667, 0x84FBDBD0, 0x9ABC8BD5, 0x9E7D9662, 0x933EB0BB, 0x97FFAD0C,
  0xAFB010B1, 0xAB710D06, 0xA6322BDF, 0xA2F33668, 0xBCB4666D, 0xB8757BDA, 0xB5365D03, 0xB1F740B4,
};
static void crc32_init(void* state)
{
  *(uint32*) state = 0xFFFFFFFF;
}
static void crc32_update(void* state, void const* chunk, uint32 length)
{
  uint8 const* buf = (uint8*) chunk;
  uint32 crc = *(uint32*) state;
  for (uint32 i = 0; i < length; i++)
    crc = crc_table[(crc ^ buf[i]) & 0xFF] ^ (crc >> 8);
  *(uint32*) state = crc;
}
static void crc32_finish(void* state, void* output)
{
  *(uint32*) output = ~(*(uint32*) state);
}

/////////////////////////////////////////////////////////////////////////

static const uint32 md5_r[64] = {
  7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
  5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20,
  4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
  6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21
};
static const uint32 md5_k[64] = {
  0xD76AA478, 0xE8C7B756, 0x242070DB, 0xC1BDCEEE,
  0xF57C0FAF, 0x4787C62A, 0xA8304613, 0xFD469501,
  0x698098D8, 0x8B44F7AF, 0xFFFF5BB1, 0x895CD7BE,
  0x6B901122, 0xFD987193, 0xA679438E, 0x49B40821,
  0xF61E2562, 0xC040B340, 0x265E5A51, 0xE9B6C7AA,
  0xD62F105D, 0x02441453, 0xD8A1E681, 0xE7D3FBC8,
  0x21E1CDE6, 0xC33707D6, 0xF4D50D87, 0x455A14ED,
  0xA9E3E905, 0xFCEFA3F8, 0x676F02D9, 0x8D2A4C8A,
  0xFFFA3942, 0x8771F681, 0x6D9D6122, 0xFDE5380C,
  0xA4BEEA44, 0x4BDECFA9, 0xF6BB4B60, 0xBEBFBC70,
  0x289B7EC6, 0xEAA127FA, 0xD4EF3085, 0x04881D05,
  0xD9D4D039, 0xE6DB99E5, 0x1FA27CF8, 0xC4AC5665,
  0xF4292244, 0x432AFF97, 0xAB9423A7, 0xFC93A039,
  0x655B59C3, 0x8F0CCC92, 0xFFEFF47D, 0x85845DD1,
  0x6FA87E4F, 0xFE2CE6E0, 0xA3014314, 0x4E0811A1,
  0xF7537E82, 0xBD3AF235, 0x2AD7D2BB, 0xEB86D391,
};
static void md5_init(void* state)
{
  uint32* s = (uint32*) state;
  s[0] = 0x67452301;
  s[1] = 0xEFCDAB89;
  s[2] = 0x98BADCFE;
  s[3] = 0x10325476;
}
static void md5_update(void* state, void const* chunk, uint32 length)
{
  uint32* s = (uint32*) state;
  uint32* m = (uint32*) chunk;

  uint32 a = s[0];
  uint32 b = s[1];
  uint32 c = s[2];
  uint32 d = s[3];
  for (int i = 0; i < 64; i++)
  {
    uint32 f, g;
    if (i < 16)
      f = (b & c) | ((~b) & d), g = i;
    else if (i < 32)
      f = (d & b) | ((~d) & c), g = (i * 5 + 1) & 0x0F;
    else if (i < 48)
      f = b ^ c ^ d, g = (i * 3 + 5) & 0x0F;
    else
      f = c ^ (b | (~d)), g = (i * 7) & 0x0F;
    uint32 temp = d;
    d = c;
    c = b;
    b += rotl32(a + f + md5_k[i] + m[g], md5_r[i]);
    a = temp;
  }
  s[0] += a;
  s[1] += b;
  s[2] += c;
  s[3] += d;
}

/////////////////////////////////////////////////////////////////////////

static void sha1_init(void* state)
{
  uint32* h = (uint32*) state;
  h[0] = 0x67452301;
  h[1] = 0xEFCDAB89;
  h[2] = 0x98BADCFE;
  h[3] = 0x10325476;
  h[4] = 0xC3D2E1F0;
}
static void shax_update(void* state, void const* chunk, uint32 length, int v)
{
  uint32* h = (uint32*) state;

  uint32 w[80];
  memcpy(w, chunk, 64);
  for (int i = 16; i < 80; i++)
    w[i] = rotl32(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);

  uint32 a = h[0];
  uint32 b = h[1];
  uint32 c = h[2];
  uint32 d = h[3];
  uint32 e = h[4];
  for (int i = 0; i < 80; i++)
  {
    uint32 f, k;
    if (i < 20)
      f = (b & c) | ((~b) & d), k = 0x5A827999;
    else if (i < 40)
      f = b ^ c ^ d, k = 0x6ED9EBA1;
    else if (i < 60)
      f = (b & c) | (b & d) | (c & d), k = 0x8F1BBCDC;
    else
      f = b ^ c ^ d, k = 0xCA62C1D6;
    uint32 temp = rotl32(a, 5) + f + e + k + w[i];
    e = d;
    d = c;
    c = (v ? rotl32(b, 30) : b);
    b = a;
    a = temp;
  }
  h[0] += a;
  h[1] += b;
  h[2] += c;
  h[3] += d;
  h[4] += e;
}
static void sha0_update(void* state, void const* chunk, uint32 length)
{
  shax_update(state, chunk, length, 0);
}
static void sha1_update(void* state, void const* chunk, uint32 length)
{
  shax_update(state, chunk, length, 1);
}

/////////////////////////////////////////////////////////////////////////

static const uint32 sha256_k[64] = {
  0x428A2F98, 0x71374491, 0xB5C0FBCF, 0xE9B5DBA5, 0x3956C25B, 0x59F111F1, 0x923F82A4, 0xAB1C5ED5,
  0xD807AA98, 0x12835B01, 0x243185BE, 0x550C7DC3, 0x72BE5D74, 0x80DEB1FE, 0x9BDC06A7, 0xC19BF174,
  0xE49B69C1, 0xEFBE4786, 0x0FC19DC6, 0x240CA1CC, 0x2DE92C6F, 0x4A7484AA, 0x5CB0A9DC, 0x76F988DA,
  0x983E5152, 0xA831C66D, 0xB00327C8, 0xBF597FC7, 0xC6E00BF3, 0xD5A79147, 0x06CA6351, 0x14292967,
  0x27B70A85, 0x2E1B2138, 0x4D2C6DFC, 0x53380D13, 0x650A7354, 0x766A0ABB, 0x81C2C92E, 0x92722C85,
  0xA2BFE8A1, 0xA81A664B, 0xC24B8B70, 0xC76C51A3, 0xD192E819, 0xD6990624, 0xF40E3585, 0x106AA070,
  0x19A4C116, 0x1E376C08, 0x2748774C, 0x34B0BCB5, 0x391C0CB3, 0x4ED8AA4A, 0x5B9CCA4F, 0x682E6FF3,
  0x748F82EE, 0x78A5636F, 0x84C87814, 0x8CC70208, 0x90BEFFFA, 0xA4506CEB, 0xBEF9A3F7, 0xC67178F2,
};
static void sha224_init(void* state)
{
  uint32* h = (uint32*) state;
  h[0] = 0xC1059ED8;
  h[1] = 0x367CD507;
  h[2] = 0x3070DD17;
  h[3] = 0xF70E5939;
  h[4] = 0xFFC00B31;
  h[5] = 0x68581511;
  h[6] = 0x64F98FA7;
  h[7] = 0xBEFA4FA4;
}
static void sha256_init(void* state)
{
  uint32* h = (uint32*) state;
  h[0] = 0x6A09E667;
  h[1] = 0xBB67AE85;
  h[2] = 0x3C6EF372;
  h[3] = 0xA54FF53A;
  h[4] = 0x510E527F;
  h[5] = 0x9B05688C;
  h[6] = 0x1F83D9AB;
  h[7] = 0x5BE0CD19;
}
static void sha256_update(void* state, void const* chunk, uint32 length)
{
  uint32* H = (uint32*) state;

  uint32 w[64];
  memcpy(w, chunk, 64);
  for (int i = 16; i < 64; i++)
  {
    uint32 s0 = rotr32(w[i - 15], 7) ^ rotr32(w[i - 15], 18) ^ (w[i - 15] >> 3);
    uint32 s1 = rotr32(w[i - 2], 17) ^ rotr32(w[i - 2], 19) ^ (w[i - 2] >> 10);
    w[i] = w[i - 16] + s0 + w[i - 7] + s1;
  }

  uint32 a = H[0];
  uint32 b = H[1];
  uint32 c = H[2];
  uint32 d = H[3];
  uint32 e = H[4];
  uint32 f = H[5];
  uint32 g = H[6];
  uint32 h = H[7];
  for (int i = 0; i < 64; i++)
  {
    uint32 S1 = rotr32(e, 6) ^ rotr32(e, 11) ^ rotr32(e, 25);
    uint32 ch = (e & f) ^ ((~e) & g);
    uint32 temp1 = h + S1 + ch + sha256_k[i] + w[i];
    uint32 S0 = rotr32(a, 2) ^ rotr32(a, 13) ^ rotr32(a, 22);
    uint32 maj = (a & b) ^ (a & c) ^ (b & c);
    uint32 temp2 = S0 + maj;

    h = g;
    g = f;
    f = e;
    e = d + temp1;
    d = c;
    c = b;
    b = a;
    a = temp1 + temp2;
  }
  H[0] += a;
  H[1] += b;
  H[2] += c;
  H[3] += d;
  H[4] += e;
  H[5] += f;
  H[6] += g;
  H[7] += h;
}

/////////////////////////////////////////////////////////////////////////

static const uint64 sha512_k[80] = {
  0x428A2F98D728AE22ULL, 0x7137449123EF65CDULL, 0xB5C0FBCFEC4D3B2FULL, 0xE9B5DBA58189DBBCULL, 0x3956C25BF348B538ULL,
  0x59F111F1B605D019ULL, 0x923F82A4AF194F9BULL, 0xAB1C5ED5DA6D8118ULL, 0xD807AA98A3030242ULL, 0x12835B0145706FBEULL,
  0x243185BE4EE4B28CULL, 0x550C7DC3D5FFB4E2ULL, 0x72BE5D74F27B896FULL, 0x80DEB1FE3B1696B1ULL, 0x9BDC06A725C71235ULL,
  0xC19BF174CF692694ULL, 0xE49B69C19EF14AD2ULL, 0xEFBE4786384F25E3ULL, 0x0FC19DC68B8CD5B5ULL, 0x240CA1CC77AC9C65ULL,
  0x2DE92C6F592B0275ULL, 0x4A7484AA6EA6E483ULL, 0x5CB0A9DCBD41FBD4ULL, 0x76F988DA831153B5ULL, 0x983E5152EE66DFABULL,
  0xA831C66D2DB43210ULL, 0xB00327C898FB213FULL, 0xBF597FC7BEEF0EE4ULL, 0xC6E00BF33DA88FC2ULL, 0xD5A79147930AA725ULL,
  0x06CA6351E003826FULL, 0x142929670A0E6E70ULL, 0x27B70A8546D22FFCULL, 0x2E1B21385C26C926ULL, 0x4D2C6DFC5AC42AEDULL,
  0x53380D139D95B3DFULL, 0x650A73548BAF63DEULL, 0x766A0ABB3C77B2A8ULL, 0x81C2C92E47EDAEE6ULL, 0x92722C851482353BULL,
  0xA2BFE8A14CF10364ULL, 0xA81A664BBC423001ULL, 0xC24B8B70D0F89791ULL, 0xC76C51A30654BE30ULL, 0xD192E819D6EF5218ULL,
  0xD69906245565A910ULL, 0xF40E35855771202AULL, 0x106AA07032BBD1B8ULL, 0x19A4C116B8D2D0C8ULL, 0x1E376C085141AB53ULL,
  0x2748774CDF8EEB99ULL, 0x34B0BCB5E19B48A8ULL, 0x391C0CB3C5C95A63ULL, 0x4ED8AA4AE3418ACBULL, 0x5B9CCA4F7763E373ULL,
  0x682E6FF3D6B2B8A3ULL, 0x748F82EE5DEFB2FCULL, 0x78A5636F43172F60ULL, 0x84C87814A1F0AB72ULL, 0x8CC702081A6439ECULL,
  0x90BEFFFA23631E28ULL, 0xA4506CEBDE82BDE9ULL, 0xBEF9A3F7B2C67915ULL, 0xC67178F2E372532BULL, 0xCA273ECEEA26619CULL,
  0xD186B8C721C0C207ULL, 0xEADA7DD6CDE0EB1EULL, 0xF57D4F7FEE6ED178ULL, 0x06F067AA72176FBAULL, 0x0A637DC5A2C898A6ULL,
  0x113F9804BEF90DAEULL, 0x1B710B35131C471BULL, 0x28DB77F523047D84ULL, 0x32CAAB7B40C72493ULL, 0x3C9EBE0A15C9BEBCULL,
  0x431D67C49C100D4CULL, 0x4CC5D4BECB3E42B6ULL, 0x597F299CFC657E2AULL, 0x5FCB6FAB3AD6FAECULL, 0x6C44198C4A475817ULL,
};
static void sha384_init(void* state)
{
  uint64* h = (uint64*) state;
  h[0] = 0xCBBB9D5DC1059ED8ULL;
  h[1] = 0x629A292A367CD507ULL;
  h[2] = 0x9159015A3070DD17ULL;
  h[3] = 0x152FECD8F70E5939ULL;
  h[4] = 0x67332667FFC00B31ULL;
  h[5] = 0x8EB44A8768581511ULL;
  h[6] = 0xDB0C2E0D64F98FA7ULL;
  h[7] = 0x47B5481DBEFA4FA4ULL;
}
static void sha512_init(void* state)
{
  uint64* h = (uint64*) state;
  h[0] = 0x6A09E667F3BCC908ULL;
  h[1] = 0xBB67AE8584CAA73BULL;
  h[2] = 0x3C6EF372FE94F82BULL;
  h[3] = 0xA54FF53A5F1D36F1ULL;
  h[4] = 0x510E527FADE682D1ULL;
  h[5] = 0x9B05688C2B3E6C1FULL;
  h[6] = 0x1F83D9ABFB41BD6BULL;
  h[7] = 0x5BE0CD19137E2179ULL;
}
static void sha512_224_init(void* state)
{
  uint64* h = (uint64*) state;
  h[0] = 0x8C3D37C819544DA2ULL;
  h[1] = 0x73E1996689DCD4D6ULL;
  h[2] = 0x1DFAB7AE32FF9C82ULL;
  h[3] = 0x679DD514582F9FCFULL;
  h[4] = 0x0F6D2B697BD44DA8ULL;
  h[5] = 0x77E36F7304C48942ULL;
  h[6] = 0x3F9D85A86A1D36C8ULL;
  h[7] = 0x1112E6AD91D692A1ULL;
}
static void sha512_256_init(void* state)
{
  uint64* h = (uint64*) state;
  h[0] = 0x22312194FC2BF72CULL;
  h[1] = 0x9F555FA3C84C64C2ULL;
  h[2] = 0x2393B86B6F53B151ULL;
  h[3] = 0x963877195940EABDULL;
  h[4] = 0x96283EE2A88EFFE3ULL;
  h[5] = 0xBE5E1E2553863992ULL;
  h[6] = 0x2B0199FC2C85B8AAULL;
  h[7] = 0x0EB72DDC81C52CA2ULL;
}
static void sha512_update(void* state, void const* chunk, uint32 length)
{
  uint64* H = (uint64*) state;

  uint64 w[80];
  memcpy(w, chunk, 128);
  for (int i = 16; i < 80; i++)
  {
    uint64 s0 = rotr64(w[i - 15], 1) ^ rotr64(w[i - 15], 8) ^ (w[i - 15] >> 7);
    uint64 s1 = rotr64(w[i - 2], 19) ^ rotr64(w[i - 2], 61) ^ (w[i - 2] >> 6);
    w[i] = w[i - 16] + s0 + w[i - 7] + s1;
  }

  uint64 a = H[0];
  uint64 b = H[1];
  uint64 c = H[2];
  uint64 d = H[3];
  uint64 e = H[4];
  uint64 f = H[5];
  uint64 g = H[6];
  uint64 h = H[7];
  for (int i = 0; i < 80; i++)
  {
    uint64 S1 = rotr64(e, 14) ^ rotr64(e, 18) ^ rotr64(e, 41);
    uint64 ch = (e & f) ^ ((~e) & g);
    uint64 temp1 = h + S1 + ch + sha512_k[i] + w[i];
    uint64 S0 = rotr64(a, 28) ^ rotr64(a, 34) ^ rotr64(a, 39);
    uint64 maj = (a & b) ^ (a & c) ^ (b & c);
    uint64 temp2 = S0 + maj;

    h = g;
    g = f;
    f = e;
    e = d + temp1;
    d = c;
    c = b;
    b = a;
    a = temp1 + temp2;
  }
  H[0] += a;
  H[1] += b;
  H[2] += c;
  H[3] += d;
  H[4] += e;
  H[5] += f;
  H[6] += g;
  H[7] += h;
}

/////////////////////////////////////////////////////////////////////////

static const uint64 keccak_round_constants[24] = {
  0x0000000000000001, 0x0000000000008082, 0x800000000000808A, 0x8000000080008000,
  0x000000000000808B, 0x0000000080000001, 0x8000000080008081, 0x8000000000008009,
  0x000000000000008A, 0x0000000000000088, 0x0000000080008009, 0x000000008000000A,
  0x000000008000808B, 0x800000000000008B, 0x8000000000008089, 0x8000000000008003,
  0x8000000000008002, 0x8000000000000080, 0x000000000000800A, 0x800000008000000A,
  0x8000000080008081, 0x8000000000008080, 0x0000000080000001, 0x8000000080008008,
};
static void keccak_update(void* state, void const* chunk, uint32 length)
{
  uint64* a = (uint64*) state;
  uint64* dst = a;
  uint64 const* src = (uint64*) chunk;
  for (int i = 0; i < length; i += 8)
    *dst++ ^= *src++;

  uint64 t[25];

  for (int n = 0; n < 24; n++)
  {
    // theta
    uint64 c0 = a[0] ^ a[5] ^ a[10] ^ a[15] ^ a[20];
    uint64 c1 = a[1] ^ a[6] ^ a[11] ^ a[16] ^ a[21];
    uint64 c2 = a[2] ^ a[7] ^ a[12] ^ a[17] ^ a[22];
    uint64 c3 = a[3] ^ a[8] ^ a[13] ^ a[18] ^ a[23];
    uint64 c4 = a[4] ^ a[9] ^ a[14] ^ a[19] ^ a[24];
    uint64 d0 = rotl64(c1, 1) ^ c4;
    uint64 d1 = rotl64(c2, 1) ^ c0;
    uint64 d2 = rotl64(c3, 1) ^ c1;
    uint64 d3 = rotl64(c4, 1) ^ c2;
    uint64 d4 = rotl64(c0, 1) ^ c3;
    a[ 0] ^= d0; a[ 5] ^= d0; a[10] ^= d0; a[15] ^= d0; a[20] ^= d0;
    a[ 1] ^= d1; a[ 6] ^= d1; a[11] ^= d1; a[16] ^= d1; a[21] ^= d1;
    a[ 2] ^= d2; a[ 7] ^= d2; a[12] ^= d2; a[17] ^= d2; a[22] ^= d2;
    a[ 3] ^= d3; a[ 8] ^= d3; a[13] ^= d3; a[18] ^= d3; a[23] ^= d3;
    a[ 4] ^= d4; a[ 9] ^= d4; a[14] ^= d4; a[19] ^= d4; a[24] ^= d4;

    // rho pi
    t[0] = a[0];
    t[10] = rotl64(a[ 1],  1); t[20] = rotl64(a[ 2], 62); t[ 5] = rotl64(a[ 3], 28); t[15] = rotl64(a[ 4], 27);
    t[16] = rotl64(a[ 5], 36); t[ 1] = rotl64(a[ 6], 44); t[11] = rotl64(a[ 7],  6); t[21] = rotl64(a[ 8], 55);
    t[ 6] = rotl64(a[ 9], 20); t[ 7] = rotl64(a[10],  3); t[17] = rotl64(a[11], 10); t[ 2] = rotl64(a[12], 43);
    t[12] = rotl64(a[13], 25); t[22] = rotl64(a[14], 39); t[23] = rotl64(a[15], 41); t[ 8] = rotl64(a[16], 45);
    t[18] = rotl64(a[17], 15); t[ 3] = rotl64(a[18], 21); t[13] = rotl64(a[19],  8); t[14] = rotl64(a[20], 18);
    t[24] = rotl64(a[21],  2); t[ 9] = rotl64(a[22], 61); t[19] = rotl64(a[23], 56); t[ 4] = rotl64(a[24], 14);

    // chi
    a[ 0] = t[ 0] ^ ((~t[ 1]) & t[ 2]); a[ 1] = t[ 1] ^ ((~t[ 2]) & t[ 3]); a[ 2] = t[ 2] ^ ((~t[ 3]) & t[ 4]);
    a[ 3] = t[ 3] ^ ((~t[ 4]) & t[ 0]); a[ 4] = t[ 4] ^ ((~t[ 0]) & t[ 1]); a[ 5] = t[ 5] ^ ((~t[ 6]) & t[ 7]);
    a[ 6] = t[ 6] ^ ((~t[ 7]) & t[ 8]); a[ 7] = t[ 7] ^ ((~t[ 8]) & t[ 9]); a[ 8] = t[ 8] ^ ((~t[ 9]) & t[ 5]);
    a[ 9] = t[ 9] ^ ((~t[ 5]) & t[ 6]); a[10] = t[10] ^ ((~t[11]) & t[12]); a[11] = t[11] ^ ((~t[12]) & t[13]);
    a[12] = t[12] ^ ((~t[13]) & t[14]); a[13] = t[13] ^ ((~t[14]) & t[10]); a[14] = t[14] ^ ((~t[10]) & t[11]);
    a[15] = t[15] ^ ((~t[16]) & t[17]); a[16] = t[16] ^ ((~t[17]) & t[18]); a[17] = t[17] ^ ((~t[18]) & t[19]);
    a[18] = t[18] ^ ((~t[19]) & t[15]); a[19] = t[19] ^ ((~t[15]) & t[16]); a[20] = t[20] ^ ((~t[21]) & t[22]);
    a[21] = t[21] ^ ((~t[22]) & t[23]); a[22] = t[22] ^ ((~t[23]) & t[24]); a[23] = t[23] ^ ((~t[24]) & t[20]);
    a[24] = t[24] ^ ((~t[20]) & t[21]);

    a[0] ^= keccak_round_constants[n];
  }
}

/////////////////////////////////////////////////////////////////////////

static HashAlgorithm algorithms[] = {
// blk  out  sta  pad  big
  {  0,   4,   4,   0,   0, crc32_init, crc32_update, crc32_finish},
  { 64,  16,  16,   8,   0, md5_init, md5_update, NULL},
  { 64,  20,  20,   8,   4, sha1_init, sha0_update, NULL},
  { 64,  20,  20,   8,   4, sha1_init, sha1_update, NULL},
  { 64,  28,  32,   8,   4, sha224_init, sha256_update, NULL},
  { 64,  32,  32,   8,   4, sha256_init, sha256_update, NULL},
  {128,  48,  64,  16,   8, sha384_init, sha512_update, NULL},
  {128,  64,  64,  16,   8, sha512_init, sha512_update, NULL},
  {128,  28,  64,  16,   8, sha512_224_init, sha512_update, NULL},
  {128,  32,  64,  16,   8, sha512_256_init, sha512_update, NULL},
  {144,  28, 200,   0,   0, NULL, keccak_update, NULL},
  {136,  32, 200,   0,   0, NULL, keccak_update, NULL},
  {104,  48, 200,   0,   0, NULL, keccak_update, NULL},
  { 72,  64, 200,   0,   0, NULL, keccak_update, NULL},
};
HashAlgorithm* hash_algorithm(int id)
{
  if (id < HashAlgorithm::FIRST || id > HashAlgorithm::LAST) return NULL;
  return &algorithms[id - HashAlgorithm::FIRST];
}
