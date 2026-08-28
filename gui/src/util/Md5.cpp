#include "util/Md5.h"

#include <cstring>

namespace n64ui {

namespace {

struct Md5Ctx {
  uint32_t a = 0x67452301, b = 0xefcdab89, c = 0x98badcfe, d = 0x10325476;
  uint64_t len = 0;
  unsigned char buf[64] = {0};
  size_t buflen = 0;

  static uint32_t rotl(uint32_t x, int c) { return (x << c) | (x >> (32 - c)); }

  void process(const unsigned char* p) {
    static const uint32_t K[64] = {
        0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a,
        0xa8304613, 0xfd469501, 0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
        0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821, 0xf61e2562, 0xc040b340,
        0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
        0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8,
        0x676f02d9, 0x8d2a4c8a, 0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
        0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70, 0x289b7ec6, 0xeaa127fa,
        0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
        0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92,
        0xffeff47d, 0x85845dd1, 0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
        0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391};
    static const int S[64] = {
        7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
        5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20,
        4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
        6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21};
    uint32_t m[16];
    for (int i = 0; i < 16; ++i)
      m[i] = (uint32_t)p[i * 4] | ((uint32_t)p[i * 4 + 1] << 8) |
             ((uint32_t)p[i * 4 + 2] << 16) | ((uint32_t)p[i * 4 + 3] << 24);
    uint32_t A = a, B = b, C = c, D = d;
    for (int i = 0; i < 64; ++i) {
      uint32_t F;
      int g;
      if (i < 16) {
        F = (B & C) | (~B & D);
        g = i;
      } else if (i < 32) {
        F = (D & B) | (~D & C);
        g = (5 * i + 1) % 16;
      } else if (i < 48) {
        F = B ^ C ^ D;
        g = (3 * i + 5) % 16;
      } else {
        F = C ^ (B | ~D);
        g = (7 * i) % 16;
      }
      uint32_t tmp = D;
      D = C;
      C = B;
      B = B + rotl(A + F + K[i] + m[g], S[i]);
      A = tmp;
    }
    a += A;
    b += B;
    c += C;
    d += D;
  }

  void update(const unsigned char* data, size_t len) {
    this->len += len;
    while (len > 0) {
      size_t take = 64 - buflen;
      if (take > len) take = len;
      memcpy(buf + buflen, data, take);
      buflen += take;
      data += take;
      len -= take;
      if (buflen == 64) {
        process(buf);
        buflen = 0;
      }
    }
  }

  void final(unsigned char out[16]) {
    uint64_t bitlen = len * 8;
    unsigned char pad = 0x80;
    update(&pad, 1);
    unsigned char zero = 0;
    while (buflen != 56) update(&zero, 1);
    for (int i = 0; i < 8; ++i) {
      unsigned char b = (unsigned char)(bitlen >> (8 * i));
      update(&b, 1);
    }
    for (int i = 0; i < 4; ++i) {
      out[i] = (unsigned char)(a >> (8 * i));
      out[i + 4] = (unsigned char)(b >> (8 * i));
      out[i + 8] = (unsigned char)(c >> (8 * i));
      out[i + 12] = (unsigned char)(d >> (8 * i));
    }
  }
};

}  // namespace

std::string md5Hex(const void* data, size_t len) {
  Md5Ctx ctx;
  ctx.update(static_cast<const unsigned char*>(data), len);
  unsigned char digest[16];
  ctx.final(digest);
  static const char* hex = "0123456789abcdef";
  std::string out;
  out.reserve(32);
  for (unsigned char b : digest) {
    out += hex[b >> 4];
    out += hex[b & 0xf];
  }
  return out;
}

}  // namespace n64ui
