#include <RCSwitch.h>

#include <DS1302.h>

#include <SimpleDHT.h>

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define OFFSET 10
#define DHT11_PIN 10

LiquidCrystal_I2C lcd(0x3F, 20, 4);
SimpleDHT11 dht11;
DS1302 rtc(11, 12, 13);
RCSwitch mySwitch = RCSwitch();

  
byte temperature = 0;
byte humidity = 0;

void setup() {
  lcd.init();
  lcd.backlight();
  delay(100);
  uint8_t degree[8] = {0x8,0xf4,0x8,0x43,0x4,0x4,0x43,0x0}; // Custom char degres c
  byte p5[] = {
  B11111,
  B10000,
  B11110,
  B00001,
  B00001,
  B10001,
  B01110,
  B10000
  };
  lcd.createChar(1, degree);
  lcd.createChar(2, p5);
  lcd.setCursor(0, 0); lcd.print("AQI");
  lcd.setCursor(0, 1); lcd.print("PM2\2 PM10 Tem Hum");
  
  Serial1.begin(9600);
  if (Serial) Serial.begin(9600);
  if (Serial2) Serial2.begin(9600);

  mySwitch.enableTransmit(9);
  mySwitch.send(126, 7);

  Time t = rtc.time();
  if (t.sec >= 60 || t.min >= 60 || t.hr >= 24) {
    rtc.writeProtect(false);
    rtc.halt(false);
    // Make a new time object to set the date and time.
    // Sunday, September 22, 2013 at 01:38:50.
    t.min += t.sec;
    t.sec %= 60;
    if (t.min >= 60) {
      t.hr++;
      t.min %= 60;
    }
    if (t.hr >= 24) {
      t.hr = 0;
    }
    Time t1(t.yr, t.mon, t.date, t.hr, t.min, t.sec, Time::kSunday);
    // Set the time and date on the chip.
    rtc.time(t1);
  }
}

inline int aqi_pm25(int pm25) {
  // break points: 12, 35.5, 55.5, 150.5, 250.5, 350.5, 500.5
  //               50   100   150    200    300    400    500
  if (pm25 <= 12) return pm25 * 50 / 12;
  else if (pm25 <= 35) return 50 + (pm25 - 12) * 50 / 23;
  else if (pm25 <= 55) return 100 + (pm25 - 35) * 5 / 2;
  else if (pm25 <= 150) return 150 + (pm25 - 55) * 50 / 95;
  else if (pm25 <= 350) return 50 + pm25;
  else return 400 + (pm25 - 350) * 2 / 3;  
}

inline int aqi_pm10(int pm10) {
  // break points: 55, 155, 255, 355, 425, 505, 605
  if (pm10 <= 55) return pm10 * 50 / 55;
  else if (pm10 <= 355) return 50 + (pm10 - 55) / 2;
  else if (pm10 <= 425) return 200 + (pm10 - 355) * 10 / 7;
  else if (pm10 <= 505) return 300 + (pm10 - 425) * 10 / 8;
  else return pm10 - 105;  
}

inline int aqi(int pm25, int pm10) {
  int a_25 = aqi_pm25(pm25);
  int a_10 = aqi_pm10(pm10);
  return (a_25 > a_10) ? a_25 : a_10;
}

int atoilen(char* buf, int offset, int len) {
  int x = 0;
  for (int i = offset; i < offset+len; ++i) {
    x *= 10;
    x += buf[i] - '0';
  }
  return x;
}

char buf1[30], buf2[10], stg[20];
char i1 = 0, i2 = 0, ic = 0, c, state = 0;
int pm1, pm25, pm10, p_aqi, p_aqi_last = 0, ch2o, ch2o_m = 0;
int mil;
int a25, a10;
bool enableSend = true;

void loop() {
  if (Serial1.available()) {
    c = Serial1.read();
    buf1[i1] = c;
    if ((i1 == 0 && c != 0x42) || (i1 == 1 && c != 0x4d)) i1 = -1;
    i1++;
  }

  if (i1 == 30) {  
    pm1 = ((int)buf1[OFFSET] << 8) | buf1[OFFSET + 1];
    pm25 = ((int)buf1[OFFSET + 2] << 8) | buf1[OFFSET + 3];
    pm10 = ((int)buf1[OFFSET + 4] << 8) | buf1[OFFSET + 5];

    if (p_aqi < 1000) {
      i1 = 0;
      ic++;
      a25 += pm25; a10 += pm10;
    }
    if (ic == 4) {
      pm25 = a25 / ic; pm10 = a10 / ic;
      p_aqi = aqi(pm25, pm10);
      a25 = 0; a10 = 0; ic = 0;
      
      lcd.setCursor(5, 0);
      sprintf(stg, "%4d", p_aqi); 
      lcd.print(stg);
      lcd.setCursor(0, 2);
      sprintf(stg, "%4d %4d", pm25, pm10);
      lcd.print(stg);
    }

    int t = temperature; t *= 10;
    buf1[24] = (byte)(t >> 8);
    buf1[25] = (byte)(t & 0xff);
    t = humidity; t *= 10;
    buf1[26] = (byte)(t >> 8);
    buf1[27] = (byte)(t & 0xff);

    Serial.write("\x42\x4d", 2);
    Serial.write(buf1, 30);
  }
  
  if (Serial2.available()) {
    c = Serial2.read();
    buf2[i2] = c;
    if ((i2 == 0 && c != 0xff) || (i2 == 1 && c != 0x17)) i2 = -1;
    i2++;
  }
  if (i2 == 9) {
    i2 = 0;
    
    ch2o = (buf2[4] << 8) | buf2[5];
    ch2o_m = (ch2o_m > ch2o) ? ch2o_m : ch2o;
    lcd.setCursor(11, 0);
    sprintf(stg, "CH2O:%4d", ch2o, ch2o_m);
    lcd.print(stg);
  }
  
  //lcd.setCursor(0, 3); lcd.print("      ");
  
  if (Serial && millis() - mil >= 1000) {
    mil = millis();
    
    Time t = rtc.time();
    lcd.setCursor(0, 3);
    sprintf(stg, "%04d-%02d-%02d %02d:%02d:%02d", t.yr, t.mon, t.date, t.hr, t.min, t.sec);    
    lcd.print(stg);
    
    if (0 == dht11.read(DHT11_PIN, &temperature, &humidity, NULL)) {
      lcd.setCursor(10, 2);
      sprintf(stg, "%2d\1 %2d%%", temperature, humidity);
      lcd.print(stg);
    } else {
      //Serial.print("DHT Err.\t");
    }

    if ((mil / 1000) % 5 == 0) {
      if (p_aqi <= 25)
        state = 0;
      else if ((p_aqi <= 75 && state > 2) || (p_aqi >= 50 && state < 2))
        state = 2;
      else if (p_aqi >= 120)
        state = 126;
      else if (p_aqi >= 100 && state < 3)
        state = 3;
      if (enableSend)
        mySwitch.send(state + 1, 7);
      p_aqi_last = p_aqi;
    }
  
//    Serial.print(pm1, DEC); Serial.print(" ");
//    Serial.print(pm25, DEC); Serial.print(" ");
//    Serial.print(pm10, DEC); Serial.print(" ");
//    Serial.print(p_aqi, DEC); Serial.print(" ");
//    Serial.print(ch2o, DEC); Serial.print(" ");
//    Serial.print("\n");
  }

  if (Serial.available()) {
    char c = Serial.read();
    switch (c) {
    case ' ':
      lcd.noBacklight();
      break;
    case ':':
      lcd.backlight();
      break;
    case '-':
      enableSend = !enableSend;
    case '0':
    case '1':
    case '2':
    case '3':
      mySwitch.send(c - '0' + 1, 7);
      break;
    case 'x':
      mySwitch.send(127, 7);
      break;
    case 'y':
      mySwitch.send(126, 7);
      break;
    case 't':
      char tbuf[12];
      for (int i = 0; i < 11; ++i) {
        while (!Serial.available()) ;
        tbuf[i] = Serial.read();
      }
      Time tt(
        2000 + atoilen(tbuf, 0, 2),
        atoilen(tbuf, 2, 2),
        atoilen(tbuf, 4, 2),
        atoilen(tbuf, 6, 2),
        atoilen(tbuf, 8, 2),
        0,
        (Time::Day)atoilen(tbuf, 10, 1));
      rtc.writeProtect(false);
      rtc.halt(false);
      rtc.time(tt);
      break;
    }    
  }

//  if (ch2o >= 60 || p_aqi >= 150) {
//    lcd.noBacklight(); delay(200); lcd.backlight(); delay(100);
//    ch2o = 0; p_aqi = 0;
//  }
}

