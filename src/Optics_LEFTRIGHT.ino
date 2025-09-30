#include <Arduino.h>

// === Pin Definitions ===
const int DIR_PIN = 4;
const int STEP_PIN = 1;
const int EN_PIN = 0;
const int LEFT_BUTTON_PIN = 3;
const int RIGHT_BUTTON_PIN = 2;

// === Motor & Control Settings ===
const int stepDelayMicroseconds = 400;      // Smaller value = faster motor.
const unsigned long saveHoldTime = 2000;    // Hold BOTH buttons for 2s to save.
const unsigned long doubleClickTime = 500;  // Max time in ms between two clicks.

// === State Machine Enum ===
enum MotorState {
  IDLE,
  MANUAL_JOG,
  MOVING_TO_TARGET
};
MotorState currentState = IDLE;

// === Position Variables ===
volatile long currentPosition = 0;
long targetPosition = 0;
long savedPosition = 1000; // Default saved position

// === Button State Variables ===
// For detecting actions with both buttons
unsigned long bothButtonsPressTime = 0;
bool bothButtonsWerePressed = false;
int bothClickCount = 0;
unsigned long lastBothClickTime = 0;

// ===================================================================
// SETUP: Runs once at the beginning
// ===================================================================
void setup() {
  Serial.begin(115200);
  Serial.println("\nESP32-C3 Stepper Control - Final Version");
  Serial.println("----------------------------------------------");
  Serial.println("- Hold L/R to jog motor.");
  Serial.println("- Hold BOTH (2s) to save position.");
  Serial.println("- Double-press BOTH to go to saved position.");
  Serial.println("----------------------------------------------");

  // Configure motor control pins
  pinMode(EN_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);
  pinMode(STEP_PIN, OUTPUT);
  digitalWrite(EN_PIN, HIGH); // Disable motor initially

  // Configure button pins with internal pull-up resistors
  pinMode(LEFT_BUTTON_PIN, INPUT_PULLUP);
  pinMode(RIGHT_BUTTON_PIN, INPUT_PULLUP);
}

// ===================================================================
// LOOP: The main program that runs continuously
// ===================================================================
void loop() {
  handleButtons();

  if (currentState == MOVING_TO_TARGET) {
    moveToTarget();
  } else if (currentState == IDLE) {
    digitalWrite(EN_PIN, HIGH);
  }
}

// ===================================================================
// HELPER FUNCTIONS
// ===================================================================

/**
 * @brief Generates a single step pulse and updates the current position.
 */
void takeStep(int dir, int stepDelay) {
  digitalWrite(EN_PIN, LOW);
  digitalWrite(DIR_PIN, dir);
  digitalWrite(STEP_PIN, HIGH);
  delayMicroseconds(5);
  digitalWrite(STEP_PIN, LOW);
  if (dir == HIGH) { currentPosition++; } else { currentPosition--; }
  delayMicroseconds(stepDelay);
}

/**
 * @brief Moves the motor one step at a time towards the targetPosition.
 */
void moveToTarget() {
  if (currentPosition == targetPosition) {
    Serial.print("Target reached at position: ");
    Serial.println(currentPosition);
    currentState = IDLE;
    return;
  }
  if (currentPosition < targetPosition) {
    takeStep(HIGH, stepDelayMicroseconds);
  } else {
    takeStep(LOW, stepDelayMicroseconds);
  }
}

/**
 * @brief Checks all button states and updates the system state accordingly.
 */
void handleButtons() {
  bool leftPressed = (digitalRead(LEFT_BUTTON_PIN) == LOW);
  bool rightPressed = (digitalRead(RIGHT_BUTTON_PIN) == LOW);

  if (currentState == MOVING_TO_TARGET) return;

  // --- PRIORITY 1: Logic for when BOTH buttons are pressed ---
  if (leftPressed && rightPressed) {
    if (!bothButtonsWerePressed) {
      // This is the moment they were first pressed together
      bothButtonsPressTime = millis();
      bothButtonsWerePressed = true;
    }
    // Check if they are being held long enough to SAVE the position
    if (millis() - bothButtonsPressTime > saveHoldTime) {
      savedPosition = currentPosition;
      Serial.print("New location SAVED: ");
      Serial.println(savedPosition);
      // Wait for release to prevent repeated saves
      while(digitalRead(LEFT_BUTTON_PIN) == LOW || digitalRead(RIGHT_BUTTON_PIN) == LOW) { delay(10); }
      // Reset all trackers for the "both buttons" action
      bothButtonsWerePressed = false;
      bothClickCount = 0; 
      return; 
    }
  } 
  // --- PRIORITY 2: Logic for when BOTH buttons are released (to check for a click) ---
  else if (bothButtonsWerePressed) {
    // This block runs only on the frame where the buttons were just released
    // Check if the press was short (i.e., not a long press for saving)
    if (millis() - bothButtonsPressTime < saveHoldTime) {
        // It was a short press, treat it as a "click"
        if (millis() - lastBothClickTime < doubleClickTime) {
            bothClickCount++;
        } else {
            bothClickCount = 1;
        }
        lastBothClickTime = millis();
        
        Serial.print("Both buttons clicked. Count: ");
        Serial.println(bothClickCount);

        if (bothClickCount == 2) {
            Serial.print("Go to Saved Location command! Target: ");
            Serial.println(savedPosition);
            targetPosition = savedPosition;
            currentState = MOVING_TO_TARGET;
            bothClickCount = 0; // Reset after successful command
        }
    }
    bothButtonsWerePressed = false; // Reset the press tracker
  }
  // --- PRIORITY 3: Logic for individual button presses (Manual Jog) ---
  else {
    // Reset double-click counter if too much time has passed since the last click
    if (millis() - lastBothClickTime > doubleClickTime) {
      if (bothClickCount > 0) {
        Serial.println("Double-click timed out.");
        bothClickCount = 0;
      }
    }
    
    // Manual Jogging
    if (leftPressed) {
      currentState = MANUAL_JOG;
      takeStep(LOW, stepDelayMicroseconds); // CCW
    } else if (rightPressed) {
      currentState = MANUAL_JOG;
      takeStep(HIGH, stepDelayMicroseconds); // CW
    } else {
      if (currentState == MANUAL_JOG) {
        currentState = IDLE;
      }
    }
  }
}