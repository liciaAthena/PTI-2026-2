#include <SoftwareSerial.h>
#include <Wire.h>
//#include "mpu6050.h" // MPU6050 code from https://www.youtube.com/watch?v=ijiZ_UyChbs
#include <Ultrasonic.h> //Erick Simões Ultrasonic library https://github.com/ErickSimoes/Ultrasonic
#include <TinyGPS++.h> // TinyGPSPlus https://arduiniana.org/libraries/tinygpsplus/

// Bluetooth -~-~-~-
SoftwareSerial bt(0,1); //rx,tx

// GPS -~-~-~-
TinyGPSPlus gps;
SoftwareSerial ss(2,3);
unsigned long currentMillis;
unsigned long startMillisGps;
const unsigned long intervalGps = 60000; // 1 min

// H-Bridge -~-~-~-
int ENA = 10; // PWM roda direita
int IN1 = 9; // CW roda direita (frente)
int IN2 = 8; // CCW (reverso)
int IN3 = 7; // CCW roda esquerda (frente)
int IN4 = 6; // CW (reverso)
int ENB = 5; // PWM roda esquerda
bool forward, backward, left, right = false;

// Ultrasonic -~-~-~-
Ultrasonic ultrasonic1(A5, A4, 11000UL); // trig - echo - timeout || right ultrasonic
Ultrasonic ultrasonic2(A3, A2, 11000UL); // left ultrasonic (car's pov) 
int distance1, distance2;
int distanceCM = 15;
String lastTurn = "";

#define enableBluetooth true
#define enableUltrasonic true
#define enableCompass true

#define enableAccelerometer false
#define enableGPS false

#define enableAutomation false
#define debug false

// Inicialization ///////////////////////////////////////
void setup() 
{
  Serial.begin(9600);
  delay(1500);

  // Bluetooth 
  if (enableBluetooth) {
    bt.begin(9600);
  }
  //GPS  
  if (enableGPS) {
    ss.begin(9600);
    startMillisGps = millis();
  }
  // Accelerometer - MPU6050 -~-~-~-
  /*if (enableAccelerometer) {
    Serial.println("// Calibrando acelerômetro");
    mpu_begin();
    delay(1000);
    mpu_calibrate(200);
    Serial.println("// Acelerômetro calibrado");
    mpu_reset();
  }*/

  // H-Bridge -~-~-~-
  pinMode(ENA, OUTPUT);
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  pinMode(ENB, OUTPUT);

  // Ensure wheels are actually turned off upon inicialization
  analogWrite(ENA, 255);
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
  analogWrite(ENB, 255);
  delay(100);

  if (enableAutomation) {
    if (!debug) {
      delay(5000);
    }
    forward = true;
    updateMotors();
  }
}

void loop() {
  currentMillis = millis();
  // Remote control
  if (enableBluetooth) {
    bt.listen();
    if (bt.available() > 0) {
      int cmd = (bt.read() - '0');
      Serial.println(cmd);
      switch (cmd) {
        case 1: // forwards down
          //run = true;
          forward = true;
          backward = false;
          break;
        case 2: // backwards down
          forward = false;
          backward = true;
          break;
        case 3: // fb up          
          backward = false;
          forward = false;
          break;
        case 4: // left down         
          left = true;
          right = false;
          break;
        case 5: // right down          
          left = false;
          right = true;
          break;
        case 6: // lr up         
          left = false;
          right = false;
          break;
        default:
          Serial.println("Unknown command");
      } 
      int w1cw = ((forward && !right) || left) ? HIGH : LOW; // right forward      
      int w1ccw = ((backward && !left) || right) ? HIGH : LOW; // right backward      
      int w2ccw = ((forward && !left) || right) ? HIGH : LOW; // left forward      
      int w2cw = ((backward && !right) || left) ? HIGH : LOW; // left backward

      digitalWrite(IN1, w1cw); // right wheel forward      
      digitalWrite(IN2, w1ccw); // right wheel backward
      digitalWrite(IN3, w2ccw); // left wheel forward
      digitalWrite(IN4, w2cw); // left wheel backward
    }
  } 

  // GPS data - runs every defined interval
  if (enableGPS && (currentMillis - startMillisGps > intervalGps)) {
    startMillisGps = currentMillis;
    ss.listen();
    if (ss.available() > 0) {
      gps.encode(ss.read());
      if (gps.location.isUpdated()) {
        Serial.print("Latitude = ");
        Serial.print(gps.location.lat(), 6);
        Serial.print(" Longitude = ");
        Serial.println(gps.location.lng(), 6);
      }
    }
  }
  
  // Accelerometer data
  /*if (enableAccelerometer) {
    mpu_loop();
    Serial.println(getAngleZ());
  }*/
  
  // Ultrasonic data
  if (enableUltrasonic) {
    distance1 = ultrasonic1.read();
    distance2 = ultrasonic2.read();
    //Serial.print(distance1); Serial.print(" | "); Serial.println(distance2);
  }

  if (enableAutomation && enableUltrasonic && enableCompass) {
    if (distance1 < distanceCM || distance2 < distanceCM) { // something is too close to either ultrasonics
      forward = false; //stop and wait 100ms
      updateMotors();
      delay(100); 
      backward = true; // go back for 500ms
      updateMotors();
      delay(500); 
      backward = false;

      int delta = abs(distance2 - distance1); //disparity between two distances
      if (delta < 7) { // delta negligible
        //turn opposite of lastTurn, move forward
        if (lastTurn == "") {
          lastTurn = "left";
        }

        if (lastTurn == "left") { // TODO - DRYS
          Serial.println("|| Turning right");
          lastTurn = "right";
          left = false;
          right = true; //turn car right on the spot
          updateMotors();
          delay(1000); //TODO replace delay, wait until car has turned 90d <- need HMC5883l to actually work for this >:(
          
          right = false;
          forward = true;
          updateMotors();
          delay(1000); //wait a certain amount of time...
          
          right = true;
          forward = false;
          updateMotors();
          delay(1000); //TODO wait until car has turned 90d
        } else {
          Serial.println("|| Turning left");
          lastTurn = "left";
          right = false;
          left = true; //turn car left on the spot
          updateMotors();
          delay(1000); //TODO replace delay, wait until car has turned 90d
          
          left = false;
          forward = true;
          updateMotors();
          delay(1000); //wait a certain amount of time...
          
          left = true;
          forward = false;
          updateMotors();
          delay(1000); //TODO wait until car has turned 90d
        }

        forward = true;
        updateMotors();
      } else {
        if ((distance1 > distance2)) { // something is too close to left sensor
          Serial.println("|| Avoiding left");
          //turn right, move forward
          forward = true;
          right = true;
          updateMotors();
          delay(500);
          left = true;
          updateMotors();
        } else if (distance1 < distance2) { // too close to right sensor
          Serial.println("|| Avoiding right");
          forward = true;
          left = true;
          updateMotors();
          delay(500);
          right = true;
        }
      }
    }
  }

  
}

void updateMotors() {
  if (!debug) {
    int w1cw = ((forward && !right) || left) ? HIGH : LOW; // right forward
    int w1ccw = ((backward && !left) || right) ? HIGH : LOW; // right backward  
    int w2ccw = ((forward && !left) || right) ? HIGH : LOW; // left forward  
    int w2cw = ((backward && !right) || left) ? HIGH : LOW; // left backward

    digitalWrite(IN1, w1cw); // right wheel forward      
    digitalWrite(IN2, w1ccw); // right wheel backward
    digitalWrite(IN3, w2ccw); // left wheel forward
    digitalWrite(IN4, w2cw); // left wheel backward
  } else {
    Serial.print("forward "); Serial.print(forward);
    Serial.print(" | backward "); Serial.print(backward);
    Serial.print(" | left "); Serial.print(left);
    Serial.print(" | right "); Serial.println(right);
  }
  
}



