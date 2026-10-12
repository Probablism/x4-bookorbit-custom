#pragma once

#include "activities/Activity.h"
#include "ChessCore.h"
#include "ChessEngine.h"

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
  int selectedSquare = -1;
  bool thinking = false;

  void refreshLegalMoves();
  void newGame();
  void playEngineMove();
  bool moveTo(int from, int to);
  int squareAt(int x, int y) const;
  void handleSquare(int square);
  const char* status() const;
};
