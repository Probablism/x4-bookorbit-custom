#pragma once

#include <array>
#include <cstdint>
#include <string>

// Local-only, freehand marks for the reader's reserved outer gutters.  These
// are intentionally not BookOrbit annotations: BookOrbit annotations are
// text/xpointer based and cannot represent a reflow-safe pen stroke.
struct MarginStrokePoint {
  uint16_t x = 0;  // 0..1000 normalized logical-screen coordinate
  uint16_t y = 0;
};

struct MarginStrokeKey {
  uint16_t spineIndex = 0;
  uint16_t pageIndex = 0;
  uint32_t layoutSignature = 0;
};

inline constexpr uint8_t MARGIN_STROKE_MAX_POINTS = 64;
inline constexpr uint8_t MARGIN_STROKE_MAX_PER_PAGE = 24;
inline constexpr uint16_t MARGIN_STROKE_MAX_PER_BOOK = 128;

struct MarginStroke {
  MarginStrokeKey key{};
  uint32_t id = 0;
  uint8_t pointCount = 0;
  std::array<MarginStrokePoint, MARGIN_STROKE_MAX_POINTS> points{};
};

// Fixed-record SD store. Page callers provide fixed arrays, so reading and
// rendering annotations never allocates and the reader keeps only one page in RAM.
class MarginStrokeStore {
 public:
  static bool loadPage(const std::string& stateDir, const MarginStrokeKey& key,
                       std::array<MarginStroke, MARGIN_STROKE_MAX_PER_PAGE>& out, uint8_t& outCount);
  static bool append(const std::string& stateDir, const MarginStroke& stroke);
  static bool eraseLatest(const std::string& stateDir, const MarginStrokeKey& key);
  static bool clearPage(const std::string& stateDir, const MarginStrokeKey& key);

 private:
  static bool rewrite(const std::string& stateDir, const MarginStroke* appendStroke, const MarginStrokeKey* removeKey,
                      bool eraseLatestOnly);
};
