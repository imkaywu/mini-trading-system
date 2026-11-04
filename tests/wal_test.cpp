#include <cassert>
#include <filesystem>
#include <fstream>

#include "../src/recovery/write_ahead_log.cpp"

namespace {

void TestWriteAndRead() {
  const std::string path = "wal_test.bin";

  std::filesystem::remove(path);

  {
    recovery::WalWriter writer;

    assert(writer.Open(path));

    assert(writer.AppendAddOrder(trading::Side::Buy, 100, 10));

    assert(writer.AppendCancelOrder(1));

    writer.Close();
  }

  // Reopening must continue the sequence number.
  {
    recovery::WalWriter writer;

    assert(writer.Open(path));

    assert(writer.AppendAddOrder(trading::Side::Sell, 105, 20));

    writer.Close();
  }

  recovery::WalReader reader;

  assert(reader.Open(path));

  recovery::WalEvent event{};

  assert(reader.ReadNext(event) == recovery::WalReadResult::Record);
  assert(event.sequence_number == 1);
  assert(event.type == recovery::WalEventType::AddOrder);
  assert(event.side == trading::Side::Buy);
  assert(event.price == 100);
  assert(event.quantity == 10);

  assert(reader.ReadNext(event) == recovery::WalReadResult::Record);
  assert(event.sequence_number == 2);
  assert(event.type == recovery::WalEventType::CancelOrder);
  assert(event.order_id == 1);

  assert(reader.ReadNext(event) == recovery::WalReadResult::Record);
  assert(event.sequence_number == 3);
  assert(event.type == recovery::WalEventType::AddOrder);
  assert(event.side == trading::Side::Sell);
  assert(event.price == 105);
  assert(event.quantity == 20);

  assert(reader.ReadNext(event) == recovery::WalReadResult::End);

  reader.Close();

  std::filesystem::remove(path);
}

void TestTruncatedRecord() {
  const std::string path = "wal_truncated_test.bin";

  std::filesystem::remove(path);

  {
    recovery::WalWriter writer;
    assert(writer.Open(path));
    assert(writer.AppendAddOrder(trading::Side::Buy, 100, 10));
    writer.Close();
  }

  const auto original_size = std::filesystem::file_size(path);

  std::filesystem::resize_file(path, original_size - 1);

  recovery::WalReader reader;
  assert(reader.Open(path));

  recovery::WalEvent event{};

  assert(reader.ReadNext(event) == recovery::WalReadResult::Error);

  reader.Close();

  std::filesystem::remove(path);
}

void TestChecksumCorruption() {
  const std::string path = "wal_corrupt_test.bin";

  std::filesystem::remove(path);

  {
    recovery::WalWriter writer;
    assert(writer.Open(path));
    assert(writer.AppendAddOrder(trading::Side::Buy, 100, 10));
    writer.Close();
  }

  // Change the final payload byte.
  {
    std::fstream file(path, std::ios::binary | std::ios::in | std::ios::out);

    assert(file.is_open());

    file.seekg(-1, std::ios::end);

    char byte = 0;
    file.read(&byte, 1);

    byte ^= 0x01;

    file.seekp(-1, std::ios::end);
    file.write(&byte, 1);
  }

  recovery::WalReader reader;
  assert(reader.Open(path));

  recovery::WalEvent event{};

  assert(reader.ReadNext(event) == recovery::WalReadResult::Error);

  reader.Close();

  std::filesystem::remove(path);
}

}  // namespace

int main() {
  TestWriteAndRead();
  TestTruncatedRecord();
  TestChecksumCorruption();

  return 0;
}
