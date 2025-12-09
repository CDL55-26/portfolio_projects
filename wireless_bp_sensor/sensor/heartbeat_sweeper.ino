#include <SPI.h>
#include <math.h>

const int PIN_SEN = 10; //latch enable

uint8_t state_for_Cshunt_pF(float C) {
  int s = (int)lround((C - 0.90f) / 0.119f);
  if (s < 0) s = 0;
  if (s > 31) s = 31;
  return (uint8_t)s;
}

void pe64906_write(uint8_t state, bool standby = false) {
  uint8_t cmd = 0x40 | (standby << 5) | (state & 0x1F);
  SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
  digitalWrite(PIN_SEN, HIGH);
  SPI.transfer(cmd);
  digitalWrite(PIN_SEN, LOW);
  SPI.endTransaction();
}

void setup() {
  SPI.begin();
  pinMode(PIN_SEN, OUTPUT);
  digitalWrite(PIN_SEN, LOW);

  Serial.begin(115200);
}


void loop() {
  static float C        = 5.0; //start at high (baseline)
  static float lowLimit = 1.0;   //current lower bound
  static float highLimit= 5.0;   //ccurrent upper bound
  const  float stepMag  = 0.1; //pF change per step
  static bool  done     = false;

  //heartbeat phases
  enum Phase { HOLD_BASE, MOVE_DOWN, MOVE_UP };
  static Phase phase = HOLD_BASE;

  static int holdCount = 0;
  static int moveCount = 0;

  if (done) {// stay at 3 pF
    C = 3.0f;
    uint8_t state = state_for_Cshunt_pF(C);
    pe64906_write(state);

    //Serial Plotter 
    Serial.print("C:");
    Serial.print(C);
    Serial.print(" low:");
    Serial.print(lowLimit);
    Serial.print(" high:");
    Serial.println(highLimit);

    delay(60);
    return;
  }


  float range = highLimit - lowLimit;
  if (range < stepMag) range = stepMag;

  int Nfull = (int)lround(range / stepMag);
  if (Nfull < 1) Nfull = 1;


  int baselineHoldSteps = 2 * Nfull; ////should be 4
  int moveSteps         = Nfull;//steps for high→low and low→high


  switch (phase) {
    case HOLD_BASE:// at baseline ~2/3 of the cycle
      C = highLimit;
      holdCount++;
      if (holdCount >= baselineHoldSteps) {
        holdCount = 0;
        moveCount = 0;
        phase = MOVE_DOWN;
      }
      break;

    case MOVE_DOWN:// Go from highLimit → lowLimit over moveSteps steps
      if (moveCount < moveSteps) {
        C -= stepMag;
        if (C < lowLimit) C = lowLimit;
        moveCount++;
      } else {
        moveCount = 0;
        phase = MOVE_UP;
      }
      break;

    case MOVE_UP://from lowLimit → highLimit over moveSteps steps
      if (moveCount < moveSteps) {
        C += stepMag;
        if (C > highLimit) C = highLimit;
        moveCount++;
      } else { //shrink
        if (highLimit > 3.0f) {
          highLimit -= 0.2f;
          if (highLimit < 3.0f) highLimit = 3.0f;
        }
        if (lowLimit < 3.0f) {
          lowLimit += 0.2f;
          if (lowLimit > 3.0f) lowLimit = 3.0f;
        }
        phase = HOLD_BASE;
        holdCount = 0;
        moveCount = 0;
      }
      break;
  }


  uint8_t state = state_for_Cshunt_pF(C);
  pe64906_write(state);


  Serial.print("C:");
  Serial.print(C);
  Serial.print(" low:");
  Serial.print(lowLimit);
  Serial.print(" high:");
  Serial.println(highLimit);


  if (lowLimit >= 3.0f && highLimit <= 3.0f && fabs(C - 3.0f) < 0.05f) {
    done = true;
    C = 3.0f;
  }

  delay(75);
}
