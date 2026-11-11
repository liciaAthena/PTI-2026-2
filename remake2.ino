#include <SoftwareSerial.h>
#include <Wire.h>
#include <math.h>
//#include <HMC5883L.h>
#include <Ultrasonic.h> //Erick Simões Ultrasonic library https://github.com/ErickSimoes/Ultrasonic

// Bluetooth -~-~-~-
SoftwareSerial bt(0,1); //rx,tx

// H-Bridge -~-~-~-
int ENA = 10; // PWM roda direita
int IN1 = 9; // CW roda direita (frente)
int IN2 = 8; // CCW (reverso)
int IN3 = 7; // CCW roda esquerda (frente)
int IN4 = 6; // CW (reverso)
int ENB = 5; // PWM roda esquerda
bool forward, backward, left, right = false;

// Ultrasonic -~-~-~-
Ultrasonic ultrasonic1(A1, A0, 11000UL); // trig - echo - timeout || right ultrasonic
Ultrasonic ultrasonic2(A3, A2, 11000UL); // left ultrasonic (car's pov) 
int distance1, distance2;
int distanceCM = 15;
String nextTurn = "";

// Compass -~-~-~-
#define HMC 0x1E
float angle;

bool escrever(byte reg, byte valor) {
  Wire.beginTransmission(HMC);
  Wire.write(reg);
  Wire.write(valor);

  byte erro = Wire.endTransmission();

  if (erro != 0) {
    Serial.print("Erro I2C: ");
    Serial.println(erro);
    return false;
  }
  return true;
}

bool ler(byte reg, byte *dados, byte qtd) {
  Wire.beginTransmission(HMC);
  Wire.write(reg);

  if (Wire.endTransmission(false) != 0)
    return false;

  if (Wire.requestFrom((uint8_t)HMC, qtd) != qtd)
    return false;

  for (byte i = 0; i < qtd; i++)
    dados[i] = Wire.read();

  return true;
}

const char* cardeal(float graus) {
  if (graus >= 337.5 || graus < 22.5) return "NORTE";
  if (graus < 67.5) return "NORDESTE";
  if (graus < 112.5) return "LESTE";
  if (graus < 157.5) return "SUDESTE";
  if (graus < 202.5) return "SUL";
  if (graus < 247.5) return "SUDOESTE";
  if (graus < 292.5) return "OESTE";
  return "NOROESTE";
}

#define enableBluetooth true
#define enableUltrasonic true
#define enableCompass true

#define enableAccelerometer false
#define enableGPS false

#define enableAutomation true
#define debug false

// Inicialization ///////////////////////////////////////
void setup() 
{
  Serial.begin(9600);
  Wire.begin();
  Wire.setClock(50000);
  delay(1500);

  // Bluetooth 
  if (enableBluetooth) {
    bt.begin(9600);
  }

  // Compass
  bool ok = true;
  ok &= escrever(0x00, 0x70);
  delay(20);
  ok &= escrever(0x01, 0x20);
  delay(20);
  ok &= escrever(0x02, 0x00);
  delay(100);

  if (ok) {
    Serial.println("Configuracao concluida!");
  } else {
    Serial.println("Falha na configuracao!");
  }
    
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

  /*
  if (enableCompass) {
    byte dados[6];
    if (ler(0x03, dados, 6)) {
      int16_t x = (int16_t)(
        ((uint16_t)dados[0] << 8) | dados[1]);
      int16_t z = (int16_t)(
        ((uint16_t)dados[2] << 8) | dados[3]);
      int16_t y = (int16_t)(
        ((uint16_t)dados[4] << 8) | dados[5]);
      if (x == -4096 || y == -4096 || z == -4096) {
        Serial.println("Leitura saturada!");
        delay(500);
        return;
      }

      angle = atan2((float)y, (float)x) * 180.0 / PI;
      if (angle < 0) angle += 360.0;

      Serial.print("X=");
      Serial.print(x);

      Serial.print(" Y=");
      Serial.print(y);

      Serial.print(" Z=");
      Serial.print(z);

      Serial.print(" | Angulo=");
      Serial.print(angle, 1);

      Serial.print(" | ");
      Serial.println(cardeal(angle));

    } else {
      Serial.println("ERRO: Falha na leitura!");
    }
    //delay(500);
  }
  */

  // Ultrasonic data
  if (enableUltrasonic) {
    distance1 = ultrasonic1.read();
    delay(10);
    distance2 = ultrasonic2.read();
    delay(10);
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

      Serial.print("d1 - "); Serial.print(distance1); Serial.print(" | d2 - "); Serial.print(distance2);

      int delta = abs(distance2 - distance1); //disparity between two distances
      Serial.print(" | delta - "); Serial.println(delta);
      if (delta < 7) { // delta negligible
        //turn opposite of nextTurn, move forward
        /*if (nextTurn == "") {
          nextTurn = "left";
        }*/
        nextTurn = (nextTurn == "right") ? "left" : "right"; //toggles nextTurn and sets it to right by default
        Serial.print("|| Turning "); Serial.println(nextTurn);

        bool dir = (nextTurn == "right") ? true : false;
        right = dir; //turn car left/right on the spot
        left = !dir;
        updateMotors();
        waitTurn(nextTurn, 90.0); 

        right = false;  // go forward a certain amount of time
        left = false;
        forward = true;
        updateMotors();
        delay(2000); 

        right = dir; //turn car left/right on the spot
        left = !dir;
        updateMotors();
        waitTurn(nextTurn, 90.0); 
        /*
        if (nextTurn == "left") { // TODO - DRYS
          Serial.println("|| Turning right");
          nextTurn = "right";
          left = false;
          right = true; //turn car right on the spot
          updateMotors();
          waitTurn("right", 90.0);

          right = false;
          forward = true;
          updateMotors();
          delay(1000); //wait a certain amount of time...
          
          right = true;
          forward = false;
          updateMotors();
          waitTurn("right", 90.0);
          
        } else {
          Serial.println("|| Turning left");
          nextTurn = "left";
          right = false;
          left = true; //turn car left on the spot
          updateMotors();
          waitTurn("left", 90.0);
          
          left = false;
          forward = true;
          updateMotors();
          delay(1000); //wait a certain amount of time...
          
          left = true;
          forward = false;
          updateMotors();
          waitTurn("left", 90.0);
        }
        */
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
        } else if (distance1 < distance2) { // too close to right sensor
          Serial.println("|| Avoiding right");
          forward = true;
          left = true;
          updateMotors();
          delay(500);
          right = true;
        }
        updateMotors();
        delay(500);
        left = right = false;
      }
    }
  }
  delay(100);
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

void waitTurn(String direction, float waitangle) {
  updateAngle();
  if (direction == "right") { // turning right substracts the angle
    waitangle*= -1;  
  }

  float targetangle = angle + waitangle;

  Serial.print("waitangle - "); Serial.print(waitangle); Serial.print(" | angle - "); Serial.print(angle); Serial.print(" | targetangle - "); Serial.print(targetangle); Serial.print(" | difference - "); Serial.println(deltaangle(angle, targetangle));

  while (!compare(direction, deltaangle(angle, targetangle))) {
    delay(100);
    updateAngle();
    Serial.print("waitangle - "); Serial.print(waitangle); 
    Serial.print(" | angle - "); Serial.print(angle); 
    Serial.print(" | targetangle - "); Serial.print(targetangle); 
    Serial.print(" | difference - "); Serial.println(deltaangle(angle, targetangle));
  }
  delay(1000);
  return;
}

bool compare(String direction, float delta) {
  return (direction == "right") ? (delta > 0) : (delta < 0);
}

double clamp(double v, double min, double max) {
    return min(max, max(min, v));
}

float repeat(float t, float length) {
    return clamp(t - floor(t/length) * length, 0.0f, length);
}

float deltaangle(float current, float target) {
    float delta = repeat((target - current), 360.0f);
    if (delta > 180.0f)
        delta -= 360.0f;
    return delta;
}

void updateAngle() {
  byte dados[6];
    if (ler(0x03, dados, 6)) {
      int16_t x = (int16_t)(
        ((uint16_t)dados[0] << 8) | dados[1]);
      int16_t z = (int16_t)(
        ((uint16_t)dados[2] << 8) | dados[3]);
      int16_t y = (int16_t)(
        ((uint16_t)dados[4] << 8) | dados[5]);
      if (x == -4096 || y == -4096 || z == -4096) {
        Serial.println("Leitura saturada!");
        delay(500);
        return;
      }

      angle = atan2((float)y, (float)x) * 180.0 / PI;
      if (angle < 0) angle += 360.0;
      /*Serial.print("X=");
      Serial.print(x);

      Serial.print(" Y=");
      Serial.print(y);

      Serial.print(" Z=");
      Serial.print(z);

      Serial.print(" | Angulo=");
      Serial.print(angle, 1);

      Serial.print(" | ");*/
      Serial.println(cardeal(angle));

    } else {
      Serial.println("ERRO: Falha na leitura!");
    }
    delay(500);
}


