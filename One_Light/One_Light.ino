// Global Variables
// ----------------------
int t = 1000; // delay time, decreases with more correct inputs
int PotentiometerStartValue = 0;
int PotentiometerLastValue = 0;
const int potentiometerChange = 100;

int SwitchLastActivePin = 0;
const int SWITCH_UP_PIN = 2;
const int SWITCH_DOWN_PIN = 3;

int lastButtonState = HIGH; // stores previous state of button (for edge detection)

// ----------------------
// Setup Function
// ----------------------
void setup() {
  // User Inputs
  pinMode(4, INPUT_PULLUP); // button
  pinMode(SWITCH_UP_PIN, INPUT_PULLUP);   // switch up
  pinMode(SWITCH_DOWN_PIN, INPUT_PULLUP); // switch down
  // Potentiometer is on A0 (no pinMode needed)

  // LED Outputs
  pinMode(6, OUTPUT); // button LED
  pinMode(7, OUTPUT); // switch LED
  pinMode(8, OUTPUT); // wheel LED

  // Initialize potentiometer reference
  PotentiometerStartValue = analogRead(A0);
  PotentiometerLastValue = PotentiometerStartValue;

  // Determine initial switch position
  if (digitalRead(SWITCH_UP_PIN) == LOW) {
    SwitchLastActivePin = SWITCH_UP_PIN;
  } else if (digitalRead(SWITCH_DOWN_PIN) == LOW) {
    SwitchLastActivePin = SWITCH_DOWN_PIN;
  }
}

// ----------------------
// Main Loop Function
// ----------------------
void loop() {
  // --- BUTTON INPUT (Detect press, not hold) ---
  int currentButtonState = digitalRead(4);

  // Detect transition from HIGH → LOW (unpressed → pressed)
  if (lastButtonState == HIGH && currentButtonState == LOW) {
    digitalWrite(6, HIGH);  // flash LED when button pressed
    delay(150);
    digitalWrite(6, LOW);
  }

  lastButtonState = currentButtonState; // update button state

  // --- SWITCH INPUT (Detects change between up/down) ---
  int currentActivePin = 0;
  int pin2State = digitalRead(SWITCH_UP_PIN);
  int pin3State = digitalRead(SWITCH_DOWN_PIN);

  // Determine current position
  if (pin2State == LOW && pin3State == HIGH) {
    currentActivePin = SWITCH_UP_PIN; // switch up
  } else if (pin3State == LOW && pin2State == HIGH) {
    currentActivePin = SWITCH_DOWN_PIN; // switch down
  }

  // Detect valid change
  if (currentActivePin != 0 && currentActivePin != SwitchLastActivePin) {
    digitalWrite(7, HIGH);          // LED on to signal change
    delay(200);                     // LED stays on shortly
    digitalWrite(7, LOW);           // turn off LED after delay
    SwitchLastActivePin = currentActivePin; // update position
  }

  // --- POTENTIOMETER INPUT ---
  int potentiometerCurrentValue = analogRead(A0);
  if (abs(potentiometerCurrentValue - PotentiometerLastValue) > potentiometerChange) {
    digitalWrite(8, HIGH);               // LED on for noticeable change
    delay(200);                          // keep it visible
    digitalWrite(8, LOW);                // turn off
    PotentiometerLastValue = potentiometerCurrentValue;
  }

}