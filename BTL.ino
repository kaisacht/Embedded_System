#include <LedControl.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define MAP_SIZE 8
#define MAX_SCORE 24
#define EAT 0
#define DIE 1
#define WIN 2
#define SPEAKER_PIN 8
#define ROW_HEIGHT 12
#define INIT_UPDATE_RATE 5
#define DELAY 1500

float updateRate;
int highScore = 0;
const int buttonPins[] = {2, 3, 4, 5};
bool gameOver;
bool appleDisplay = true;
byte gameMap[MAP_SIZE]; // The 8 rows of the LED Matrix

#define OLED_ADDR   0x3C
Adafruit_SSD1306 display(128, 64, &Wire, -1);

struct Point {
  int rPos; // The row of the point
  int cPos; // The column of the point

  bool equals(Point p) {
    return rPos == p.rPos && cPos == p.cPos;
  } 

  Point add(int dr, int dc) {
    int newRPos = (rPos + dr) % MAP_SIZE;
    int newCPos = (cPos + dc) % MAP_SIZE;
    if(newRPos < 0) newRPos += MAP_SIZE;
    if(newCPos < 0) newCPos += MAP_SIZE;
    return {newRPos, newCPos};
  }
};
Point apple;

Point genApple();

void emitSound(int condition) {
  switch(condition){
    case EAT:
      tone(8, 100, 1200);
      break;
    case DIE:
      tone(8, 150, 1500);
      break;
    case WIN:
      tone(8, 500, 3000);
      break;
  }
}

struct Snake {
  Point head;     // The head of the snake's head
  Point body[MAX_SCORE]; // An array that contains the coordinates of the the snake's body
  int len;         // The length of the snake 
  int dir[2];      // A direction to move the snake along

  void move() {
    Point newHead = head.add(dir[0], dir[1]);
  
    // Check if the Snake hits itself
    for(int i = 0; i < len; i++) {
      if(body[i].equals(newHead)) {
        emitSound(DIE);
        gameOver = true;
        display.clearDisplay();
        return;
      }
    }

    // Check if The snake ate the apple
    if(newHead.equals(apple)) {
      emitSound(EAT);
      len += 1;
      if(len > highScore) {
        highScore = len;
      }
      apple = genApple();
      updateRate += 0.1;
      display.clearDisplay();
    } else {
      // Shift the body forward
      for(int i = 1; i < len; i++)
        body[i - 1] = body[i];
    }

    // Append the new head to the body
    body[len - 1] = newHead;
    head = newHead;
  }
};
Snake snake;

//MAX72XX led Matrix
const int DIN = 11;
const int CS = 10;
const int CLK = 13;
LedControl lc = LedControl(DIN, CLK, CS, 1);

// Variables To Handle The Game Time
float oldTime, timer;

void initGame() {
  display.clearDisplay();
  snake = {{1, 5}, {{0, 5}, {1, 5}}, 2, {1, 0}}; // Initialize a snake object
  apple = genApple(); // Initialize an apple object
  oldTime = millis();
  timer = 0;
  updateRate = INIT_UPDATE_RATE;
  gameOver = false;
}

void setup() {
  display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  display.setTextColor(WHITE);
  display.setTextSize(1);
  pinMode(SPEAKER_PIN, OUTPUT);
  /*
  The MAX72XX is in power-saving mode on startup,
  we have to do a wakeup call
  */
  lc.shutdown(0, false);
  /* Set the brightness to a medium values */
  lc.setIntensity(0, 8);
  /* and clear the display */
  lc.clearDisplay(0);

  for(int i = 0; i < 4; i++){
    pinMode(buttonPins[i], INPUT_PULLUP);
  }
  initGame();
}

void printDisplay(int row, const char* message) {
  display.setCursor(0, row * ROW_HEIGHT);
  display.print(message);
  display.display();
}

Point genApple() {
  Point newPoint;
  while(true) {
    newPoint = {(int)random(0, MAP_SIZE), (int)random(0, MAP_SIZE)};
    bool collide = false;
    for(int i = 0; i < snake.len; i++){
      if(newPoint.equals(snake.body[i])) {
        collide = true;
        break;
      }
    }
    if (!collide) break;
  }
  return newPoint;
}

void checkGameState() {
  int score = snake.len;
  if (score < MAX_SCORE) {
    printDisplay(1, ("Your score: " + String(score)).c_str());
    printDisplay(2, ("High score: " + String(highScore)).c_str());
  } else {
    emitSound(WIN);
    printDisplay(1, "You win!");
  }
}

bool isClicked() {
  return digitalRead(buttonPins[0]) == LOW ||
    digitalRead(buttonPins[1]) == LOW ||
    digitalRead(buttonPins[2]) == LOW ||
    digitalRead(buttonPins[3]) == LOW;
}

void readInstruction() {
  if(digitalRead(buttonPins[0]) == LOW && snake.dir[1] == 0){
    snake.dir[0] = 0;
    snake.dir[1] = -1;
  } else if(digitalRead(buttonPins[1]) == LOW && snake.dir[1] == 0){
    snake.dir[0] = 0;
    snake.dir[1] = 1;
  } else if(digitalRead(buttonPins[2]) == LOW && snake.dir[0] == 0){
    snake.dir[0] = -1;
    snake.dir[1] = 0;
  } else if(digitalRead(buttonPins[3]) == LOW && snake.dir[0] == 0){
    snake.dir[0] = 1;
    snake.dir[1] = 0;
  }
}

void Update() {
  float currentTime = millis();
  float dt = currentTime - oldTime;
  oldTime = currentTime;
  timer += dt;
  if(timer < DELAY / (2 * updateRate) && !appleDisplay) {
    appleDisplay = true;
  }
  if(timer > DELAY / (2 * updateRate) && appleDisplay) {
    appleDisplay = false;
  }
  if(timer > DELAY / updateRate){
    timer = 0;
    snake.move();
  }
}

void Render() {
  for(int i = 0; i < MAP_SIZE; i++)
    gameMap[i] = 0;

  for(int i = 0; i < snake.len; i++)
    gameMap[snake.body[i].rPos] |= 128 >> snake.body[i].cPos;
  
  if(appleDisplay)
    gameMap[apple.rPos] |= 128 >> apple.cPos;

  for(int i = 0; i < MAP_SIZE; i++)
    lc.setRow(0, i, gameMap[i]);
}

void loop() {
  if(gameOver) {
    printDisplay(1, ("Your Score: " + String(snake.len)).c_str());
    printDisplay(2, ("High Score: " + String(highScore)).c_str());
    printDisplay(3, "Press any button...");
    if(isClicked())
      initGame();
  } else {
    checkGameState();
    readInstruction();
    Update();
    Render();
  }
}