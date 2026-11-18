#include "order_book_recovery.h"

#include <filesystem>

#include "snapshot.h"
#include "wal_replay.h"

namespace recovery {

bool32 RecoverOrderBook(const std::string& snapshot_path,
                        const std::string& wal_path,
                        matching::LimitOrderBook& order_book,
                        WalReplayStats& stats) {
  stats = {};

  uint64 last_sequence_number = 0;

  if (std::filesystem::exists(snapshot_path)) {
    if (!ReadSnapshot(snapshot_path, order_book, last_sequence_number)) {
      return false;
    }
  } else if (!std::filesystem::exists(wal_path)) {
    return true;
  }

  if (!std::filesystem::exists(wal_path)) {
    return last_sequence_number == 0;
  }

  return ReplayWal(wal_path, order_book, stats, last_sequence_number);
}

}  // namespace recovery
