/* DEBUG instrucions
 * TBD
 */

#include <Arduino.h>
#include <Config.h>
#if USE_WiFi == 1
#include <WifiHandler.h>
#endif
#include <esp32-hal-log.h>
#include <FastLED.h>
#include <ESP32Servo.h>

#if defined(CDC)
HWCDC HWCDCSerial;
#endif

// ----- Define variables, classes and functions -----
#pragma region Variables

ulong bootTime;
bool ledOn = false;
CRGB leds[1]; // Array for the RGB LED

ESP32PWM motorPwm;
Servo steeringServo;
Servo topServo;
int speed = 0;
int steering = 90;
int top = 90;

#pragma endregion

#pragma region Functions

void steer(int value);
void drive(int value);
void topServoGo(int value);

#pragma endregion
// ---

#pragma region Setup
void setup() {

  #if defined(CDC)
  HWCDCSerial.begin();
  HWCDCSerial.setDebugOutput(true);
  while (!HWCDCSerial.isConnected()) {
    delay(10); // Halt if not connected
  }
  delay(2000); // Wait for the connection to be established
  #else
  Serial.begin(BAUDRATE);
  Serial.setDebugOutput(true);
  #endif

  log_i("START " __FILE__ " from " __DATE__);
  log_i("Setting up...");
  
	ESP32PWM::allocateTimer(0);
	ESP32PWM::allocateTimer(1);
	ESP32PWM::allocateTimer(2);
	ESP32PWM::allocateTimer(3);
  motorPwm.attachPin(M_EN, 1000);
  steeringServo.attach(STEER_SERVO, 1200, 1700);
  topServo.attach(TOP_SERVO, 500, 2400);

  log_i("Setting up builtin RGB LED...");
  FastLED.addLeds<SK6812, LED, GRB>(leds, 1);
  FastLED.setBrightness(50);
  leds[0] = CRGB(255, 150, 0);
  FastLED.show();

  pinMode(PIN_3V3_BUS, OUTPUT);
  digitalWrite(PIN_3V3_BUS, HIGH);

  pinMode(M1, OUTPUT);
  pinMode(M2, OUTPUT);

  #if USE_WiFi == 1
  wifi_init();
  leds[0] = CRGB(255, 150, 0);
  FastLED.show();
  #endif

  log_i("Setup done.");
  bootTime = millis();

  leds[0] = CRGB(0, 0, 255);
  FastLED.setBrightness(0);
  FastLED.show();
}
#pragma endregion

#pragma region Loop
void loop() {
  steeringServo.write(steering);
  topServo.write(top);

  if (speed > 0) {
    digitalWrite(M1, HIGH);
    digitalWrite(M2, LOW);
    motorPwm.writeScaled(speed / 100.0);
  } else if (speed < 0) {
    digitalWrite(M1, LOW);
    digitalWrite(M2, HIGH);
    motorPwm.writeScaled(-speed / 100.0);
  } else {
    digitalWrite(M1, LOW);
    digitalWrite(M2, LOW);
    motorPwm.writeScaled(0.0);
  }

}
#pragma endregion

// --- Functions ---
#pragma region Functions

void steer(int value) {
  steeringServo.write(value);
  steering = value;
  if (steering < 0) {
    steering = 0;
    steeringServo.write(steering);
  } else if (steering > 180) {
    steering = 180;
    steeringServo.write(steering);
  }
}

void drive(int value) {
  motorPwm.writeScaled(value / 100.0);
  speed = value;
  if (speed < -100) {
    speed = -100;
    motorPwm.writeScaled(speed / 100.0);
  } else if (speed > 100) {
    speed = 100;
    motorPwm.writeScaled(speed / 100.0);
  }
}

void topServoGo(int value) {
  value = 180 - value;
  topServo.write(value);
  top = value;
  if (top < 0) {
    top = 0;
    topServo.write(top);
  } else if (top > 180) {
    top = 180;
    topServo.write(top);
  }
}

#pragma endregion
// ---
