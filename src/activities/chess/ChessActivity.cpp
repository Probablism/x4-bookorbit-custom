#include "ChessActivity.h"

#include <GfxRenderer.h>
#include <FreeInkApp.h>
#include <I18n.h>

#include <algorithm>

#include "MappedInputManager.h"
#include "components/CompactHeader.h"
#include "components/TouchHeaderBackButton.h"
#include "fontIds.h"

namespace {
constexpr int kBoardMaxSize = 400;
constexpr int kLibraryTop = 92;
constexpr int kLibraryRowHeight = 62;

int boardSizeFor(const GfxRenderer& renderer) {
  const int available = std::min(renderer.getScreenWidth() - 64, renderer.getScreenHeight() - 130);
  return std::max(8, std::min(kBoardMaxSize, available) / 8 * 8);
}

char pieceLetter(const uint8_t piece) {
  static constexpr char letters[] = {' ', 'P', 'N', 'B', 'R', 'Q', 'K'};
  const uint8_t type = chess::pieceType(piece);
  return type == chess::Empty || type > chess::King ? ' ' : letters[type];
}
}  // namespace

ChessActivity::ChessActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : Activity("Chess", renderer, mappedInput) {}

void ChessActivity::onEnter() {
  Activity::onEnter();
  loadGameSlots();
  activeSlot = -1;
  selectedSquare = -1;
  thinking = false;
  requestUpdate();
}

void ChessActivity::loadGameSlots() { ChessGameStore::loadAll(gameSlots); }

void ChessActivity::newGame(const int slot) {
  if (slot < 0 || slot >= ChessGameStore::SLOT_COUNT) return;
  activeSlot = slot;
  chess::setStartPosition(position);
  selectedSquare = -1;
  thinking = false;
  refreshLegalMoves();
  saveGame();
  requestUpdate();
}

void ChessActivity::openGame(const int slot) {
  if (slot < 0 || slot >= ChessGameStore::SLOT_COUNT) return;
  if (!gameSlots[slot].occupied) {
    newGame(slot);
    return;
  }
  activeSlot = slot;
  position = gameSlots[slot].position;
  selectedSquare = -1;
  thinking = false;
  refreshLegalMoves();
  requestUpdate();
}

void ChessActivity::saveGame() {
  if (activeSlot < 0 || activeSlot >= ChessGameStore::SLOT_COUNT) return;
  if (ChessGameStore::save(static_cast<uint8_t>(activeSlot), position)) loadGameSlots();
}

void ChessActivity::refreshLegalMoves() { chess::generateLegalMoves(position, legalMoves); }

int ChessActivity::squareAt(const int x, const int y) const {
  const int boardSize = boardSizeFor(renderer);
  const int squareSize = boardSize / 8;
  const int left = (renderer.getScreenWidth() - boardSize) / 2;
  const int top = 54 + std::max(0, (renderer.getScreenHeight() - 54 - 52 - boardSize) / 2);
  if (x < left || x >= left + boardSize || y < top || y >= top + boardSize) return -1;
  const int file = (x - left) / squareSize;
  const int rank = 7 - (y - top) / squareSize;
  return rank * 8 + file;
}

int ChessActivity::slotAt(const int x, const int y) const {
  if (x < 42 || x >= renderer.getScreenWidth() - 42 || y < kLibraryTop ||
      y >= kLibraryTop + kLibraryRowHeight * ChessGameStore::SLOT_COUNT)
    return -1;
  return (y - kLibraryTop) / kLibraryRowHeight;
}

int ChessActivity::newSlotAt(const int x, const int y) const {
  const int slot = slotAt(x, y);
  return slot >= 0 && x >= renderer.getScreenWidth() - 178 ? slot : -1;
}

bool ChessActivity::moveTo(const int from, const int to) {
  for (int i = 0; i < legalMoves.count; ++i) {
    const chess::Move& move = legalMoves.moves[i];
    if (move.from != from || move.to != to) continue;
    chess::Move actual = move;
    if ((actual.flags & chess::FlagPromotion) != 0) actual.promotion = chess::Queen;
    chess::Undo undo;
    chess::makeMove(position, actual, undo);
    refreshLegalMoves();
    saveGame();
    return true;
  }
  return false;
}

void ChessActivity::handleSquare(const int square) {
  if (square < 0 || thinking || legalMoves.count == 0) return;
  const uint8_t piece = position.squares[square];
  if (selectedSquare < 0) {
    if (chess::isColour(piece, position.whiteToMove)) selectedSquare = square;
  } else if (!moveTo(selectedSquare, square)) {
    selectedSquare = chess::isColour(piece, position.whiteToMove) ? square : -1;
  } else {
    selectedSquare = -1;
    if (legalMoves.count != 0) playEngineMove();
  }
  requestUpdate();
}

void ChessActivity::playEngineMove() {
  thinking = true;
  requestUpdate();
  chess::Position scratch = position;
  const chess::SearchResult result = chess::search(scratch, 3, searchBuffers, 25000);
  if (result.hasMove) {
    chess::Undo undo;
    chess::makeMove(position, result.best, undo);
    refreshLegalMoves();
    saveGame();
  }
  thinking = false;
}

const char* ChessActivity::status() const {
  if (legalMoves.count == 0) return chess::isInCheck(position) ? "CHECKMATE" : "STALEMATE";
  if (thinking) return "THINKING...";
  return position.whiteToMove ? "YOUR MOVE — WHITE" : "COMPUTER MOVE — BLACK";
}

void ChessActivity::loop() {
  if (TouchHeaderBackButton::wasTapped(mappedInput, TouchHeaderBackButton::compactHeaderRect(renderer)) ||
      mappedInput.wasReleased(MappedInputManager::Button::Back) || mappedInput.wasHomeGesture()) {
    if (activeSlot >= 0) {
      saveGame();
      activeSlot = -1;
      loadGameSlots();
      requestUpdate();
    } else {
      onGoHome();
    }
    return;
  }
  int x = 0, y = 0;
  if (!mappedInput.wasScreenTapped(x, y)) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm) && activeSlot >= 0 && legalMoves.count == 0)
      newGame(activeSlot);
    return;
  }
  if (activeSlot < 0) {
    const int reset = newSlotAt(x, y);
    if (reset >= 0)
      newGame(reset);
    else {
      const int slot = slotAt(x, y);
      if (slot >= 0) openGame(slot);
    }
    return;
  }
  const int boardSize = boardSizeFor(renderer);
  const int boardTop = 54 + std::max(0, (renderer.getScreenHeight() - 54 - 52 - boardSize) / 2);
  if (y >= boardTop + boardSize + 4) {
    saveGame();
    activeSlot = -1;
    loadGameSlots();
    requestUpdate();
    return;
  }
  handleSquare(squareAt(x, y));
}

void ChessActivity::drawGameLibrary() {
  renderer.clearScreen();
  if (mappedInput.hasTouchHardware())
    TouchHeaderBackButton::drawCompact(renderer, "CHESS GAMES");
  else
    CompactHeader::drawTitle(renderer, "CHESS GAMES");
  renderer.drawCenteredText(UI_10_FONT_ID, 67, "Resume a game or start a new one");
  for (int i = 0; i < ChessGameStore::SLOT_COUNT; ++i) {
    const int y = kLibraryTop + i * kLibraryRowHeight;
    renderer.drawRoundedRect(42, y, renderer.getScreenWidth() - 84, kLibraryRowHeight - 8, 1, 2, true);
    renderer.drawText(UI_12_FONT_ID, 58, y + 12, gameSlots[i].name.data());
    renderer.drawText(UI_10_FONT_ID, 58, y + 31, gameSlots[i].occupied ? "IN PROGRESS — TAP TO RESUME" : "EMPTY — TAP TO START");
    const int buttonX = renderer.getScreenWidth() - 172;
    renderer.fillRoundedRect(buttonX, y + 10, 114, 30, 3, Color::Black);
    renderer.drawText(UI_10_FONT_ID, buttonX + 18, y + 19, gameSlots[i].occupied ? "NEW" : "START", false);
  }
  renderer.drawCenteredText(UI_10_FONT_ID, kLibraryTop + kLibraryRowHeight * ChessGameStore::SLOT_COUNT + 12,
                            "NEW replaces only that game slot");
  renderer.displayBuffer();
}

void ChessActivity::drawBoard() {
  renderer.clearScreen();
  if (mappedInput.hasTouchHardware())
    TouchHeaderBackButton::drawCompact(renderer, "CHESS");
  else
    CompactHeader::drawTitle(renderer, "CHESS");
  const int boardSize = boardSizeFor(renderer);
  const int squareSize = boardSize / 8;
  const int left = (renderer.getScreenWidth() - boardSize) / 2;
  const int top = 54 + std::max(0, (renderer.getScreenHeight() - 54 - 52 - boardSize) / 2);
  for (int displayRank = 0; displayRank < 8; ++displayRank) {
    for (int file = 0; file < 8; ++file) {
      const int square = (7 - displayRank) * 8 + file;
      const int x = left + file * squareSize, y = top + displayRank * squareSize;
      const bool dark = ((file + displayRank) & 1) != 0;
      if (dark) renderer.fillRectDither(x, y, squareSize, squareSize, Color::LightGray);
      if (square == selectedSquare) renderer.drawRect(x + 2, y + 2, squareSize - 4, squareSize - 4, true);
      const uint8_t piece = position.squares[square];
      const char glyph[] = {pieceLetter(piece), '\0'};
      if (glyph[0] == ' ') continue;
      const int pieceSize = std::max(22, squareSize - 18);
      const int pieceX = x + (squareSize - pieceSize) / 2;
      const int pieceY = y + (squareSize - pieceSize) / 2;
      const bool black = chess::isBlack(piece);
      renderer.fillRoundedRect(pieceX, pieceY, pieceSize, pieceSize, 4, black ? Color::Black : Color::White);
      renderer.drawRoundedRect(pieceX, pieceY, pieceSize, pieceSize, 1, 4, true);
      const int textX = x + (squareSize - renderer.getTextWidth(BITTER_16_FONT_ID, glyph)) / 2;
      renderer.drawText(BITTER_16_FONT_ID, textX, y + (squareSize - renderer.getLineHeight(BITTER_16_FONT_ID)) / 2,
                        glyph, !black, EpdFontFamily::BOLD);
    }
  }
  renderer.drawRect(left, top, boardSize, boardSize, true);
  renderer.drawCenteredText(UI_10_FONT_ID, top + boardSize + 14, status());
  renderer.drawCenteredText(UI_10_FONT_ID, top + boardSize + 30, "White outlined · Black solid · Tap below for games");
  renderer.displayBuffer();
}

void ChessActivity::render(RenderLock&&) {
  if (activeSlot < 0)
    drawGameLibrary();
  else
    drawBoard();
}
