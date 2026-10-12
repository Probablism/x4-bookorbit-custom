#include "CoverGridHomeUi.h"

#include <BoardConfig.h>
#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>
#include <utility>

#include "MappedInputManager.h"
#include "UITheme.h"
#include "components/icons/bookOrbitIcons.h"
#include "components/icons/listIcons.h"
#include "components/icons/readingStatsIcons.h"

namespace fui = freeink::ui;
namespace {
constexpr fui::ActionId SELECT = 1;
constexpr int16_t COVER_CELL_INSET = 6;
constexpr int LAUNCHER_COLUMNS = 4;
constexpr int LAUNCHER_GAP = 10;
}  // namespace

CoverGridHomeUi::CoverGridHomeUi(GfxRenderer& renderer)
    : UiAppHost(renderer), coverCache(renderer), renderer(renderer) {}

namespace {
bool showsTab(const CoverGridHomeUi::Tab tab, const bool hasOpds, const bool hasBookOrbit) {
  return (tab != CoverGridHomeUi::Tab::Opds || hasOpds) && (tab != CoverGridHomeUi::Tab::BookOrbit || hasBookOrbit);
}

const char* launcherLabel(const CoverGridHomeUi::Tab tab) {
  switch (tab) {
    case CoverGridHomeUi::Tab::Files:
      return tr(STR_BROWSE_FILES);
    case CoverGridHomeUi::Tab::Library:
      return tr(STR_LIBRARY);
    case CoverGridHomeUi::Tab::Opds:
      return tr(STR_OPDS_BROWSER);
    case CoverGridHomeUi::Tab::BookOrbit:
      return tr(STR_BOOKORBIT);
    case CoverGridHomeUi::Tab::Chess:
      return "Chess";
    case CoverGridHomeUi::Tab::ReadingStats:
      return tr(STR_READING_STATS);
    case CoverGridHomeUi::Tab::Transfer:
      return tr(STR_FILE_TRANSFER);
    case CoverGridHomeUi::Tab::Settings:
      return tr(STR_SETTINGS_TITLE);
    case CoverGridHomeUi::Tab::Count:
      return "";
  }
  return "";
}
}  // namespace

int CoverGridHomeUi::tabCount(const bool hasOpds, const bool hasBookOrbit) {
  return MAX_TABS - (hasOpds ? 0 : 1) - (hasBookOrbit ? 0 : 1);
}

CoverGridHomeUi::Tab CoverGridHomeUi::tabAt(int position, const bool hasOpds, const bool hasBookOrbit) {
  if (position < 0) return Tab::Count;
  for (int i = 0; i < MAX_TABS; ++i) {
    const Tab tab = static_cast<Tab>(i);
    if (!showsTab(tab, hasOpds, hasBookOrbit)) continue;
    if (position-- == 0) return tab;
  }
  return Tab::Count;
}

int CoverGridHomeUi::tabPosition(const Tab tab, const bool hasOpds, const bool hasBookOrbit) {
  if (tab == Tab::Count || !showsTab(tab, hasOpds, hasBookOrbit)) return -1;
  int position = 0;
  for (int i = 0; i < static_cast<int>(tab); ++i) {
    if (showsTab(static_cast<Tab>(i), hasOpds, hasBookOrbit)) ++position;
  }
  return position;
}

void CoverGridHomeUi::begin(const std::vector<RecentBook>& recent, bool opds, bool bookOrbit, bool continuing,
                            float featuredProgress) {
  books = &recent;
  hasOpds = opds;
  hasBookOrbit = bookOrbit;
  if (!recent.empty()) coverCache.begin();
  reset();
  app.on(SELECT, &CoverGridHomeUi::onAction, this);
  app.setScreen(&CoverGridHomeUi::screenFn, this);
  refreshCoverPaths();
  progress = continuing && featuredProgress >= 0 ? static_cast<int>(featuredProgress + 0.5f) : -1;
  if (progress >= 0) snprintf(progressText, sizeof(progressText), "%d%%", progress);
}

void CoverGridHomeUi::refreshCoverPaths() {
  coverCache.invalidate();
  for (size_t i = 0; i < books->size() && i < coverPaths.size(); ++i) refreshCoverPath(i);
}

void CoverGridHomeUi::refreshCoverPath(size_t index) {
  if (index >= books->size() || index >= coverPaths.size()) return;
  coverCache.invalidate(index);
  coverPaths[index] =
      thumbWidths[index] > 0 && thumbHeights[index] > 0
          ? UITheme::getCoverThumbPath((*books)[index].coverBmpPath, thumbWidths[index], thumbHeights[index], false)
          : std::string();
}

int CoverGridHomeUi::thumbWidthFor(size_t index) const {
  return index < thumbWidths.size() && thumbWidths[index] > 0 ? thumbWidths[index] : THUMB_HEIGHT * 2 / 3;
}

int CoverGridHomeUi::thumbHeightFor(size_t index) const {
  return index < thumbHeights.size() && thumbHeights[index] > 0 ? thumbHeights[index] : THUMB_HEIGHT;
}

bool CoverGridHomeUi::takeThumbHeightsChanged() { return std::exchange(thumbHeightsChanged, false); }

void CoverGridHomeUi::noteThumbSize(size_t index, int slotWidth, int slotHeight) {
  if (index >= thumbHeights.size()) return;
  const int width = std::max(1, slotWidth);
  const int height = std::max(1, slotHeight);
  if (thumbWidths[index] != width || thumbHeights[index] != height) {
    thumbWidths[index] = width;
    thumbHeights[index] = height;
    thumbHeightsChanged = true;
    refreshCoverPath(index);
  }
}

void CoverGridHomeUi::onAction(const fui::ActionEvent& event, void* user) {
  auto& self = *static_cast<CoverGridHomeUi*>(user);
  self.pending = event.value;
  self.app.clearTapFlash();
}

int CoverGridHomeUi::selectedAction(const MappedInputManager& input) {
  pending = -1;
  fui::ActionEvent event{};
  return routeTouch(input, event) ? pending : -1;
}

void CoverGridHomeUi::screenFn(UiScreen& screen, void* user) { static_cast<CoverGridHomeUi*>(user)->draw(screen); }

void CoverGridHomeUi::draw(UiScreen& screen) {
  coverCache.prepare();
  const auto& theme = screen.theme();
  const auto safe = UITheme::getInstance().getScreenSafeArea(renderer, !BoardConfig::hasTouch());
  screen.setContentMarginFromScreen(fui::Insets{
      static_cast<int16_t>(safe.y), static_cast<int16_t>(renderer.getScreenWidth() - safe.x - safe.width),
      static_cast<int16_t>(renderer.getScreenHeight() - safe.y - safe.height), static_cast<int16_t>(safe.x)});
  const int16_t hInset = std::max<int16_t>(
      0, static_cast<int16_t>(UITheme::getInstance().getMetrics().headerSidePadding - COVER_CELL_INSET - safe.x));
  const int16_t topInset =
      BoardConfig::hasTouch() ? static_cast<int16_t>(theme.spaceLg) : static_cast<int16_t>(theme.spaceSm + 4);
  screen.insetContent(fui::Insets{topInset, hInset, theme.spaceSm, hInset});
  const bool landscape = renderer.getScreenWidth() > renderer.getScreenHeight();
  screen.takeTop(UITheme::getInstance().getMetrics().batteryBarHeight, theme.spaceSm);
  if (books->empty()) {
    drawLauncher(screen);
    drawEmpty(screen);
    drawHeaderBand();
    return;
  }
  const fui::Rect body = screen.body();
  const int coverRowHeight = std::max(1, body.height / 2);
  const int16_t featuredHeight = std::min<int>(
      body.height / 2, std::max<int>(coverRowHeight / 2,
                                     screen.target().lineHeight(theme.bodyText.font) * (landscape ? 1 : 2) +
                                         screen.target().lineHeight(theme.smallText.font) * 2 + 32));
  const auto featuredRect = screen.takeTop(featuredHeight, theme.spaceSm);
  drawCurrent(screen, featuredRect, coverRowHeight);
  drawLauncher(screen);
  drawHeaderBand();
}

void CoverGridHomeUi::drawHeaderBand() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, renderer.getScreenWidth(), metrics.homeTopPadding}, nullptr);
}

void CoverGridHomeUi::drawEmpty(UiScreen& screen) {
  const auto& theme = screen.theme();
  const auto body = screen.body();
  auto title = theme.titleText;
  title.bold = true;
  title.align = fui::TextAlign::Center;
  auto message = theme.bodyText;
  message.align = fui::TextAlign::Center;
  constexpr int16_t ICON_SIZE = 32;
  const int16_t titleHeight = screen.target().lineHeight(title.font);
  const int16_t messageHeight = screen.target().lineHeight(message.font);
  const int16_t contentHeight = ICON_SIZE + theme.spaceLg + titleHeight + theme.spaceSm + messageHeight;
  int16_t y = body.y + std::max(0, (body.height - contentHeight) / 2);
  drawLucideIcon(renderer, icon_book_32, body.x + (body.width - ICON_SIZE) / 2, y);
  y += ICON_SIZE + theme.spaceLg;
  screen.target().text(fui::Rect{body.x, y, body.width, titleHeight}, tr(STR_NO_OPEN_BOOK), title);
  y += titleHeight + theme.spaceSm;
  screen.target().text(fui::Rect{body.x, y, body.width, messageHeight}, tr(STR_START_READING), message);
}

void CoverGridHomeUi::drawCurrent(UiScreen& screen, fui::Rect rect, const int coverRowHeight) {
  const auto& theme = screen.theme();
  const auto& book = books->front();
  card.title = book.title.c_str();
  card.author = book.author.empty() ? nullptr : book.author.c_str();
  card.meta = nullptr;
  card.progressLabel = progress >= 0 ? progressText : nullptr;
  card.centerTextOnCover = true;
  card.progress = std::max(0, progress);
  card.progressMax = progress >= 0 ? 100 : 0;
  card.action = SELECT;
  card.state = fui::StateNormal;
  card.styles = theme.listRow;
  card.styles.selected.background = fui::Paint::dither(fui::Color::LightGray);
  card.styles.selected.border = fui::Paint::dither(fui::Color::LightGray);
  card.styles.selected.foreground = fui::Paint::solid(fui::Color::Black);
  card.styles.selected.radius = theme.listRowRadius;
  card.styles.active = card.styles.selected;
  card.titleText = theme.bodyText;
  card.titleText.maxLines = renderer.getScreenWidth() > renderer.getScreenHeight() ? 1 : 2;
  card.authorText = theme.smallText;
  card.progressText = theme.smallText;
  card.progressHeight = 6;
  card.padding = fui::Insets{6, 6, 6, 6};
  card.gap = theme.spaceLg + theme.spaceSm;
  const int maxCoverWidth = rect.width / 4 - 2 * COVER_CELL_INSET;
  card.coverSize.height = std::max(1, std::min(std::min<int>(rect.height, coverRowHeight) - 12, maxCoverWidth * 3 / 2));
  card.coverSize.width = std::max(1, card.coverSize.height * 2 / 3);
  noteThumbSize(0, card.coverSize.width, card.coverSize.height);
  card.coverPainterUserData = this;
  card.coverPainter = [](fui::DrawTarget& target, fui::Rect cover, const fui::BookCardProps&, void* user) {
    return static_cast<CoverGridHomeUi*>(user)->paintFramedCover(target, cover, 0);
  };
  fui::bookCard(screen.frame(), rect, card);

  if (selected == 0 && !BoardConfig::hasTouch()) {
    const int16_t barHeight = card.coverSize.height;
    screen.target().fill(fui::Rect{static_cast<int16_t>(rect.x - 9),
                                   static_cast<int16_t>(rect.y + (rect.height - barHeight) / 2), 3, barHeight},
                         fui::Paint::dither(fui::Color::LightGray));
  }
}

void CoverGridHomeUi::drawLauncher(UiScreen& screen) {
  // Indexed by Tab.
  static constexpr const freeink::Icon* ICONS[MAX_TABS] = {
      &icon_folder_32, &icon_landmark_32, &icon_lyra_library_32, &icon_bookorbit_32,
      &icon_book_marked_32, &icon_reading_stats_32, &icon_lyra_transfer_32, &icon_lyra_settings_32};
  const int count = tabCount(hasOpds, hasBookOrbit);
  if (count <= 0) return;
  const fui::Rect area = screen.body();
  const int rows = (count + LAUNCHER_COLUMNS - 1) / LAUNCHER_COLUMNS;
  const int cellWidth = std::max(1, (area.width - (LAUNCHER_COLUMNS - 1) * LAUNCHER_GAP) / LAUNCHER_COLUMNS);
  const int cellHeight = std::max(1, (area.height - (rows - 1) * LAUNCHER_GAP) / rows);
  const int tileSide = std::max(1, std::min(cellWidth, cellHeight - 22));
  for (int i = 0; i < count; ++i) {
    const Tab shown = tabAt(i, hasOpds, hasBookOrbit);
    if (shown == Tab::Count) continue;
    const int column = i % LAUNCHER_COLUMNS;
    const int row = i / LAUNCHER_COLUMNS;
    const int cellX = area.x + column * (cellWidth + LAUNCHER_GAP);
    const int cellY = area.y + row * (cellHeight + LAUNCHER_GAP);
    const int x = cellX + (cellWidth - tileSide) / 2;
    const int y = cellY + std::max(0, (cellHeight - tileSide - 16) / 2);
    const bool selectedTile = selected == static_cast<int>(books->size()) + i;
    if (selectedTile) renderer.fillRectDither(x, y, tileSide, tileSide, Color::LightGray);
    renderer.drawRect(x, y, tileSide, tileSide, true);
    constexpr int iconSize = 32;
    drawLucideIcon(renderer, *ICONS[static_cast<int>(shown)], x + (tileSide - iconSize) / 2,
                   y + (tileSide - iconSize) / 2);
    const char* label = launcherLabel(shown);
    const std::string clipped = renderer.truncatedText(UI_10_FONT_ID, label, cellWidth - 4);
    renderer.drawText(UI_10_FONT_ID, cellX + (cellWidth - renderer.getTextWidth(UI_10_FONT_ID, clipped.c_str())) / 2,
                      y + tileSide + 3, clipped.c_str());
    screen.frame().hit(fui::Rect{static_cast<int16_t>(x), static_cast<int16_t>(y), static_cast<int16_t>(tileSide),
                                 static_cast<int16_t>(tileSide + 20)},
                       SELECT, static_cast<int16_t>(books->size() + i), fui::InputTouch);
  }
}

bool CoverGridHomeUi::paintFramedCover(fui::DrawTarget& target, fui::Rect rect, size_t index) {
  constexpr int16_t SHADOW_OFFSET = 2;
  const auto ink = fui::Paint::solid(fui::Color::Black);
  target.fill(fui::Rect{rect.right(), static_cast<int16_t>(rect.y + SHADOW_OFFSET), SHADOW_OFFSET, rect.height}, ink);
  target.fill(fui::Rect{static_cast<int16_t>(rect.x + SHADOW_OFFSET), rect.bottom(), rect.width, SHADOW_OFFSET}, ink);
  const bool drawn = index < coverPaths.size() && coverCache.paint(rect, index, coverPaths[index]);
  target.stroke(rect, ink, 1, 0);
  return drawn;
}
