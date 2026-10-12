#pragma once

#include <array>
#include <cstdint>

#include "ChessCore.h"

// Small local save bank for the on-device chess app. Fixed slots keep memory
// and SD churn modest while allowing several unfinished games at once.
class ChessGameStore {
 public:
  static constexpr uint8_t SLOT_COUNT = 4;
  static constexpr uint8_t NAME_SIZE = 16;

  struct Slot {
    bool occupied = false;
    std::array<char, NAME_SIZE + 1> name{};
    chess::Position position{};
  };

  static bool loadAll(std::array<Slot, SLOT_COUNT>& slots);
  static bool save(uint8_t slot, const chess::Position& position);
  static void defaultName(uint8_t slot, std::array<char, NAME_SIZE + 1>& out);
};
