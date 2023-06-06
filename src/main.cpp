#include <Arduino.h>
#include <ArduinoJson.h>
#include <EEPROM.h>
#include <HTTPClient.h>
#include <Key.h>
#include <Keypad.h>
#include <Keypad_I2C.h>
#include <MFRC522.h>
#include <SPI.h>
#include <WiFiManager.h>
#include <Wire.h>
#include <prodSecret.h>

// INFO: Variable declaration
#define I2C_KEYPAD_ADDR 0x27 // 0x20 //0x27 or 0x20
#define SS_RFID_PIN 15
#define RST_RFID_PIN 4
#define TOUCH 33 // 25
#define RELAY 27
#define BUZZ_PIN 26
#define DELAY_TIME 5000
#define DISPLAY_DELAY 2000
#define TXD2 17
#define RXD2 16
const byte ROWS = 4;
const byte COLS = 4;
unsigned long previousMillis = 0;
unsigned long interval = 30000;
unsigned long messageTimestamp = 0;
// char keys [ROWS] [COLS] = {
//   {'D', '#', '0', '*'},
//   {'C', '9', '8', '7'},
//   {'B', '6', '5', '4'},
//   {'A', '3', '2', '1'},
// };
// GEDUNG Q
char keys[ROWS][COLS] = {
    {'D', 'C', 'B', 'A'},
    {'#', '9', '6', '3'},
    {'0', '8', '5', '2'},
    {'*', '7', '4', '1'},
};
// PCB
// char keys [ROWS] [COLS] = {
//   {'1', '4', '7', '*'},
//   {'2', '5', '8', '0'},
//   {'3', '6', '9', '#'},
//   {'A', 'B', 'C', 'D'},
// };
byte rowPins[ROWS] = {0, 1, 2, 3};
byte colPins[COLS] = {4, 5, 6, 7};
// byte rowPins [ROWS] = {4, 5, 6, 7};
// byte colPins [COLS] = {0, 1, 2, 3};
String SECRET = "?id=" + String(API_ID) + "&key=" + String(API_KEY);
String BASE_URL = "https://103.179.57.88";
String ADMIN_AUTH_URL;
String CHECKIN_URL;
String DEVICE_ID;
String cardIdContainer;
String pinContainer;
String connectionStatus = "CON";
String deviceMode = "CIN";
String responsesTime = "";
boolean isCardExist = false;
boolean is_checkin = true;
boolean isAdmin = false;
boolean isConnected = true;
boolean isDisconnected = false;
boolean changeMode = false;
boolean localChange = false;

// INFO: Create instance of object
WiFiManager wm;
Keypad_I2C keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS,
                  I2C_KEYPAD_ADDR, PCF8574);
MFRC522 rfid(SS_RFID_PIN, RST_RFID_PIN);

void setupWiFi() {
  bool res;
  res = wm.autoConnect("Smart Door", "t4np454nd1"); // password protected ap

  if (!res) {
    Serial.println("Failed to connect");
  } else {
    Serial.println("Connected to the WiFi network");
  }
}

String readFromEEPROM(int addrOffset) {
  int newStrLen = EEPROM.read(addrOffset);
  char data[newStrLen + 1];
  for (int i = 0; i < newStrLen; i++) {
    data[i] = EEPROM.read(addrOffset + 1 + i);
  }
  data[newStrLen] = '\0';
  return String(data);
}

void writeToEEPROM(int addrOffset, const String &strToWrite) {
  int len = strToWrite.length();
  Serial.println("Store new data to EEPROM");
  EEPROM.write(addrOffset, len);
  for (int i = 0; i < len; i++) {
    EEPROM.write(addrOffset + 1 + i, strToWrite[i]);
    EEPROM.commit();
  }
}

void initializeDevice() {
  Serial.println("Initialize device id");
  String url = BASE_URL + "/api/v2/room/h/init" + SECRET;
  HTTPClient clinet;
  clinet.begin(url);
  clinet.addHeader("Content-Type", "application/json");
  int httpCode = clinet.POST("");
  Serial.print("HTTP STATUS: ");
  Serial.println(httpCode);
  if (httpCode > 0) {
    String payload = clinet.getString();
    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, payload);
    Serial.print("Data: ");
    String id = doc["data"]["device_id"];
    Serial.println(id);
    writeToEEPROM(0, id);
  }
  clinet.end();
  return;
}

int sendDataToServer(String endpoint, String payload) {
  String url = BASE_URL + endpoint + SECRET;
  Serial.print("URL: ");
  Serial.println(url);
  HTTPClient clinet;
  clinet.begin(url);
  clinet.addHeader("Content-Type", "application/json");
  int httpCode = clinet.POST(payload);
  Serial.println("ms");
  Serial.print("HTTP STATUS: ");
  Serial.println(httpCode);
  clinet.end();
  return httpCode;
}

void BUZZER_ON() {
  digitalWrite(BUZZ_PIN, HIGH);
  delay(150);
  digitalWrite(BUZZ_PIN, LOW);
}

void BUZZER_SUCCESS() {
  digitalWrite(BUZZ_PIN, HIGH);
  delay(500);
  digitalWrite(BUZZ_PIN, LOW);
}

void BUZZER_FAILED() {
  digitalWrite(BUZZ_PIN, HIGH);
  delay(50);
  digitalWrite(BUZZ_PIN, LOW);
  delay(50);
  digitalWrite(BUZZ_PIN, HIGH);
  delay(50);
  digitalWrite(BUZZ_PIN, LOW);
  delay(50);
  digitalWrite(BUZZ_PIN, HIGH);
  delay(100);
  digitalWrite(BUZZ_PIN, LOW);
}

void setup() {
  // INFO: Pin mode setup
  pinMode(TOUCH, INPUT);
  pinMode(RELAY, OUTPUT);
  pinMode(BUZZ_PIN, OUTPUT);

  // INFO: Start object
  Serial.begin(9600);
  Serial2.begin(19200, SERIAL_8N1, RXD2, TXD2);
  EEPROM.begin(512);
  WiFi.mode(WIFI_STA);
  Wire.begin();
  keypad.begin(makeKeymap(keys));
  SPI.begin();
  rfid.PCD_Init();
  delay(100);
  Serial.println("Init RFC522 module....");

  // INFO: Setup WiFi
  setupWiFi();
  Serial.println("Connecting to WiFi..");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Success connected to wifi");

  delay(500);
  // INFO: Setup DEVICE ID
  DEVICE_ID = readFromEEPROM(0);
  if (DEVICE_ID.length() == 0) {
    initializeDevice();
  } else {
    String endpoint = BASE_URL + "/api/v2/room/h/detail/" + DEVICE_ID + SECRET;
    HTTPClient clinet;
    clinet.begin(endpoint);
    int httpCode = clinet.GET();
    if (httpCode != 200 && httpCode > 0) {
      Serial.println("Failed to get room detail, try to");
      initializeDevice();
    } else {
      Serial.println("Success to get room information");
    }
    clinet.end();
  }

  // INFO: DEBUG
  Serial.print("DEVICE ID: ");
  DEVICE_ID = readFromEEPROM(0);
  Serial.println(DEVICE_ID);
  ADMIN_AUTH_URL = "/api/v2/room/h/validate/" + DEVICE_ID;
  CHECKIN_URL = "/api/v2/room/h/check-in/" + DEVICE_ID;
  Serial.println("Waiting for card");
  delay(1000);
  Serial.println("DIS#" + connectionStatus + "#" + DEVICE_ID + "#" +
                 deviceMode + "#" + String(pinContainer.length()) + "!");
  Serial2.print("DIS#" + connectionStatus + "#" + DEVICE_ID + "#" + deviceMode +
                "#" + String(pinContainer.length()) + "!");
  BUZZER_SUCCESS();
}

void loop() {
  char key = keypad.getKey();
  Serial.println(key);
  // INFO: Touch Sensor
  if (analogRead(TOUCH) > 1000) {
    digitalWrite(RELAY, LOW);
    BUZZER_ON();
    Serial2.print("ALT#OPENING FROM INSIDE!");
    Serial.println("ALT#OPENING FROM INSIDE!");
    // alert("OPENING\n FROM\n INSIDE");
    delay(DELAY_TIME);
    Serial.println("DIS#" + connectionStatus + "#" + DEVICE_ID + "#" +
                   deviceMode + "#" + String(pinContainer.length()) + "!");
    Serial2.print("DIS#" + connectionStatus + "#" + DEVICE_ID + "#" +
                  deviceMode + "#" + String(pinContainer.length()) + "!");
  }
  // Serial.print("TOUCH: ");
  // Serial.println(analogRead(TOUCH));

  // INFO: Check Server Connection
  if (key == '*') {
    BUZZER_ON();
    Serial2.print("ALT#CHECK SERVER CONNECTION!");
    String endpoint = "/api/v2/room/h/online/" + DEVICE_ID + "/";
    String payload = "{\"responsesTime\":\"" + responsesTime + "\"}";
    int httpCode = sendDataToServer(endpoint, payload);
    if (httpCode < 0) {
      Serial2.print("ALT#REQUEST TIME OUT!");
    } else {
      if (httpCode == 200) {
        responsesTime = ""; // reset reponse time
        Serial2.print("ALT#CONNECTION SUCCESS!");
      } else {
        Serial2.print("ALT#FAILED CONNECT TO-SERVER!");
      }
    }

    delay(DISPLAY_DELAY);

    Serial.println("DIS#" + connectionStatus + "#" + DEVICE_ID + "#" +
                   deviceMode + "#" + String(pinContainer.length()) + "!");
    Serial2.print("DIS#" + connectionStatus + "#" + DEVICE_ID + "#" +
                  deviceMode + "#" + String(pinContainer.length()) + "!");
  }

  // INFO: Changing Mode
  if (key == '#') {
    BUZZER_ON();
    pinContainer = "";
    cardIdContainer = "";
    isCardExist = false;
    if (changeMode == false) {
      deviceMode = "AUT";
      changeMode = true;
      Serial2.print("DIS#" + connectionStatus + "#" + DEVICE_ID + "#" +
                    deviceMode + "#" + String(pinContainer.length()) + "!");
      Serial2.flush();
    } else {
      if (isAdmin) {
        deviceMode = "REG";
        Serial2.print("DIS#" + connectionStatus + "#" + DEVICE_ID + "#" +
                      deviceMode + "#" + String(pinContainer.length()) + "!");
        Serial2.flush();
      } else {
        deviceMode = "CIN";
        Serial2.print("DIS#" + connectionStatus + "#" + DEVICE_ID + "#" +
                      deviceMode + "#" + String(pinContainer.length()) + "!");
        Serial2.flush();
      }
      changeMode = false;
    }
  }

  // INFO: Clearing all store data
  if (key == 'A') {
    if (isAdmin) {
      BUZZER_SUCCESS();
      Serial2.print("ALT#LOCAL PIN " + String(DEVICE_SEC_PIN)) + "!";
      Serial.println("Showing Local Pin");
      delay(DISPLAY_DELAY);
      delay(DISPLAY_DELAY);
      pinContainer = "";
      cardIdContainer = "";
      isCardExist = false;
      Serial2.print("DIS#" + connectionStatus + "#" + DEVICE_ID + "#" +
                    deviceMode + "#" + String(pinContainer.length()) + "!");
      Serial2.flush();
    } else {
      BUZZER_FAILED();
      Serial2.print("ALT#ADMIN ONLY!");
      delay(DISPLAY_DELAY);
      Serial2.print("DIS#" + connectionStatus + "#" + DEVICE_ID + "#" +
                    deviceMode + "#" + String(pinContainer.length()) + "!");
    }
  }

  // INFO: Clearing all store data
  if (key == 'C') {
    BUZZER_SUCCESS();
    Serial2.print("ALT#CLEARING ALL DATA!");
    pinContainer = "";
    cardIdContainer = "";
    isCardExist = false;
    Serial.println("Clear all data");
    delay(DISPLAY_DELAY);
    Serial2.print("DIS#" + connectionStatus + "#" + DEVICE_ID + "#" +
                  deviceMode + "#" + String(pinContainer.length()) + "!");
    Serial2.flush();
  }

  // INFO: Reset Wifi settings
  if (key == 'B') {
    if (isAdmin) {
      BUZZER_ON();
      Serial2.print("ALT#RESET WIFI SETTINGS!");
      // alert("RESET \n WIFI \n SETTINGS");
      wm.resetSettings();
      setupWiFi();
      Serial2.print("DIS#" + connectionStatus + "#" + DEVICE_ID + "#" +
                    deviceMode + "#" + String(pinContainer.length()) + "!");
    } else {
      BUZZER_FAILED();
      Serial2.print("ALT#ADMIN ONLY!");
      delay(DISPLAY_DELAY);
      Serial2.print("DIS#" + connectionStatus + "#" + DEVICE_ID + "#" +
                    deviceMode + "#" + String(pinContainer.length()) + "!");
    }
  }

  // INFO: Storing input pin
  if (key) {
    if (key != 'A' && key != 'B' && key != 'C' && key != 'D' && key != '*' &&
        key != '#') {
      BUZZER_ON();
      if (pinContainer.length() < 6) {
        pinContainer += key;
      }
      Serial.print("Current Pin : ");
      Serial.println(pinContainer);
      Serial2.print("DIS#" + connectionStatus + "#" + DEVICE_ID + "#" +
                    deviceMode + "#" + String(pinContainer.length()) + "!");
      Serial2.flush();
    }
  }

  // INFO: Changing Mode
  if (key == 'D') {
    BUZZER_ON();
    if (changeMode) {
      if (!localChange) {
        String payload = "{\"pin\":\"" + pinContainer + "\"}";
        // loading();
        Serial2.print("ALT# LOADING!");
        int httpCode = sendDataToServer(ADMIN_AUTH_URL, payload);
        if (httpCode == 200) {
          // alert("SUCCES \n CHANGING \n MODE");
          BUZZER_SUCCESS();
          Serial.println("ALT#SUCCES CHANGING MODE!");
          Serial2.print("ALT#SUCCES CHANGING MODE!");
          delay(DISPLAY_DELAY);
          changeMode = false;
          if (!isAdmin) {
            isAdmin = true;
            is_checkin = false;
            deviceMode = "REG";
          } else {
            isAdmin = false;
            is_checkin = true;
            deviceMode = "CIN";
          }
        } else {
          if (httpCode < 0) {
            Serial2.print("ALT#ENTER LOCAL PIN(RTO)!");
            delay(DISPLAY_DELAY);
            localChange = true;
          } else {
            BUZZER_FAILED();
            Serial2.print("ALT#FAILED CHANGGING MODE!");
            delay(DISPLAY_DELAY);
          }
          // alert("FAILED \n CHANGING \n MODE");
        }
        // After enter, clear all data
        Serial.println("FINISH SENDING DATA, CLEAR ALL STORE DATA");
        pinContainer = "";
        cardIdContainer = "";
        isCardExist = false;
        Serial2.print("DIS#" + connectionStatus + "#" + DEVICE_ID + "#" +
                      deviceMode + "#" + String(pinContainer.length()) + "!");
        Serial2.flush();
      } else {
        if (pinContainer == DEVICE_SEC_PIN) {
          changeMode = false;
          if (!isAdmin) {
            isAdmin = true;
            is_checkin = false;
            deviceMode = "REG";
          } else {
            isAdmin = false;
            is_checkin = true;
            deviceMode = "CIN";
          }
          Serial.print("ALT#SUCCES CHANGING MODE!");
          Serial2.print("ALT#SUCCES CHANGING MODE!");
          delay(DISPLAY_DELAY);
          localChange = false;
        } else {
          Serial.print("ALT#FAILED CHANGGING MODE!");
          Serial2.print("ALT#FAILED CHANGGING MODE!");
          delay(DISPLAY_DELAY);
          localChange = true;
        }
        pinContainer = "";
        cardIdContainer = "";
        isCardExist = false;
        Serial2.print("DIS#" + connectionStatus + "#" + DEVICE_ID + "#" +
                      deviceMode + "#" + String(pinContainer.length()) + "!");
        Serial2.flush();
      }
    }
  }

  // INFO: Reading RFID
  if (!isCardExist) {
    if (rfid.PICC_IsNewCardPresent() &&
        rfid.PICC_ReadCardSerial()) { // new tag is available & NUID has been
                                      // readed
      BUZZER_ON();
      for (byte i = 0; i < rfid.uid.size; i++) {
        cardIdContainer.concat(String(rfid.uid.uidByte[i] < 0x10 ? " 0" : " "));
        cardIdContainer.concat(String(rfid.uid.uidByte[i], HEX));
      }

      Serial.print("CARD ID : ");
      Serial.println(cardIdContainer);
      isCardExist = true;

      // INFO: Sending Data To Server
      if (!changeMode) {
        Serial.print("Data send to server : ");
        String payload = "{\"cardNumber\":\"" + cardIdContainer + "\"" + "," +
                         "\"pin\":\"" + pinContainer + "\"}";
        Serial.println(payload);
        // loading();
        Serial2.print("ALT# LOADING!");
        if (is_checkin == true) {
          unsigned long StartTime = millis();
          int httpCode = sendDataToServer(CHECKIN_URL, payload);
          unsigned long executionTime = millis() - StartTime;
          Serial.print("Response time: ");
          Serial.print(executionTime);
          String strTime = String(executionTime);
          responsesTime += strTime + ","; // Generate report for servers

          if (httpCode == 200) {
            digitalWrite(RELAY, LOW);
            BUZZER_SUCCESS();
            Serial2.print("ALT#SUCCESS OPEN ROOM!");
            // alert("SUCCESS \n OPEN \n THE ROOM");
            Serial.println("RUANGAN BERHASIL TERBUKA");
            delay(DELAY_TIME);
          }

          if (httpCode == 401) {
            BUZZER_FAILED();
            // alert("ENTER \n CORRECT \n PIN");
            Serial2.print("ALT#ENTER CORRECT PIN!");
            delay(DISPLAY_DELAY);
          }

          if (httpCode == 400) {
            BUZZER_FAILED();
            // alert("FAILED \n OPEN \n THE ROOM");
            Serial2.print("ALT#FAILED OPEN ROOM!");
            delay(DISPLAY_DELAY);
            Serial.println("RUANGAN GAGAL TERBUKA");
          }

          if (httpCode < 0) {
            Serial2.print("ALT#REQUEST TIME OUT!");
            delay(DISPLAY_DELAY);
          }
        }

        if (is_checkin == false) {
          if (pinContainer.length() == 6) {
            int httpCode = sendDataToServer("/api/v1/card/h/register", payload);
            if (httpCode == 201) {
              // alert("SUCCESS \n REGISTER \n CARD");
              digitalWrite(BUZZ_PIN, HIGH);
              Serial2.print("ALT#SUCCESS REGISTER " + cardIdContainer + "!");
              delay(DISPLAY_DELAY);
              Serial.println("CARD \n SUCCESSFULLY \n REGISTER");
              digitalWrite(BUZZ_PIN, LOW);
            }
            if (httpCode != 201) {
              // alert("CARD \n ALREADY \n REGISTER");
              BUZZER_FAILED();
              Serial2.print("ALT#FAILED REGISTER CARD!");
              delay(DISPLAY_DELAY);
              Serial.println("CARD ALREADY REGISTER");
            }
            if (httpCode == 500) {
              // alert("CARD \n ALREADY \n REGISTER");
              BUZZER_FAILED();
              Serial2.print("ALT#CARD ALREADY REGISTER!");
              delay(DISPLAY_DELAY);
              Serial.println("CARD ALREADY REGISTER");
            }

            if (httpCode < 0) {
              Serial2.print("ALT#REQUEST TIME OUT!");
              delay(DISPLAY_DELAY);
            }
          } else {
            // alert("ENTER \n 6 MIN \n PIN");
            BUZZER_FAILED();
            Serial2.print("ALT#ENTER CORRECT PIN!");
            delay(DISPLAY_DELAY);
            Serial.println("GAGAL MENDAFTARKAN KARTU, MINIMUM 6 PIN");
          }
        }

        Serial.println("FINISH SENDING DATA, CLEAR ALL STORE DATA");
        pinContainer = "";
        cardIdContainer = "";
        isCardExist = false;
        Serial2.print("DIS#" + connectionStatus + "#" + DEVICE_ID + "#" +
                      deviceMode + "#" + String(pinContainer.length()) + "!");
      }

      rfid.PCD_Init();
    }
  }

  rfid.PCD_Init();

  // INFO: Wifi Functionality
  if (WiFi.status() != WL_CONNECTED) {
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= interval) {
      WiFi.disconnect();
      Serial.print("Recon status: ");
      if (WiFi.reconnect()) {
        Serial2.print("DIS#" + connectionStatus + "#" + DEVICE_ID + "#" +
                      deviceMode + "#" + String(pinContainer.length()) + "!");
      }
      previousMillis = currentMillis;
    }
  }
  digitalWrite(RELAY, HIGH);
  // Serial.println("DIS#" + connectionStatus + "#" + DEVICE_ID + "#" +
  // deviceMode + "#" + String(pinContainer.length()) + "!");

  // INFO: Update Online Status
  uint64_t now = millis();
  if (now - messageTimestamp > 300000) {
    messageTimestamp = millis();
    String endpoint = "/api/v2/room/h/online/" + DEVICE_ID + "/";
    String payload = "{\"responsesTime\":\"" + responsesTime + "\"}";
    int httpCode = sendDataToServer(endpoint, payload);
    responsesTime = ""; // reset reponse time
    Serial.println("Updating online time");
  }
}