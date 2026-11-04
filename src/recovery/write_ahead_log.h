#pragma once

#include <fstream>
#include <string>

#include "../trading/order.h"
#include "../types.h"

namespace recovery {

enum class WalEventType : uint16 {
  AddOrder = 1,
  CancelOrder = 2,
};

struct WalEvent {
  uint64 sequence_number = 0;
  WalEventType type = WalEventType::AddOrder;

  // AddOrder fields.
  trading::Side side = trading::Side::Buy;
  int64 price = 0;
  int64 quantity = 0;

  // CancelOrder field.
  uint64 order_id = 0;
};

enum class WalReadResult {
  Record,
  End,
  Error,
};

class WalWriter {
 public:
  bool32 Open(const std::string& path);
  void Close();

  bool32 AppendAddOrder(trading::Side side, int64 price, int64 quantity);
  bool32 AppendCancelOrder(uint64 order_id);

 private:
  bool32 Append(const WalEvent& event);

  std::ofstream file_;
  uint64 next_sequence_number_ = 1;
};

class WalReader {
 public:
  bool32 Open(const std::string& path);
  void Close();

  WalReadResult ReadNext(WalEvent& event);

 private:
  std::ifstream file_;
  uint64 expected_sequence_number_ = 1;
};

}  // namespace recovery
