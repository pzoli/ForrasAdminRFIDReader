#include <Arduino.h>
#include <SPI.h>
#include "EEpromWriteAnything.h"
#include <Ethernet.h>
#include <MFRC522.h>
#include <ArduinoJson.h>
JsonDocument doc;

#define RST_PIN 9 // Configurable, see typical pin layout above
#define SS_PIN 7  // Configurable, see typical pin layout above
#define BUZZER_PIN 6

#define RGB_R 2
#define RGB_G 3
#define RGB_B 4

// #define DEBUG
#define DHCP

MFRC522 mfrc522(SS_PIN, RST_PIN); // Create MFRC522 instance

struct NetConfig
{
  char state[10];
  uint8_t mac[6];
  uint8_t usedhcp;
  uint8_t serverip[4];
  uint8_t ip[4];
  uint8_t subnet[4];
  uint8_t gateway[4];
  uint8_t dnsserver[4];
  uint16_t serverport;
  char request[80];
} conf;

void printIPToSerial(String ipname, IPAddress addr)
{
  char ipno[16];
  sprintf(ipno, "%d.%d.%d.%d", addr[0], addr[1], addr[2], addr[3]);
  Serial.print(ipname);
  Serial.println(ipno);
};

void initEthernet()
{
#ifdef DHCP
  if (conf.usedhcp == 1)
  {
    Serial.println(F("Ethernet configure useing DHCP"));
    Ethernet.begin(conf.mac);
    Ethernet.maintain();
  }
  else
  {
#endif
    Serial.println(F("Ethernet configure using fix IP"));
    Ethernet.begin(conf.mac, conf.ip, conf.dnsserver, conf.gateway, conf.subnet);
#ifdef DHCP
  }
#endif
}

String getMACasString(uint8_t *mac)
{
  char macno[32];
  sprintf(macno, "%02X-%02X-%02X-%02X-%02X-%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(macno);
}

void setColor(int redValue, int greenValue, int blueValue)
{
  analogWrite(RGB_R, redValue);
  analogWrite(RGB_G, greenValue);
  analogWrite(RGB_B, blueValue);
}

void setup()
{
  // Open serial communications and wait for port to open:
  Serial.begin(115200);
  pinMode(10, OUTPUT);
  digitalWrite(10, HIGH);
  pinMode(7, OUTPUT);
  digitalWrite(7, HIGH);
  pinMode(4, OUTPUT);
  digitalWrite(4, HIGH);
  pinMode(BUZZER_PIN, OUTPUT);

  pinMode(RGB_R, OUTPUT);
  pinMode(RGB_G, OUTPUT);
  pinMode(RGB_B, OUTPUT);

  setColor(255, 0, 0);
  delay(1000);
  setColor(0, 255, 0);
  delay(1000);
  setColor(0, 0, 255);
  delay(1000);
  setColor(255, 255, 255);

#ifdef DEBUG
  while (!Serial)
  {
    ;
  }
#endif
  EEPROM_readAnything(1, conf);
  bool configured = String(conf.state).startsWith("CONFIG");
  if (configured)
  {
    initEthernet();
  }
  Serial.println(configured ? F("Configured") : F("Not confugured yet"));

  if (configured)
  {
    Serial.print(F("reader ip: "));
    Serial.println(Ethernet.localIP());
    printIPToSerial(F("server ip: "), conf.serverip);
  }

  Serial.println("init RFID reader...");
  mfrc522.PCD_Init(); // Init MFRC522
#ifdef DEBUG
  mfrc522.PCD_DumpVersionToSerial(); // Show details of PCD - MFRC522 Card Reader details
#endif
  Serial.println("RFID reader initialized.");
  tone(BUZZER_PIN, 1000);
  delay(250);
  noTone(BUZZER_PIN);
}

EthernetClient webClient;
bool dataSent = false;
bool inJSON = false;
String webResult = "";
String command = "";

void loop()
{
  if (webClient.available())
  {
    char buff[64];
    int cnt = webClient.readBytes(buff, sizeof(buff));
    for (int i = 0; i < cnt; i++)
    {
      if (buff[i] == '{' && !inJSON)
      {
        inJSON = true;
        webResult = "";
      }
      else if (buff[i] == '}' && inJSON)
      {
        inJSON = false;
        webResult += buff[i];
        if (webResult.equals(F("{\"RESPONSE\":\"OK\"}")))
        {
          setColor(0, 255, 0);
          tone(BUZZER_PIN, 1000);
          delay(250);
          noTone(BUZZER_PIN);
          setColor(255, 255, 255);
        }
        else
        {
          setColor(255, 0, 0);
          tone(BUZZER_PIN, 1000);
          delay(250);
          noTone(BUZZER_PIN);
          delay(250);
          tone(BUZZER_PIN, 1000);
          delay(250);
          noTone(BUZZER_PIN);
          setColor(255, 255, 255);
        }
      }
      if (inJSON)
      {
        webResult += buff[i];
      }
    }
#ifdef DEBUG
    Serial.write(buff, cnt);
#endif
  }

  if (!webClient.connected() && dataSent)
  {
    webClient.stop();
    dataSent = false;
  }
  // Look for new cards
  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial())
  {
    String cardId;
    for (uint8_t i = 0; i < 4; i++)
    {
      byte readCard = mfrc522.uid.uidByte[i];
      char chrHex[2];
      sprintf(chrHex, "%02X", readCard);
      cardId += chrHex;
    }
    mfrc522.PICC_HaltA(); // Stop reading

#ifdef DEBUG
    Serial.println();
    Serial.print(F("cardid:"));
    Serial.println(cardId);
#endif
    if (webClient.connect(conf.serverip, conf.serverport)) // 8080
    {
      webClient.print(F("GET "));
      String request = String(conf.request);
      request.replace(F("%RID%"), getMACasString(conf.mac));
      request.replace(F("%CID%"), cardId);
#ifdef DEBUG
      Serial.println();
      Serial.print(F("request:"));
      Serial.println(request);
#endif
      webClient.print(request);
      webClient.println(F(" HTTP/1.1"));
      webClient.print(F("Host: "));
      String serverIp = doc["serverip"];
      webClient.print(serverIp);
      webClient.println();
      webClient.println(F("Connection: close"));
      webClient.println();
      dataSent = true;
    }
    else
    {
#ifdef DEBUG
      Serial.println(F("connection failed"));
      setColor(0, 0, 255);
      tone(BUZZER_PIN, 500);
      delay(2000);
      noTone(BUZZER_PIN);
      setColor(255, 255, 255);
#endif
    }
  }
}
