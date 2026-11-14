#pragma once

#include <cstdint>
#include <vector>

#include "../types.h"

namespace recovery::binary {

using Bytes = std::vector<uint8>;

inline void WriteU16(Bytes& bytes, uint16 value) {
  for (usize i = 0; i < 2; ++i) {
    bytes.push_back(static_cast<uint8>(value >> (i * 8)));
  }
}

inline void WriteU32(Bytes& bytes, uint32 value) {
  for (usize i = 0; i < 4; ++i) {
    bytes.push_back(static_cast<uint8>(value >> (i * 8)));
  }
}

inline void WriteU64(Bytes& bytes, uint64 value) {
  for (usize i = 0; i < 8; ++i) {
    bytes.push_back(static_cast<uint8>(value >> (i * 8)));
  }
}

inline void WriteI64(Bytes& bytes, int64 value) {
  WriteU64(bytes, std::bit_cast<uint64>(value));
}

inline uint16 ReadU16(const uint8* data) {
  uint16 value = 0;

  for (usize i = 0; i < 2; ++i) {
    value |= static_cast<uint16>(data[i]) << (i * 8);
  }

  return value;
}

inline uint32 ReadU32(const uint8* data) {
  uint32 value = 0;

  for (usize i = 0; i < 4; ++i) {
    value |= static_cast<uint32>(data[i]) << (i * 8);
  }

  return value;
}

inline uint64 ReadU64(const uint8* data) {
  uint64 value = 0;

  for (usize i = 0; i < 8; ++i) {
    value |= static_cast<uint64>(data[i]) << (i * 8);
  }

  return value;
}

inline int64 ReadI64(const uint8* data) {
  return std::bit_cast<int64>(ReadU64(data));
}

inline uint32 CalculateCrc32(const uint8* data, usize size) {
  uint32 crc = 0xFFFFFFFF;

  for (usize i = 0; i < size; ++i) {
    crc ^= data[i];

    for (int bit = 0; bit < 8; ++bit) {
      const uint32 mask = -(crc & 1);
      crc = (crc >> 1) ^ (0xEDB88320 & mask);
    }
  }

  return ~crc;
}

}  // namespace recovery::binary
