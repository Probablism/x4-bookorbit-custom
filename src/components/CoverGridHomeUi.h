#pragma once

#include <array>
#include <string>
#include <vector>

#include "HomeCoverCache.h"
#include "RecentBooksStore.h"
#include "UiAppHost.h"
#include "components/bars/tab-bar.h"
#include "components/media/book-card.h"
#include "components/media/cover-grid.h"

class CoverGridHomeUi final : public UiAppHost<16, 1> {
 public:
  using UiScreen = UiAppHost<16, 1>::Screen;
  static constexpr int THUMB_HEIGHT = 400;
  // The X4 launcher keeps the active book prominent and uses the remaining
  // home space for applications. More recent books remain available through
  // Library/BookOrbit rather than competing with the launcher tiles.
  static constexpr int MAX_BOOKS = 1;
  static_assert(MAX_BOOKS <= HomeCoverCache::MAX_COVERS);
  // The tab bar's entries, in display order. OPDS and this fork's BookOrbit catalog only
  // take a slot once they are set up, so positions are resolved through the helpers
  // below rather than counted by hand.
  enum class Tab : uint8_t {
    Files,
    Library,
    Opds,
    BookOrbit,
    Chess,
    ReadingStats,
    Transfer,
    Settings,
    Count
  };
  static constexpr int MAX_TABS = static_cast<int>(Tab::Count);
  static int tabCount(bool hasOpds, bool hasBookOrbit);
  // Tab::Count when the position is outside the bar.
  static Tab tabAt(int position, bool hasOpds, bool hasBookOrbit);
  // -1 when the tab is not shown.
  static int tabPosition(Tab tab, bool hasOpds, bool hasBookOrbit);
  explicit CoverGridHomeUi(GfxRenderer& renderer);
  void begin(const std::vector<RecentBook>& books, bool hasOpds, bool hasBookOrbit, bool hasContinueReading,
             float featuredProgress);
  void refreshCoverPaths();
  void setSelection(int selection) { selected = selection; }
  int selectedAction(const MappedInputManager& input);
  // Exact generation size for a slot, recorded during draw. Rescaling a
  // dithered 1-bit image aliases badly.
  int thumbWidthFor(size_t index) const;
  int thumbHeightFor(size_t index) const;
  bool takeThumbHeightsChanged();

 private:
  static void screenFn(UiScreen& screen, void* user);
  static void onAction(const freeink::ui::ActionEvent& event, void* user);
  void draw(UiScreen& screen);
  void drawHeaderBand();
  void drawEmpty(UiScreen& screen);
  void drawCurrent(UiScreen& screen, freeink::ui::Rect rect, int coverRowHeight);
  void drawLauncher(UiScreen& screen);
  bool paintFramedCover(freeink::ui::DrawTarget& target, freeink::ui::Rect rect, size_t index);
  void refreshCoverPath(size_t index);
  void noteThumbSize(size_t index, int slotWidth, int slotHeight);

  HomeCoverCache coverCache;
  GfxRenderer& renderer;
  const std::vector<RecentBook>* books = nullptr;
  std::array<std::string, MAX_BOOKS> coverPaths;
  std::array<int, MAX_BOOKS> thumbWidths{};
  std::array<int, MAX_BOOKS> thumbHeights{};
  bool thumbHeightsChanged = false;
  int selected = 0;
  int pending = -1;
  int progress = -1;
  bool hasOpds = false;
  bool hasBookOrbit = false;
  char progressText[12]{};
  // Component styles and interaction tables stay off the render task's stack.
  freeink::ui::BookCardProps card;
  std::array<freeink::ui::TabItem, MAX_TABS> tabItems;
};
