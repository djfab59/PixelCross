#include "shared.h"
#include "settings.h"

static bool prevExitCombo = HIGH;

void initSettings() {
  prevExitCombo = HIGH;
}

void loopSettings() {
  bool btnG1 = (digitalRead(BTN_GREEN1_PIN) == LOW);
  bool btnG2 = (digitalRead(BTN_GREEN2_PIN) == LOW);
  bool exitCombo = (btnG1 && btnG2);
  
  FastLED.clear();
  drawCenteredString("OPTIONS", 2, CRGB::Cyan); // Placeholder en attendant les vrais réglages
  FastLED.show();

  if (exitCombo && prevExitCombo == HIGH) {
    declencherBip(800, 50);
    currentState = STATE_MENU;
  }
  prevExitCombo = exitCombo;
}