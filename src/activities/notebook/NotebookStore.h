#pragma once

#include <array>
#include <cstdint>

struct NotebookPoint { uint16_t x = 0; uint16_t y = 0; };
inline constexpr uint8_t NOTEBOOK_MAX_POINTS_PER_STROKE = 32;
inline constexpr uint8_t NOTEBOOK_MAX_STROKES_PER_PAGE = 32;
inline constexpr uint8_t NOTEBOOK_MAX_PAGES = 32;
inline constexpr uint8_t NOTEBOOK_MAX_NOTEBOOKS = 16;
inline constexpr uint8_t NOTEBOOK_TITLE_MAX = 24;

struct NotebookStroke {
  uint32_t id = 0;
  uint8_t pointCount = 0;
  std::array<NotebookPoint, NOTEBOOK_MAX_POINTS_PER_STROKE> points{};
};
struct NotebookPage {
  uint16_t pageNumber = 0;
  uint8_t strokeCount = 0;
  std::array<NotebookStroke, NOTEBOOK_MAX_STROKES_PER_PAGE> strokes{};
};
struct NotebookInfo {
  uint32_t id = 0;
  char title[NOTEBOOK_TITLE_MAX]{};
  uint8_t pageCount = 0;
};

// Local-only, fixed-capacity handwriting persistence. No BookOrbit data is
// read or written here. All mutations publish via a .bak-protected rename.
class NotebookStore {
 public:
  static bool list(std::array<NotebookInfo, NOTEBOOK_MAX_NOTEBOOKS>& out, uint8_t& outCount);
  static bool create(const char* title, NotebookInfo& out);
  static bool remove(uint32_t id);
  static bool loadPage(uint32_t id, uint16_t pageNumber, NotebookPage& out);
  static bool appendStroke(uint32_t id, uint16_t pageNumber, const NotebookStroke& stroke);
  static bool eraseLatest(uint32_t id, uint16_t pageNumber);
  static bool clearPage(uint32_t id, uint16_t pageNumber);
  static bool savePage(uint32_t id, const NotebookPage& page);
};
