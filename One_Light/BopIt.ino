#include <SoftwareSerial.h>
#include "DFRobotDFPlayerMini.h"

// // // DFPlayer Setup
SoftwareSerial mp3Serial(9, 10); // RX, TX
DFRobotDFPlayerMini dfplayer;

// ----- Enums -----
enum PlayerAction {
  ACTION_NONE,
  ACTION_SLASH, // button
  ACTION_FIRE,  // switch
  ACTION_STEER  // potentiometer
};

enum GameState {
  STATE_IDLE,         // waiting for game to start
  STATE_NEW_ROUND,    // pick command + play audio
  STATE_WAIT_INPUT,   // wait for user action or timeout
  STATE_FEEDBACK,     // play correct/wrong audio, update score
  STATE_GAME_OVER     // play lose/win audio, wait for restart
};

// ----- Pins -----
const int pot = A0;
const int switchUp = 2;
const int switchDown = 3;
const int btn = 4;

// 7-segment decoder pins (ABCD)
const int decoder1A = A1;
const int decoder1B = A2;
const int decoder1C = A3;
const int decoder1D = A4;

const int decoder2A = 7;
const int decoder2B = 8;
const int decoder2C = 5;
const int decoder2D = A5;

// mp3 files
const int slashAudio = 1;
const int steerAudio = 2;
const int fireAudio = 3;
const int wrongAudio = 4;
const int correctAudio = 5;
const int backgroundAudio = 6; //0006.mp3
const int winAudio = 7;
const int loseAudio = 8;

// ----- Globals -----
int potBaseline = 0;
bool potTurnRegistered = false;
const int potThreshold = 50;

int lastUpState = HIGH;
int lastDownState = HIGH;

GameState gameState = STATE_IDLE;
uint8_t score = 0;
PlayerAction expectedAction = ACTION_NONE;
bool lastInputWasCorrect = false;

unsigned long commandIssuedAtMs = 0;
unsigned long commandDeadlineMs = 0;

PlayerAction lastExpectedAction = ACTION_NONE; // to prevent back-to-back inputs

// Difficulty
const unsigned long BASE_TIME = 4500; // early rounds
const unsigned long MIN_TIME = 1300; // very hard rounds
const uint8_t MAX_SCORE_FOR_CURVE = 40; // where difficulty caps out

// ----- User Input Detection -----
bool buttonPush(){
  static bool lastState = HIGH; 
  bool current = digitalRead(btn);

  bool pressed = (lastState == HIGH && current == LOW);

  lastState = current;
  return pressed;
}

bool switchFlip(){
  int upState = digitalRead(switchUp);
  int downState = digitalRead(switchDown);
  bool result = false;

  // Detect UP toggle
  if (lastUpState == HIGH && upState == LOW){
    result = true;
  }

  // Detect down toggle
  if (lastDownState == HIGH && downState == LOW){
    result = true;
  }

  lastUpState = upState;
  lastDownState = downState;

  return result;

}

bool potTurn(){
  int current = analogRead(pot);
  int delta = current - potBaseline;
  if (delta < 0) delta = -delta;

  return (delta >= potThreshold);
}

// ----- Helpers -----
void resetPotentiometer(){
  potBaseline = analogRead(pot); // getting starting position
  potTurnRegistered = false; // reset per round
}

PlayerAction readPlayerAction(){
  // In idle or game over, only use the button to start/restart
  if (gameState == STATE_IDLE || gameState == STATE_GAME_OVER) {
    if (buttonPush()) {
      return ACTION_SLASH;  // "start game" trigger
    }
    return ACTION_NONE;
  }

  switch (expectedAction) {
    case ACTION_SLASH:
      if (buttonPush()) return ACTION_SLASH;
      break;

    case ACTION_FIRE:
      if (switchFlip()) return ACTION_FIRE;
      break;

    case ACTION_STEER:
      if (potTurn()) return ACTION_STEER;
      break;
  }

  return ACTION_NONE;
}

PlayerAction pickRandomAction(){
    int r = random(3);
    if (r == 0) return ACTION_SLASH;
    if (r == 1) return ACTION_FIRE;
    else return ACTION_STEER;
}

unsigned long getTimeWindow(uint8_t currentScore){
  if (currentScore >= MAX_SCORE_FOR_CURVE){
    return MIN_TIME;
  }

  float t = (float)currentScore / (float)MAX_SCORE_FOR_CURVE;
  float timeMs = BASE_TIME - t * (BASE_TIME - MIN_TIME);

  return (unsigned long)timeMs;
}

void writeDigitToDecoder(uint8_t digit, int pinA, int pinB, int pinC, int pinD){
  if (digit > 9) digit = 9;

  digitalWrite(pinA, (digit & 0x01) ? HIGH : LOW);
  digitalWrite(pinB, (digit & 0x02) ? HIGH : LOW);
  digitalWrite(pinC, (digit & 0x04) ? HIGH : LOW);
  digitalWrite(pinD, (digit & 0x08) ? HIGH : LOW);
}

void showScore(uint8_t score){
  if (score > 99) score = 99;
  uint8_t tens = score / 10;
  uint8_t ones = score % 10;

  writeDigitToDecoder(tens, decoder1A, decoder1B, decoder1C, decoder1D);
  writeDigitToDecoder(ones, decoder2A, decoder2B, decoder2C, decoder2D);
}

void resetSwitchStateForRound(){
  lastUpState = digitalRead(switchUp);
  lastDownState = digitalRead(switchDown);
}

void countdown(){

  showScore(0);
  delay(300);

  for (int i = 3; i >= 1; i--){
    showScore(i);
    delay(1000);
  }

  showScore(0);
  delay(200);
}

// ----- State Handlers -----
void handleIdleState(unsigned long now, PlayerAction action){
  if (action != ACTION_NONE){
    score = 0;
    showScore(score);

    countdown();

    audioPlayBackgroundLoop();
    gameState = STATE_NEW_ROUND;
  }
}

void handleNewRoundState(unsigned long now){
  expectedAction = pickRandomAction();

  if (expectedAction == ACTION_STEER){
    resetPotentiometer();
  }
  if (expectedAction == ACTION_FIRE){
    resetSwitchStateForRound();
  }

  audioPauseBackground();
  audioPlayCommand(expectedAction);

  // setting timing / difficulty
  unsigned long window = getTimeWindow(score);
  commandIssuedAtMs = now;
  commandDeadlineMs = now + window;

  // switch game state to waiting for input
  gameState = STATE_WAIT_INPUT;
}

const unsigned long INPUT_GRACE_MS = 200;  // 0.2s cushion

void handleWaitInputState(unsigned long now){
  // checking grace period
  if (now < commandIssuedAtMs + INPUT_GRACE_MS) {
    return;
  }

  // checking for timeout
  if (now > commandDeadlineMs){
    lastInputWasCorrect = false;
    gameState = STATE_FEEDBACK;
    return;
  }

  
  PlayerAction action = readPlayerAction();

  // no input this tick, keep waiting
  if (action == ACTION_NONE){
    return;
  }

  
  lastInputWasCorrect = (action == expectedAction);
  gameState = STATE_FEEDBACK;
}


void handleFeedbackState(unsigned long now){
  if (lastInputWasCorrect){
    score++;
    if (score > 99) score = 99;

    // --- WIN CONDITION: reached 99 ---
    if (score == 99) {
      showScore(score);   // show 99

      // Play win audio
      audioPlayWin();

      // Flash 99 a few times (blocking is fine here, it's a one-time event)
      for (int i = 0; i < 6; i++) {   // 6 flashes -> about ~2.4s
        showScore(99);
        delay(200);
        showScore(0);      // or showScore(score) with different pattern if you prefer
        delay(200);
      }

      // Leave 99 showing
      showScore(99);

      // Go to GAME_OVER and wait for player to start a new game
      gameState = STATE_GAME_OVER;
      return;
    }

    // --- NORMAL CORRECT CASE (score < 99) ---
    showScore(score);

    audioPlayCorrect();
    audioPlayBackgroundLoop(); // resume music
    gameState = STATE_NEW_ROUND;

  } else {
    // WRONG INPUT / TIMEOUT -> normal lose behavior
    audioPlayWrong();
    audioPlayLose();

    gameState = STATE_GAME_OVER;
  }
}


void handleGameOverState(unsigned long now, PlayerAction action){
  if (action != ACTION_NONE){
    score = 0;
    showScore(score);
    
    audioPlayBackgroundLoop();
    gameState = STATE_IDLE;
  }
}

// ----- Audio Functions -----
void audioPlayBackgroundLoop(){
  dfplayer.stop();
  delay(100);
  dfplayer.loop(backgroundAudio);
}

void audioPauseBackground(){
  dfplayer.pause();
}

void audioPlayCommand(PlayerAction action){
  int track = 0;

  switch (action) {
    case ACTION_SLASH:
      track = slashAudio;
      break;
    case ACTION_FIRE:
      track = fireAudio;
      break;
    case ACTION_STEER:
      track = steerAudio;
      break;
    default:
      return;  // no valid action, do nothing
  }

  dfplayer.stop();
  delay(100);
  dfplayer.play(track);
}

void audioPlayCorrect(){
  dfplayer.stop();
  delay(100);
  dfplayer.play(correctAudio);
}

void audioPlayWrong(){
  dfplayer.stop();
  delay(100);
  dfplayer.play(wrongAudio);
}

void audioPlayLose(){
  dfplayer.stop();
  delay(100);
  dfplayer.play(loseAudio);
}

void audioPlayWin(){
  dfplayer.stop();
  delay(100);
  dfplayer.play(winAudio);
}

void audioInit(){
  mp3Serial.begin(9600);
  delay(500); // giving DFPlayer time to boot

  // flash 67 if there's an error (haha funny number)
  if (!dfplayer.begin(mp3Serial)){
    while (true){
      digitalWrite(decoder1A, LOW);
    digitalWrite(decoder1B, HIGH);
    digitalWrite(decoder1C, HIGH);
    digitalWrite(decoder1D, LOW);

    digitalWrite(decoder2A, LOW);
    digitalWrite(decoder2B, HIGH);
    digitalWrite(decoder2C, HIGH);
    digitalWrite(decoder2D, HIGH);

    delay(500);

    digitalWrite(decoder1A, LOW);
    digitalWrite(decoder1B, LOW);
    digitalWrite(decoder1C, LOW);
    digitalWrite(decoder1D, LOW);

    digitalWrite(decoder2A, LOW);
    digitalWrite(decoder2B, LOW);
    digitalWrite(decoder2C, LOW);
    digitalWrite(decoder2D, LOW);
    }
    
  }

  dfplayer.volume(20);
  delay(200);
}


void setup() {
  pinMode(btn, INPUT_PULLUP);
  pinMode(switchUp, INPUT_PULLUP);
  pinMode(switchDown, INPUT_PULLUP);

  // 7-segment decoder pins
  pinMode(decoder1A, OUTPUT);
  pinMode(decoder1B, OUTPUT);
  pinMode(decoder1C, OUTPUT);
  pinMode(decoder1D, OUTPUT);
  pinMode(decoder2A, OUTPUT);
  pinMode(decoder2B, OUTPUT);
  pinMode(decoder2C, OUTPUT);
  pinMode(decoder2D, OUTPUT);

  // Recording switch positions
  resetSwitchStateForRound();

  // Initialize audio & score
  showScore(0);
  audioInit();
  audioPlayBackgroundLoop();

  // Randomness using potentiometer (no analog pins open)
  randomSeed(analogRead(pot));
}

void loop() { /* nothing — stays showing "3" */ 
  unsigned long now = millis();
  

  switch (gameState){
    case STATE_IDLE: {
      PlayerAction action = readPlayerAction();
      handleIdleState(now, action);
      break;
    }

    case STATE_NEW_ROUND:
      handleNewRoundState(now);
      break;

    case STATE_WAIT_INPUT:
      handleWaitInputState(now);  // no action param now
      break;

    case STATE_FEEDBACK:
      handleFeedbackState(now);
      break;

    case STATE_GAME_OVER: {
      PlayerAction action = readPlayerAction();
      handleGameOverState(now, action);
      break;
    }
  }
}
