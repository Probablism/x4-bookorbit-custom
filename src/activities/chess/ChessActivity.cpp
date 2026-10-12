#include "ChessActivity.h"

#include <GfxRenderer.h>
#include <FreeInkApp.h>
#include <I18n.h>

#include "MappedInputManager.h"
#include "components/CompactHeader.h"
#include "components/TouchHeaderBackButton.h"
#include "fontIds.h"

namespace {
constexpr int kBoardMaxSize = 400;

int boardSizeFor(const GfxRenderer& renderer) {
  const int available = std::min(renderer.getScreenWidth() - 64, renderer.getScreenHeight() - 130);
  return std::max(8, std::min(kBoardMaxSize, available) / 8 * 8);
}

char pieceLetter(uint8_t piece) {
  static constexpr char letters[] = {' ', 'P', 'N', 'B', 'R', 'Q', 'K'};
  const uint8_t type = chess::pieceType(piece);
  if (type == chess::Empty || type > chess::King) return ' ';
  char letter = letters[type];
  if (chess::isBlack(piece) && letter >= 'A' && letter <= 'Z') letter = static_cast<char>(letter - 'A' + 'a');
  return letter;
}
}  // namespace

ChessActivity::ChessActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : Activity("Chess", renderer, mappedInput) {}

void ChessActivity::onEnter() {
  Activity::onEnter();
  newGame();
}

void ChessActivity::newGame() {
  chess::setStartPosition(position);
  selectedSquare = -1;
  thinking = false;
  refreshLegalMoves();
  requestUpdate();
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

bool ChessActivity::moveTo(const int from, const int to) {
  for (int i = 0; i < legalMoves.count; ++i) {
    const chess::Move& move = legalMoves.moves[i];
    if (move.from != from || move.to != to) continue;
    chess::Move actual = move;
    // A queen is the conventional and least surprising default on a compact board.
    if ((actual.flags & chess::FlagPromotion) != 0) actual.promotion = chess::Queen;
    chess::Undo undo;
    chess::makeMove(position, actual, undo);
    refreshLegalMoves();
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
  }
  thinking = false;
}

const char* ChessActivity::status() const {
  if (legalMoves.count == 0) return chess::isInCheck(position) ? "CHECKMATE" : "STALEMATE";
  if (thinking) return "THINKING...";
  return position.whiteToMove ? "YOUR MOVE" : "COMPUTER MOVE";
}

void ChessActivity::loop() {
  if (TouchHeaderBackButton::wasTapped(mappedInput, TouchHeaderBackButton::compactHeaderRect(renderer)) ||
      mappedInput.wasReleased(MappedInputManager::Button::Back) || mappedInput.wasHomeGesture()) {
    onGoHome();
    return;
  }
  int x = 0, y = 0;
  if (mappedInput.wasScreenTapped(x, y)) {
    handleSquare(squareAt(x, y));
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm) && legalMoves.count == 0) newGame();
}

void ChessActivity::render(RenderLock&&) {
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
      const char glyph[] = {pieceLetter(position.squares[square]), '\0'};
      if (glyph[0] != ' ') {
        const int textX = x + (squareSize - renderer.getTextWidth(UI_12_FONT_ID, glyph)) / 2;
        renderer.drawText(UI_12_FONT_ID, textX, y + (squareSize - renderer.getLineHeight(UI_12_FONT_ID)) / 2, glyph);
      }
    }
  }
  renderer.drawRect(left, top, boardSize, boardSize, true);
  renderer.drawCenteredText(UI_10_FONT_ID, top + boardSize + 14, status());
  renderer.drawCenteredText(UI_10_FONT_ID, top + boardSize + 30, "Tap piece, then destination");
  renderer.displayBuffer();
}
