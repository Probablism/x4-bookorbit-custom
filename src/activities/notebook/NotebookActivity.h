#pragma once

#include <array>
#include <cstdint>

#include "activities/Activity.h"
#include "activities/notebook/NotebookStore.h"

// X4 Pro-only, local handwriting notebooks.  Pages are deliberately bounded
// and stored independently; BookOrbit only models reading annotations and is
// never contacted from this activity.
class NotebookActivity final : public Activity {
 public:
  explicit NotebookActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool preventAutoSleep() override { return drawing; }

 private:
  enum class View : uint8_t { List, Page };
  static constexpr uint16_t kMaxPageIndex = NOTEBOOK_MAX_PAGES - 1;
  static constexpr int kPointSpacingPx = 4;

  View view = View::List;
  std::array<NotebookInfo, NOTEBOOK_MAX_NOTEBOOKS> notebooks{};
  uint8_t notebookCount = 0;
  uint8_t selectedNotebook = 0;
  NotebookInfo openNotebook{};
  uint16_t pageNumber = 0;
  NotebookPage page{};
  NotebookStroke pendingStroke{};
  bool drawing = false;
  bool strokeMoved = false;
  bool pageDirty = false;

  void reloadList();
  bool openSelected();
  bool createNotebook();
  bool deleteSelected();
  bool loadPage(uint16_t number);
  bool persistPage();
  void undoStroke();
  void clearPage();
  void handleListInput();
  void handlePageInput();
  bool handleDrawingTouch();
  bool inCanvas(int x, int y) const;
  NotebookPoint normalizedPoint(int x, int y) const;
  void drawList();
  void drawPage();
  void drawStroke(const NotebookStroke& stroke) const;
  void drawChrome(const char* title, const char* left, const char* right) const;
};
