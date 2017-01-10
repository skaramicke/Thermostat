
#include <SevSeg.h>

// Pins
#define PinPot 0
#define PinDHT 1
#define PinDigit1 2
#define PinDigit2 3
#define PinSegmentA 4
#define PinSegmentB 5
#define PinSegmentC 6
#define PinSegmentD 7
#define PinSegmentE 8
#define PinSegmentF 9
#define PinSegmentG 12
#define PinSegmentDP 13
#define PinRedLED 10
#define PinYellowLED 11

// Pulse modes
#define PulseModeRising 1
#define PulseModeBoth 2

SevSeg sevseg;

float wantedTemp;
float minTemp = 15;
float maxTemp = 25;
float currentTemp = 1;
float span = 2.0;
float flashTime = 0.5;
float pulseTime = 2.0;
int pulseStrength = 100;
int settingTime = 2; // seconds;
static unsigned long settingTimeout = 0;

#define NORMAL 0
#define SETTING 1

int state = NORMAL;
bool warming = false;

#define MILLIS_PER_SECOND 1000

void setup() {
  // Serial.begin(9600);
  
  DDRC |= _BV(PinDHT);
  PORTC |= _BV(PinDHT);

  pinMode(INPUT, PinPot);
  
  wantedTemp = map(analogRead(A0), 0, 1024, minTemp, maxTemp);
  byte numDigits = 2;
  byte digitPins[] = {PinDigit1, PinDigit2};
  byte segmentPins[] = {PinSegmentA, PinSegmentB, PinSegmentC, PinSegmentD, PinSegmentE, PinSegmentF, PinSegmentG , PinSegmentDP};
  bool resistorsOnSegments = false;
  byte hardwareConfig = COMMON_ANODE;
  sevseg.begin(hardwareConfig, numDigits, digitPins, segmentPins, resistorsOnSegments);
  sevseg.setBrightness(1);
  pinMode(PinRedLED, OUTPUT);
  analogWrite(PinRedLED, 255);
  sevseg.setNumber(wantedTemp);
  sevsegRefreshDelay(1000);
  analogWrite(PinRedLED, 0);
}


void loop() {
  readWantedTemperature();
  updateTemperature();
  setWarmingState();
  setDisplayState();  
}


// Setting the target temperature
void readWantedTemperature() 
{
  float newValue = map(analogRead(PinPot), 0, 1024, minTemp, maxTemp);
  int didChange = 0;
  if (round(newValue) != round(wantedTemp)) {
    didChange = 1;
    state = SETTING;
    settingTimeout = millis() + settingTime * MILLIS_PER_SECOND;
  }
  wantedTemp = newValue;
}

// Warming or not
void setWarmingState() 
{
  if (currentTemp < wantedTemp - span/2) {
    warming = true;
  }
  else if (currentTemp > wantedTemp + span/2) {
    warming = false;
  }
}

// What should be displayed?
void setDisplayState()
{
  if (state == SETTING) {
    flashStatusLight();
    if (settingTimeout > 0 && millis() > settingTimeout) {
      state = NORMAL;
      settingTimeout = 0;
    }
    sevseg.setNumber(wantedTemp);
  }
  else {
    if (warming) {
      pulseStatusLight();
    }
    else {
      analogWrite(PinRedLED, 0);
      analogWrite(PinYellowLED, 0);
    }
    sevseg.setNumber(currentTemp);
  }
  
  sevseg.refreshDisplay();
}

// Temperature reading
byte read_dht11_dat()
{
  byte i = 0;
  byte result=0;
  for(i=0; i< 8; i++){
    while(!(PINC & _BV(PinDHT)));  // wait for 50us
    delayMicroseconds(30);
    if(PINC & _BV(PinDHT))
      result |=(1<<(7-i));
    while((PINC & _BV(PinDHT)));  // wait '1' finish
  }
  return result;
}


void updateTemperature() 
{
  static unsigned long tempCountdown = 0;
  static unsigned long tempInterval = 10000;
  unsigned long now = millis();
  if (state == NORMAL && (now > tempCountdown || tempCountdown > now + tempInterval)) {
    int tests = 5;
    int total = 0;
    for (int i = 0; i < tests; i++) {
      int thisResult = get_temp();
      total += thisResult;
      int blinks = 2;
      for (int y = 0; y < blinks; y ++) {
        analogWrite(PinYellowLED, y%2==0?0:50);
        analogWrite(PinRedLED, y%2==0?50:0);
        sevsegRefreshDelay(150/blinks);
      }
    }
    analogWrite(PinYellowLED, 0);
    currentTemp = total / tests;
    tempCountdown = now + tempInterval;
  }
}


int get_temp()
{
  int temp1[3];
  int temp2[3];
  int hum1[3];
  int hum2[3];

  byte dht11_dat[5];
  byte dht11_in;
  byte i;
  // start condition
  // 1. pull-down i/o pin from 18ms
  PORTC &= ~_BV(PinDHT);
  delay(18);
  PORTC |= _BV(PinDHT);
  delayMicroseconds(40);
  
  DDRC &= ~_BV(PinDHT);
  delayMicroseconds(40);

  dht11_in = PINC & _BV(PinDHT);

  if (dht11_in) {
     // Serial.println("dht11 start condition 1 not met");
     return 0;
  }
  delayMicroseconds(80);

  dht11_in = PINC & _BV(PinDHT);

  if (!dht11_in) {
    // Serial.println("dht11 start condition 2 not met");
    return 0;
  }
  delayMicroseconds(80);
  // now ready for data reception
  for (i=0; i<5; i++)
    dht11_dat[i] = read_dht11_dat();

  DDRC |= _BV(PinDHT);
  PORTC |= _BV(PinDHT);

  byte dht11_check_sum = dht11_dat[0]+dht11_dat[1]+dht11_dat[2]+dht11_dat[3];
  // check check_sum
  if (dht11_dat[4]!= dht11_check_sum)
  {
    // Serial.println("DHT11 checksum error");
    return 0;
  }

  return dht11_dat[2];

}


// LED control
void flashStatusLight() {
  setStatusLight(flashTime, PulseModeBoth);
}


void pulseStatusLight() {
  setStatusLight(pulseTime, PulseModeRising);
}


void setStatusLight(float multiple, int mode) {
  float something = millis()/1000.0 / multiple;
  int value = pulseStrength / 2 + pulseStrength / 2 * sin( something * 2.0 * PI  );
  analogWrite(PinRedLED, value);
  if (mode == PulseModeRising) {
    int offsetValue = pulseStrength / 2 + pulseStrength / 2 * cos( something * 2.0 * PI  );
    analogWrite(PinYellowLED, offsetValue);
  }
  else if (mode == PulseModeBoth) {
    analogWrite(PinYellowLED, value);
  }
}


// Refresh display while delaying

void sevsegRefreshDelay(unsigned int delayInMillis) {
  unsigned long stopTime = millis() + delayInMillis;
  while (millis() < stopTime) {
    sevseg.refreshDisplay();
  }
}

