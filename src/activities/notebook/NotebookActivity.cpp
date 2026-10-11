#include "NotebookActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "components/TouchHeaderBackButton.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr int kHeaderHeight = 42;
constexpr int kFooterHeight = 38;
constexpr unsigned long kDestructiveHoldMs = 900;
bool inRect(const Rect& r, int x, int y) { return x >= r.x && y >= r.y && x < r.x + r.width && y < r.y + r.height; }
}

NotebookActivity::NotebookActivity(GfxRenderer& r, MappedInputManager& input) : Activity("Notebook", r, input) {}

void NotebookActivity::onEnter() { Activity::onEnter(); reloadList(); requestUpdate(); }
void NotebookActivity::onExit() { drawing = false; Activity::onExit(); }

void NotebookActivity::reloadList() {
  if (!NotebookStore::list(notebooks, notebookCount)) {
    notebookCount = 0;
    LOG_ERR("NB", "Unable to load notebook index");
  }
  if (notebookCount == 0) selectedNotebook = 0;
  else if (selectedNotebook >= notebookCount) selectedNotebook = notebookCount - 1;
}

bool NotebookActivity::createNotebook() {
  char title[NOTEBOOK_TITLE_MAX];
  std::snprintf(title, sizeof(title), "%s %u", tr(STR_NOTEBOOK), static_cast<unsigned>(notebookCount + 1));
  NotebookInfo created{};
  if (!NotebookStore::create(title, created)) return false;
  reloadList(); selectedNotebook = notebookCount ? notebookCount - 1 : 0; return true;
}

bool NotebookActivity::deleteSelected() {
  if (selectedNotebook >= notebookCount || !NotebookStore::remove(notebooks[selectedNotebook].id)) return false;
  reloadList(); return true;
}

bool NotebookActivity::openSelected() {
  if (selectedNotebook >= notebookCount) return false;
  openNotebook = notebooks[selectedNotebook]; pageNumber = 0;
  if (!loadPage(pageNumber)) return false;
  view = View::Page; return true;
}

bool NotebookActivity::loadPage(uint16_t number) {
  if (number >= NOTEBOOK_MAX_PAGES || !NotebookStore::loadPage(openNotebook.id, number, page)) return false;
  pageNumber = number; pageDirty = false; return true;
}

bool NotebookActivity::persistPage() {
  if (!pageDirty) return true;
  if (!NotebookStore::savePage(openNotebook.id, page)) return false;
  pageDirty = false; return true;
}

bool NotebookActivity::inCanvas(int x, int y) const {
  return x >= 0 && x < renderer.getScreenWidth() && y >= kHeaderHeight && y < renderer.getScreenHeight() - kFooterHeight;
}

NotebookPoint NotebookActivity::normalizedPoint(int x, int y) const {
  const int w = std::max(1, renderer.getScreenWidth() - 1);
  const int top = kHeaderHeight;
  const int h = std::max(1, renderer.getScreenHeight() - kHeaderHeight - kFooterHeight - 1);
  return {static_cast<uint16_t>((std::clamp(x, 0, w) * 1000) / w),
          static_cast<uint16_t>(((std::clamp(y, top, top + h) - top) * 1000) / h)};
}

bool NotebookActivity::handleDrawingTouch() {
  int x = 0, y = 0;
  if (!drawing && mappedInput.wasScreenTouchDown(x, y)) {
    if (!inCanvas(x, y) || page.strokeCount >= NOTEBOOK_MAX_STROKES_PER_PAGE) return false;
    pendingStroke = {}; pendingStroke.pointCount = 1; pendingStroke.points[0] = normalizedPoint(x, y);
    drawing = true; strokeMoved = false; return true;
  }
  if (!drawing) return false;
  if (mappedInput.isScreenTouchHeld(x, y)) {
    if (!inCanvas(x, y)) return true;
    const NotebookPoint last = pendingStroke.points[pendingStroke.pointCount - 1];
    const int px = (last.x * std::max(1, renderer.getScreenWidth() - 1)) / 1000;
    const int py = kHeaderHeight + (last.y * std::max(1, renderer.getScreenHeight() - kHeaderHeight - kFooterHeight - 1)) / 1000;
    if (std::abs(x - px) >= kPointSpacingPx || std::abs(y - py) >= kPointSpacingPx) {
      strokeMoved = true; mappedInput.suppressNextTouchTap();
      if (pendingStroke.pointCount < NOTEBOOK_MAX_POINTS_PER_STROKE) pendingStroke.points[pendingStroke.pointCount++] = normalizedPoint(x, y);
    }
    return true;
  }
  if (!mappedInput.wasScreenTouchReleased()) return true;
  drawing = false;
  if (!strokeMoved || pendingStroke.pointCount < 2) return true;
  if (!NotebookStore::appendStroke(openNotebook.id, pageNumber, pendingStroke)) { LOG_ERR("NB", "Unable to save stroke"); return true; }
  if (page.strokeCount < NOTEBOOK_MAX_STROKES_PER_PAGE) page.strokes[page.strokeCount++] = pendingStroke;
  pageDirty = false; requestUpdate(); return true;
}

void NotebookActivity::undoStroke() {
  if (!page.strokeCount) return;
  if (NotebookStore::eraseLatest(openNotebook.id, pageNumber)) { --page.strokeCount; requestUpdate(); }
}
void NotebookActivity::clearPage() {
  if (NotebookStore::clearPage(openNotebook.id, pageNumber)) { page = {}; page.pageNumber = pageNumber; requestUpdate(); }
}

void NotebookActivity::handleListInput() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) { finish(); return; }
  if (mappedInput.wasReleased(MappedInputManager::Button::Power)) { if (createNotebook()) requestUpdate(); return; }
  if (mappedInput.isPressed(MappedInputManager::Button::Confirm) && mappedInput.getHeldTime() >= kDestructiveHoldMs) {
    mappedInput.suppressNextConfirmRelease(); if (deleteSelected()) requestUpdate(); return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) { if (openSelected()) requestUpdate(); return; }
  if (mappedInput.wasPressed(MappedInputManager::Button::Up) && selectedNotebook) { --selectedNotebook; requestUpdate(); }
  if (mappedInput.wasPressed(MappedInputManager::Button::Down) && selectedNotebook + 1 < notebookCount) { ++selectedNotebook; requestUpdate(); }
  int x=0,y=0; if (mappedInput.wasScreenTapped(x,y)) {
    if (y > renderer.getScreenHeight() - kFooterHeight) { if(createNotebook())requestUpdate(); return; }
    const int row=std::max(0,(y-kHeaderHeight)/44); if(row<notebookCount){selectedNotebook=row; if(openSelected())requestUpdate();}
  }
}

void NotebookActivity::handlePageInput() {
  if (handleDrawingTouch()) return;
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) { persistPage(); view=View::List; reloadList(); requestUpdate(); return; }
  if (mappedInput.wasReleased(MappedInputManager::Button::Left) && pageNumber) { persistPage(); loadPage(pageNumber-1); requestUpdate(); return; }
  if (mappedInput.wasReleased(MappedInputManager::Button::Right) && pageNumber < kMaxPageIndex) { persistPage(); loadPage(pageNumber+1); requestUpdate(); return; }
  if (mappedInput.wasReleased(MappedInputManager::Button::Power)) { undoStroke(); return; }
  if (mappedInput.isPressed(MappedInputManager::Button::Confirm) && mappedInput.getHeldTime() >= kDestructiveHoldMs) { mappedInput.suppressNextConfirmRelease(); clearPage(); return; }
  int x=0,y=0; if (mappedInput.wasScreenTapped(x,y)) {
    if (y < kHeaderHeight) { persistPage(); view=View::List; reloadList(); requestUpdate(); }
    else if (y >= renderer.getScreenHeight()-kFooterHeight) { if(x < renderer.getScreenWidth()/3 && pageNumber){persistPage();loadPage(pageNumber-1);requestUpdate();} else if(x < renderer.getScreenWidth()*2/3)undoStroke(); else if(pageNumber<kMaxPageIndex){persistPage();loadPage(pageNumber+1);requestUpdate();} }
  }
}
void NotebookActivity::loop() { if(view==View::List) handleListInput(); else handlePageInput(); }

void NotebookActivity::drawStroke(const NotebookStroke& s) const {
  if (s.pointCount < 2) return; const int w=std::max(1,renderer.getScreenWidth()-1); const int h=std::max(1,renderer.getScreenHeight()-kHeaderHeight-kFooterHeight-1);
  for(uint8_t i=1;i<s.pointCount;i++) renderer.drawLine((s.points[i-1].x*w)/1000,kHeaderHeight+(s.points[i-1].y*h)/1000,(s.points[i].x*w)/1000,kHeaderHeight+(s.points[i].y*h)/1000,true);
}
void NotebookActivity::drawChrome(const char* title,const char* left,const char* right) const {
  const Rect header{0,0,renderer.getScreenWidth(),kHeaderHeight}; GUI.drawHeader(renderer,header,title,nullptr);
  const auto labels=mappedInput.mapLabels(mappedInput.withBackArrow(left),"",right,""); GUI.drawButtonHints(renderer,labels.btn1,labels.btn2,labels.btn3,labels.btn4,true);
}
void NotebookActivity::drawList() {
  renderer.clearScreen(); drawChrome(tr(STR_NOTEBOOK),tr(STR_BACK),tr(STR_NEW));
  if(!notebookCount) renderer.drawCenteredText(UI_12_FONT_ID,renderer.getScreenHeight()/2,tr(STR_NO_FILES_FOUND));
  for(uint8_t i=0;i<notebookCount;i++){const int y=kHeaderHeight+i*44; if(i==selectedNotebook) renderer.invertRect(8,y,renderer.getScreenWidth()-16,40);renderer.drawText(UI_12_FONT_ID,18,y+12,notebooks[i].title);}
  renderer.drawCenteredText(UI_10_FONT_ID,renderer.getScreenHeight()-kFooterHeight/2,tr(STR_NEW)); renderer.displayBuffer();
}
void NotebookActivity::drawPage() {
  renderer.clearScreen(); char label[48]; std::snprintf(label,sizeof(label),"%s  %u",openNotebook.title,static_cast<unsigned>(pageNumber+1)); drawChrome(label,tr(STR_BACK),tr(STR_UNDO));
  for(uint8_t i=0;i<page.strokeCount;i++) drawStroke(page.strokes[i]); if(drawing)drawStroke(pendingStroke);
  renderer.drawLine(0,renderer.getScreenHeight()-kFooterHeight,renderer.getScreenWidth()-1,renderer.getScreenHeight()-kFooterHeight,true);
  renderer.drawText(UI_10_FONT_ID,12,renderer.getScreenHeight()-24,"<"); renderer.drawCenteredText(UI_10_FONT_ID,renderer.getScreenHeight()-24,tr(STR_UNDO)); renderer.drawText(UI_10_FONT_ID,renderer.getScreenWidth()-24,renderer.getScreenHeight()-24,">"); renderer.displayBuffer();
}
void NotebookActivity::render(RenderLock&&) { if(view==View::List)drawList();else drawPage(); }
