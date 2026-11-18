#include "write_ahead_log.h"

#include <array>
#include <filesystem>
#include <vector>

#include "binary_utils.h"

namespace recovery {

namespace wal_detail {

constexpr uint32 Magic = 0x314C4157;  // "WAL1" in little-endian.
constexpr uint16 Version = 1;
// Header
//   Magic                  4 bytes
//   Version                2 bytes
//   Event type             2 bytes
//   Next sequence number   8 bytes
//   Payload size           4 bytes
//   CRC32                  4 bytes
constexpr usize HeaderSize = 24;

constexpr usize AddOrderPayloadSize = 17;
constexpr usize CancelOrderPayloadSize = 8;

bool32 ReadExact(std::ifstream& file, uint8* data, usize size) {
  file.read(reinterpret_cast<char*>(data), static_cast<std::streamsize>(size));

  return file.gcount() == static_cast<std::streamsize>(size);
}

bool32 IsValidSide(uint8 side) {
  return side == static_cast<uint8>(trading::Side::Buy) ||
         side == static_cast<uint8>(trading::Side::Sell);
}

}  // namespace wal_detail

// ------------------------------------------------------------
// WAL writer
// ------------------------------------------------------------

bool32 WalWriter::Open(const std::string& path) {
  Close();

  next_sequence_number_ = 1;

  // Scan an existing WAL to continue its sequence numbers.
  if (std::filesystem::exists(path)) {
    WalReader reader;

    if (!reader.Open(path)) {
      return false;
    }

    WalEvent event;

    while (true) {
      const WalReadResult result = reader.ReadNext(event);

      if (result == WalReadResult::End) {
        break;
      }

      if (result == WalReadResult::Error) {
        reader.Close();
        return false;
      }

      next_sequence_number_ = event.sequence_number + 1;
    }

    reader.Close();
  }

  file_.open(path, std::ios::binary | std::ios::app);

  return file_.is_open();
}

void WalWriter::Close() {
  if (file_.is_open()) {
    file_.close();
  }
}

bool32 WalWriter::AppendAddOrder(trading::Side side,
                                 int64 price,
                                 int64 quantity) {
  if (side != trading::Side::Buy && side != trading::Side::Sell) {
    return false;
  }

  WalEvent event{};
  event.type = WalEventType::AddOrder;
  event.side = side;
  event.price = price;
  event.quantity = quantity;

  return Append(event);
}

bool32 WalWriter::AppendCancelOrder(uint64 order_id) {
  WalEvent event{};
  event.type = WalEventType::CancelOrder;
  event.order_id = order_id;

  return Append(event);
}

bool32 WalWriter::Append(const WalEvent& event) {
  using namespace wal_detail;

  if (!file_.is_open()) {
    return false;
  }

  binary::Bytes payload;

  if (event.type == WalEventType::AddOrder) {
    payload.push_back(static_cast<uint8>(event.side));
    binary::WriteU64(payload, static_cast<uint64>(event.price));
    binary::WriteU64(payload, static_cast<uint64>(event.quantity));
  } else if (event.type == WalEventType::CancelOrder) {
    binary::WriteU64(payload, event.order_id);
  } else {
    return false;
  }

  binary::Bytes record;

  binary::WriteU32(record, Magic);
  binary::WriteU16(record, Version);
  binary::WriteU16(record, static_cast<uint16>(event.type));
  binary::WriteU64(record, next_sequence_number_);
  binary::WriteU32(record, static_cast<uint32>(payload.size()));

  // Checksum covers version through payload, excluding magic and
  // the checksum field itself.
  binary::Bytes checksum_data(record.begin() + 4, record.end());
  checksum_data.insert(checksum_data.end(), payload.begin(), payload.end());

  binary::WriteU32(
      record,
      binary::CalculateCrc32(checksum_data.data(), checksum_data.size()));

  record.insert(record.end(), payload.begin(), payload.end());

  file_.write(reinterpret_cast<const char*>(record.data()),
              static_cast<std::streamsize>(record.size()));

  file_.flush();

  if (!file_) {
    return false;
  }

  ++next_sequence_number_;

  return true;
}

// ------------------------------------------------------------
// WAL reader
// ------------------------------------------------------------

bool32 WalReader::Open(const std::string& path) {
  Close();

  file_.open(path, std::ios::binary);
  expected_sequence_number_ = 1;

  return file_.is_open();
}

void WalReader::Close() {
  if (file_.is_open()) {
    file_.close();
  }
}

WalReadResult WalReader::ReadNext(WalEvent& event) {
  using namespace wal_detail;

  if (!file_.is_open()) {
    return WalReadResult::Error;
  }

  std::array<uint8, HeaderSize> header{};

  // Read the first byte separately to distinguish clean EOF from
  // a truncated header.
  file_.read(reinterpret_cast<char*>(header.data()), 1);

  if (file_.gcount() == 0 && file_.eof()) {
    return WalReadResult::End;
  }

  if (!ReadExact(file_, header.data() + 1, HeaderSize - 1)) {
    return WalReadResult::Error;
  }

  const uint32 magic = binary::ReadU32(header.data());
  const uint16 version = binary::ReadU16(header.data() + 4);
  const uint16 event_type = binary::ReadU16(header.data() + 6);
  const uint64 sequence_number = binary::ReadU64(header.data() + 8);
  const uint32 payload_size = binary::ReadU32(header.data() + 16);
  const uint32 checksum = binary::ReadU32(header.data() + 20);

  if (magic != Magic || version != Version) {
    return WalReadResult::Error;
  }

  if (sequence_number != expected_sequence_number_) {
    return WalReadResult::Error;
  }

  if (event_type != static_cast<uint16>(WalEventType::AddOrder) &&
      event_type != static_cast<uint16>(WalEventType::CancelOrder)) {
    return WalReadResult::Error;
  }

  const WalEventType type = static_cast<WalEventType>(event_type);

  const usize expected_payload_size = (type == WalEventType::AddOrder)
                                          ? AddOrderPayloadSize
                                          : CancelOrderPayloadSize;

  if (payload_size != expected_payload_size) {
    return WalReadResult::Error;
  }

  binary::Bytes payload(payload_size);

  if (!ReadExact(file_, payload.data(), payload.size())) {
    return WalReadResult::Error;
  }

  binary::Bytes checksum_data(header.begin() + 4, header.begin() + 20);
  checksum_data.insert(checksum_data.end(), payload.begin(), payload.end());

  if (binary::CalculateCrc32(checksum_data.data(), checksum_data.size()) !=
      checksum) {
    return WalReadResult::Error;
  }

  event = {};
  event.sequence_number = sequence_number;
  event.type = type;

  if (type == WalEventType::AddOrder) {
    if (!IsValidSide(payload[0])) {
      return WalReadResult::Error;
    }

    event.side = static_cast<trading::Side>(payload[0]);
    event.price = static_cast<int64>(binary::ReadU64(payload.data() + 1));
    event.quantity = static_cast<int64>(binary::ReadU64(payload.data() + 9));
  } else {
    event.order_id = binary::ReadU64(payload.data());
  }

  ++expected_sequence_number_;

  return WalReadResult::Record;
}

}  // namespace recovery
