#pragma once

#include <array>

#include "activities/Activity.h"
#include "ChessCore.h"
#include "ChessEngine.h"
#include "ChessGameStore.h"

// A deliberately compact, touch-first chess board for the X4 Pro.  The rules
// and opponent remain independent of the UI in ChessCore/ChessEngine.
class ChessActivity final : public Activity {
 public:
  ChessActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  chess::Position position{};
  chess::MoveList legalMoves{};
  chess::SearchBuffers searchBuffers{};
  std::array<ChessGameStore::Slot, ChessGameStore::SLOT_COUNT> gameSlots{};
  int selectedSquare = -1;
  int activeSlot = -1;
  bool thinking = false;

  void refreshLegalMoves();
  void loadGameSlots();
  void newGame(int slot);
  void openGame(int slot);
  void saveGame();
  void playEngineMove();
  bool moveTo(int from, int to);
  int squareAt(int x, int y) const;
  int slotAt(int x, int y) const;
  int newSlotAt(int x, int y) const;
  void handleSquare(int square);
  void drawGameLibrary();
  void drawBoard();
  const char* status() const;
};
