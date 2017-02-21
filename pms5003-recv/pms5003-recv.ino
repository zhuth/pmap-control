#include <RCSwitch.h>
#define SW1 8
#define SW2 9
RCSwitch mySwitch = RCSwitch();

int v = 0, state = 0;
void setup() {
  Serial.begin(9600);
  Serial2.begin(9600);
  
  // 中断0使用2脚，终端1使用3脚
  mySwitch.enableReceive(0);  // Receiver on inerrupt 0 => that is pin #2

  pinMode(SW1, OUTPUT);
  pinMode(SW2, OUTPUT);
  pinMode(10, OUTPUT); digitalWrite(10, HIGH);
  pinMode(6, OUTPUT); digitalWrite(10, LOW);
  pinMode(7, OUTPUT); digitalWrite(10, HIGH);
}

inline void pressSW(int sw) {
  digitalWrite(sw, HIGH);
  delay(200);
  digitalWrite(sw, LOW);
  delay(500);
}

void deal() {
  if (v == 126) {
    if (state == 0) {
      // turn on
      pressSW(SW1);
      delay(1000);
      state = 1;
    }
    v = 3;
  } else if (v == 125) {
    state = 0;
    // as if turned off
  }
  if (v == 0 && state > 0) {
    // turn off
    state = 0;
    pressSW(SW1);
    Serial.println("Turned off.");
  }
  if (v >= 1 && v <= 3 && v != state && state > 0) {
    // switch to v-rd speed
    pressSW(SW2);
    if (v == state - 1 || v - state == 2) pressSW(SW2);
    state = v;
  } 
}

void loop() {
  if (mySwitch.available()) {
    
    int value = mySwitch.getReceivedValue();
    if (value == 0) {
      Serial.print("Unknown encoding");
    } else {
      v = mySwitch.getReceivedValue() - 1;
      Serial.print("Received ");
      Serial.print(v);
      Serial.print(" / ");
      Serial.print(state);
      deal();
    }
    mySwitch.resetAvailable();
  }

  if (Serial2.available()) {
    v = Serial2.read();
    deal();
  }

  if (Serial.available()) {
    Serial.println(state);
    char c = Serial.read();
    switch (c) {
    case 'a':
      pressSW(SW1);
      break;
    case 'b':
      pressSW(SW2);
      break;
    case '0':
    case '1':
    case '2':
    case '3':
      state = c - '0';
      break;
    }
  }
}
