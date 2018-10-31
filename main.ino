#include <LedControl.h>
#include <ArduinoSTL.h>

namespace Utils {
  void clearScreen(LedControl & led) {
  for (int row = 0; row < 8; ++row) {
    led.setRow(0, row, 0x0);
    }
  }

  void getNumberMatrix(int (&numbers)[8], int score) {
    if (score == 0) {
      numbers[0] = numbers[7] = 0x0;
      numbers[1] = numbers[6] = 0x0;
      numbers[2] = numbers[5] = 0x7E;
      numbers[3] = numbers[4] = 0x42;
      return;
    } else if (score == 1) {
      numbers[0] = numbers[1] = numbers[2] = numbers[6] = numbers[7] = 0x0;
      numbers[4] = numbers[5] = 0x7E;
      numbers[3] = 0x60;
      return;
    }

    for (int row = 0; row < 8; ++row) numbers[row] = 0x00;
  }
};

struct Controls {
  Controls(int chUp, int chDown, int chRight, int chLeft, int chFire) :
    _chUp(chUp), _chDown(chDown), _chRight(chRight), _chLeft(chLeft), _chFire(chFire) {}

  Controls() {}

  int getUp() { return _chUp; }
  int getDown() { return _chDown; }
  int getRight() { return _chRight; }
  int getLeft() { return _chLeft; }
  int getFire() { return _chFire; }

private:
  int _chUp; int _chDown; int _chRight; int _chLeft; int _chFire;
};

enum class PlayerNumber {
  P1, P2
};

enum class PlayerStatus {
  Alive, Dead
};

struct Player {
  Player(PlayerNumber playerNumber, int column, const Controls & controls) : 
    _column(column), _defaultColumn(column),
    _positions(0x38), _defaultPositions(0x38),// 00111000
    _score(0), _status(PlayerStatus::Alive),
    _playerNumber(playerNumber), _controls(controls) {
  }

  Player() {}

  bool move(int ch) {
    // eh tudo ao contrario 
    bool isShot = false;
    if (ch == _controls.getUp()) {
      if (this->getPosition(7)) return; // 128
      _positions = _positions << 1;
    } else if (ch == _controls.getDown()) {
      if (this->getPosition(0)) return;
      _positions = _positions >> 1;
    } else if (ch == _controls.getRight()) {
      if (_column == 0) return;
      _column -= 1;
    } else if (ch == _controls.getLeft()) {
      if (_column == 7) return;
      _column += 1;
    } else if (ch == _controls.getFire()) {
      isShot = true;
    } else {
      // todos os outros caracteres lixo
    }

    return isShot;
  }

  int getColumn() const {
    return _column;
  }

  int getRow() const {
    for (int row = 0; row < 8; ++row) {
      if (this->getPosition(row)) {
        return row + 1; // sempre vai ser a prox
      }
    }
    return 0;
  }

  int getPosition(int row) const {
    return (_positions & (0x1 << row)) > 0;
  }

  void resetPositions() {
    _positions = _defaultColumn; _positions = _defaultPositions;
    _status = PlayerStatus::Alive;
  }

  void print(LedControl & led) const {
    for (int row = 0; row < 8; row++) {
      led.setLed(0, _column, row, this->getPosition(row));
    }
  }

  void printScore(LedControl & led) const {
    int numbers[8];
    Utils::getNumberMatrix(numbers, _score);
    for (int row = 0; row < 8; ++row) {
      led.setRow(0, row, numbers[row]);
    }
  }

  PlayerStatus getStatus() const {
    return _status;
  }

  void died() {
    _status = PlayerStatus::Dead;
  }

  PlayerNumber getPlayerNumber() const {
    return _playerNumber;
  }

  int incScore() {
    _score++;
  }

  int getScore() const {
    return _score;
  }

private:
  int _column; int _defaultColumn; int _positions; int _defaultPositions; int _score; PlayerStatus _status; PlayerNumber _playerNumber; Controls _controls;
};

enum class ShotStatus {
  Active, Inactive
};

struct Shot {
  Shot(PlayerNumber firedBy, int actualLed, int row, int column) :
    _actualLed(actualLed), _row(row), _column(column),
    _firedBy(firedBy), _status(ShotStatus::Active)
    {}

  void move() {
    if (_firedBy == PlayerNumber::P1) {
      if (_column == 7 && _actualLed == 1) {
        _status = ShotStatus::Inactive;
      } else if (_column == 7 && _actualLed == 0) {
        _column = 0;
        _actualLed = 1;
      } else {
        _column++;
      }
    } else {
      if (_column == 0 && _actualLed == 0) {
        _status = ShotStatus::Inactive;
      } else if (_column == 0) {
        _column = 7;
        _actualLed = 0;
      } else {
        _column--;
      }
    }
  }

  void changeStatus(ShotStatus status) {
    _status = status;
  }

  ShotStatus getStatus() const {
    return _status;
  }

  void print(LedControl & led) const {
    if (_status == ShotStatus::Active) {
      led.setLed(0, _column, _row, 1);
    }
  }

  int getColumn() const {
    return _column;
  }

  int getRow() const {
    return _row;
  }

  PlayerNumber getFiredBy() const {
    return _firedBy;
  }

  int getActualLed() const {
    return _actualLed;
  }

private:
  int _actualLed; int _row; int _column; PlayerNumber _firedBy; ShotStatus _status;
};

struct Game {
  Game(const Player & p1, const Player & p2) : _p1(p1), _p2(p2) {
    _shots.reserve(50); // 50 tiros por vez max, 25 cada
  }

  void print(LedControl (&leds)[2]) {
    if (_p1.getStatus() == PlayerStatus::Alive) _p1.print(leds[0]);
    if (_p2.getStatus() == PlayerStatus::Alive) _p2.print(leds[1]);
    for (const auto & s : _shots) {
      s.print(leds[s.getActualLed()]);
    }
  }

  void printScore(LedControl (&leds)[2]) {
    _p1.printScore(leds[0]); _p2.printScore(leds[1]);
  }

  void resetPositions() {
    _p1.resetPositions(); _p2.resetPositions();
    _shots.erase(_shots.begin(), _shots.end());
  }

  void move() {
    for (const auto & s : _shots) {
      s.move();
    }
  }

  void movePlayers(int byteRec) {    
    bool isShot;
    if (_p1.getStatus() == PlayerStatus::Alive) {
      isShot = _p1.move(byteRec);
      if (isShot) this->addShot(_p1);
    }
    
    if (_p2.getStatus() == PlayerStatus::Alive) {
      isShot = _p2.move(byteRec);
      if (isShot) this->addShot(_p2);
    }
  }

  bool detectCollisions() {
    bool collisionDetected = false;
    for (const auto & s : _shots) {
      int ledShot = s.getActualLed();
      if (s.getStatus() == ShotStatus::Active && static_cast<int>(s.getFiredBy()) != ledShot) {
        Player & p = ledShot == 0 ? _p1 : _p2;
        Player & pOther = ledShot == 0 ? _p2 : _p1;
        if (p.getStatus() == PlayerStatus::Alive && s.getColumn() == p.getColumn()) {
          int shotRow = s.getRow(); int playerRow = p.getRow();
          if (shotRow == playerRow || shotRow == playerRow + 1 || shotRow == playerRow - 1) {
            p.died();
            pOther.incScore();
            collisionDetected = true;
          }
        }
      }
    }

    return collisionDetected;
  }

  void removeInactiveShots() {
    _shots.erase(std::remove_if(_shots.begin(), _shots.end(), [](const Shot & s) {
      return s.getStatus() == ShotStatus::Inactive;
    }), _shots.end());
  }

  void addShot(const Player & p) {
    PlayerNumber pNumber = p.getPlayerNumber();
    _shots.push_back(Shot(pNumber, static_cast<int>(pNumber), p.getRow(), p.getColumn()));
  }

private:
  Player _p1; Player _p2;
  std::vector<Shot> _shots;
};

const int dataPin1 = 4, clkPin1 = 6, csPin1 = 5;
const int dataPin2 = 11, clkPin2 = 13, csPin2 = 12;
LedControl leds[2] = {
  LedControl(dataPin1, clkPin1, csPin1, 1), // left
  LedControl(dataPin2, clkPin2, csPin2, 1)  // right
};

Game game(
  Player(PlayerNumber::P1, 0, Controls('w', 's', 'a', 'd', ' ')), // left
  Player(PlayerNumber::P2, 7, Controls('8', '5', '4', '6', '0')) // right
);

unsigned long timeGameRefresh, timeGameRemoveShots;
void setup() {
  Serial.begin(9600); // configura a serial para 9600

  leds[0].shutdown(0, false); leds[0].setIntensity(0, 2);
  leds[1].shutdown(0, false); leds[1].setIntensity(0, 2);

  timeGameRefresh = timeGameRemoveShots = millis();
}

void loop() {
  Utils::clearScreen(leds[0]); Utils::clearScreen(leds[1]);
  game.print(leds);

  if (millis() - timeGameRefresh > 200) {
    game.move();
    timeGameRefresh = millis();
  }

  if (millis() - timeGameRemoveShots > 5000) {
    game.removeInactiveShots();
    timeGameRemoveShots = millis();
  }

  int byteRec;
  if (Serial.available() > 0) {
    byteRec = Serial.read(); 
    game.movePlayers(byteRec);
  }

  if (game.detectCollisions()) {
    Utils::clearScreen(leds[0]); Utils::clearScreen(leds[1]);
    game.printScore(leds);
    delay(2000);
    game.resetPositions();
  }
}
