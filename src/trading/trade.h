#pragma once

#include "../types.h"

namespace trading {

struct Trade {
  uint64 incoming_order_id;
  uint64 resting_order_id;
  int64 price;
  int64 quantity;
};

}  // namespace trading
