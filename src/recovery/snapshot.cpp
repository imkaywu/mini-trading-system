#include "snapshot.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <fstream>
#include <limits>
#include <vector>

#include "binary_utils.h"

namespace recovery {

namespace {

constexpr std::array<uint8, 4> Magic = {'S', 'N', 'P', '1'};
constexpr uint16 Version = 1;
constexpr usize HeaderSize = 20;
constexpr usize OrderSize = 25;
constexpr usize PayloadPrefixSize = 16;

bool32 ReadExact(std::ifstream& file, uint8* data, usize size) {
  file.read(reinterpret_cast<char*>(data), static_cast<std::streamsize>(size));

  return file.gcount() == static_cast<std::streamsize>(size);
}

}  // namespace

bool32 WriteSnapshot(const std::string& path,
                     const matching::LimitOrderBook& order_book) {
  const matching::LimitOrderBookSnapshot snapshot = order_book.ExportSnapshot();

  binary::Bytes payload;

  binary::WriteU64(payload, snapshot.next_order_id);
  binary::WriteU64(payload, snapshot.orders.size());

  for (const auto& order : snapshot.orders) {
    binary::WriteU64(payload, order.id);
    payload.push_back(static_cast<uint8>(order.side));
    binary::WriteI64(payload, order.price);
    binary::WriteI64(payload, order.quantity);
  }

  binary::Bytes record;

  record.insert(record.end(), Magic.begin(), Magic.end());
  binary::WriteU16(record, Version);
  binary::WriteU16(record, 0);  // Reserved.
  binary::WriteU64(record, payload.size());

  binary::Bytes checksum_data(record.begin() + 4, record.end());
  checksum_data.insert(checksum_data.end(), payload.begin(), payload.end());

  const uint32 checksum =
      binary::CalculateCrc32(checksum_data.data(), checksum_data.size());

  binary::WriteU32(record, checksum);

  record.insert(record.end(), payload.begin(), payload.end());

  std::ofstream file(path, std::ios::binary | std::ios::trunc);

  if (!file.is_open()) {
    return false;
  }

  file.write(reinterpret_cast<const char*>(record.data()),
             static_cast<std::streamsize>(record.size()));

  file.flush();

  return static_cast<bool>(file);
}

bool32 ReadSnapshot(const std::string& path,
                    matching::LimitOrderBook& order_book) {
  std::ifstream file(path, std::ios::binary);

  if (!file.is_open()) {
    return false;
  }

  std::array<uint8, HeaderSize> header{};

  if (!ReadExact(file, header.data(), header.size())) {
    return false;
  }

  if (!std::equal(Magic.begin(), Magic.end(), header.begin())) {
    return false;
  }

  const uint16 version = binary::ReadU16(header.data() + 4);
  const uint16 reserved = binary::ReadU16(header.data() + 6);
  const uint64 payload_size = binary::ReadU64(header.data() + 8);
  const uint32 checksum = binary::ReadU32(header.data() + 16);

  if (version != Version || reserved != 0) {
    return false;
  }

  if (payload_size < PayloadPrefixSize ||
      payload_size > std::numeric_limits<usize>::max() ||
      payload_size >
          static_cast<uint64>(std::numeric_limits<std::streamsize>::max())) {
    return false;
  }

  // Read the fixed payload prefix first so we can validate the declared
  // order count before allocating the rest of the payload.
  std::array<uint8, PayloadPrefixSize> prefix{};

  if (!ReadExact(file, prefix.data(), PayloadPrefixSize)) {
    return false;
  }

  const uint64 next_order_id = binary::ReadU64(prefix.data());
  const uint64 order_count = binary::ReadU64(prefix.data() + 8);

  if (order_count >
      (std::numeric_limits<uint64>::max() - PayloadPrefixSize) / OrderSize) {
    return false;
  }

  const uint64 expected_payload_size =
      PayloadPrefixSize + order_count * OrderSize;

  if (payload_size != expected_payload_size) {
    return false;
  }

  binary::Bytes payload(prefix.begin(), prefix.end());

  const usize remaining_size =
      static_cast<usize>(payload_size - PayloadPrefixSize);

  payload.resize(static_cast<usize>(payload_size));

  if (!ReadExact(file, payload.data() + PayloadPrefixSize, remaining_size)) {
    return false;
  }

  // Reject trailing bytes rather than silently accepting a malformed
  // file.
  char extra_bytes;

  if (file.read(&extra_bytes, 1)) {
    return false;
  }

  binary::Bytes checksum_data(header.begin() + 4, header.begin() + 16);
  checksum_data.insert(checksum_data.end(), payload.begin(), payload.end());

  if (binary::CalculateCrc32(checksum_data.data(), checksum_data.size()) !=
      checksum) {
    return false;
  }

  matching::LimitOrderBookSnapshot snapshot;
  snapshot.next_order_id = next_order_id;
  snapshot.orders.reserve(static_cast<usize>(order_count));

  usize offset = PayloadPrefixSize;

  for (uint64 i = 0; i < order_count; ++i) {
    trading::Order order{};

    order.id = binary::ReadU64(payload.data() + offset);
    offset += 8;

    const uint8 side = payload[offset];
    offset += 1;

    if (side > static_cast<uint8>(trading::Side::Sell)) {
      return false;
    }

    order.side = static_cast<trading::Side>(side);
    order.price = binary::ReadI64(payload.data() + offset);
    offset += 8;

    order.quantity = binary::ReadI64(payload.data() + offset);
    offset += 8;

    order.timestamp = std::chrono::steady_clock::now();

    snapshot.orders.push_back(std::move(order));
  }

  return order_book.RestoreSnapshot(snapshot);
}

}  // namespace recovery
