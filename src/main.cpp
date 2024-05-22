#include <Arduino.h>
#include <SPI.h>
#include "EEpromWriteAnything.h"
#include <Ethernet.h>
#include <MFRC522.h>
#include <ArduinoJson.h>
JsonDocument doc;

#define SD_SS_PIN 4

#define RST_PIN         9          // Configurable, see typical pin layout above
#define SS_PIN          7         // Configurable, see typical pin layout above
#define DEBUG
#define DHCP
#define RFCReader

#ifdef RFCReader
MFRC522 mfrc522(SS_PIN, RST_PIN);  // Create MFRC522 instance
#endif
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
} conf;


void initEthernet() {
#ifdef DHCP
  if (conf.usedhcp == 1) {
    Serial.println(F("Ethernet configure useing DHCP"));
    Ethernet.begin(conf.mac);
    Ethernet.maintain();
  } else {
#endif
    Serial.println(F("Ethernet configure using fix IP"));
    Ethernet.begin(conf.mac, conf.ip, conf.dnsserver, conf.gateway, conf.subnet);
#ifdef DHCP
  }
#endif
}

String getMACasString(uint8_t* mac) {
  char macno[32];
  sprintf(macno, "%02X-%02X-%02X-%02X-%02X-%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(macno);
}

void setup() {
  // Open serial communications and wait for port to open:
  Serial.begin(115200);
  pinMode(10, OUTPUT);
  digitalWrite(10, HIGH);
  pinMode(7, OUTPUT);
  digitalWrite(7, HIGH);
  pinMode(4, OUTPUT);
  digitalWrite(4, HIGH);

#ifdef DEBUG
  while (!Serial) {
    ;
  }
#endif
  EEPROM_readAnything(1, conf);
  bool configured = String(conf.state).startsWith("CONFIG");
  if (configured) {
    initEthernet();
  } 
  Serial.println(configured?F("Configured"):F("Not confugured yet"));
  
#ifdef DEBUG
  if (configured) {
    Serial.print(F("reader ip: "));
    Serial.println(Ethernet.localIP());
  }
#endif

#ifdef RFCReader
  Serial.println("init RFID reader...");
  mfrc522.PCD_Init();   // Init MFRC522
  #ifdef DEBUG
    mfrc522.PCD_DumpVersionToSerial();  // Show details of PCD - MFRC522 Card Reader details
  #endif
  Serial.println("RFID reader initialized.");
#endif
}

void getMACFromString(String mac, uint8_t* result) {
  for(uint8_t i = 0; i < 6; i++) {
    String part = mac.substring(0,mac.indexOf("-"));
    char * pEnd;
    result[i] = strtol(part.c_str(),&pEnd, 16);
    mac.remove(0,part.length()+1);
  }
}

void printIPToSerial(String ipname, IPAddress addr) {
  char ipno[16];
  sprintf(ipno, "%d.%d.%d.%d", addr[0], addr[1], addr[2], addr[3]);
  Serial.print(F("\""));
  Serial.print(ipname);
  Serial.print(F("\":\""));
  Serial.print(ipno);
  Serial.print(F("\""));
};

void printMACToSerial(String ipname, uint8_t* result) {
  Serial.print(F("\""));
  Serial.print(ipname);
  Serial.print(F("\":\""));
  Serial.print(getMACasString(result));
  Serial.print(F("\""));
}

void getIPFromString(String ip, uint8_t* result, byte length=4) {
  for(uint8_t i; i < length; i++) {
    String part = ip.substring(0,ip.indexOf("."));
    result[i] = part.toInt();
    ip.remove(0,part.length()+1);
  }
}

void printConfigToSerial() {
      Serial.print(F("{"));
      Serial.print(F("state:\""));
      Serial.print(String(conf.state));
      Serial.print(F("\","));
      Serial.print(F("usedhcp:"));
      Serial.print(conf.usedhcp == 1);
      Serial.print(F(","));
      printIPToSerial(F("ip"), conf.ip);
      Serial.print(",");
      printIPToSerial(F("serverip"), conf.serverip);
      Serial.print(",");
      printIPToSerial(F("gateway"), conf.gateway);
      Serial.print(",");
      printIPToSerial(F("dnsserver"), conf.dnsserver);
      Serial.print(",");
      printIPToSerial(F("subnet"), conf.subnet);
      Serial.print(",");
      printMACToSerial(F("mac"), conf.mac);
      Serial.println(F("}"));
};

void updateConf() {
    String state = doc["state"];
    strcpy(conf.state,state.c_str());
#ifdef DEBUG
    Serial.print(F("state:"));
    Serial.println(state);
#endif
    Serial.print(F("ip:"));
    Serial.println(doc[F("ip")].as<String>());
    getIPFromString(doc[F("ip")].as<String>(),conf.ip);

    Serial.print(F("serverip:"));
    Serial.println(doc[F("serverip")].as<String>());
    getIPFromString(doc[F("serverip")].as<String>(),conf.serverip);

    conf.usedhcp = doc[F("usedhcp")].as<int>();
    Serial.print(F("usedhcp:"));
    Serial.println(doc[F("usedhcp")].as<int>());

    Serial.print(F("subnet:"));
    Serial.println(doc[F("subnet")].as<String>());
    getIPFromString(doc[F("subnet")].as<String>(),conf.subnet);

    Serial.print(F("gateway:"));
    Serial.println(doc[F("gateway")].as<String>());
    getIPFromString(doc[F("gateway")].as<String>(),conf.gateway);

    Serial.print(F("dnsserver:"));
    Serial.println(doc[F("dnsserver")].as<String>());
    getIPFromString(doc[F("dnsserver")].as<String>(),conf.dnsserver);

    Serial.print(F("mac:"));
    Serial.println(doc[F("mac")].as<String>());
    getMACFromString(doc[F("mac")].as<String>(), conf.mac);
}

EthernetClient webClient;
bool dataSent = false;
bool inJSON = false;
String webResult = "";
String command = "";

void loop() {
  if (webClient.available()) {
    char buff[64];
    int cnt = webClient.readBytes(buff, sizeof(buff));
    for(int i=0;i<cnt;i++) {
      if (buff[i] == '{' && !inJSON) {
        inJSON = true;
      } else if (buff[i] == '}' && inJSON) {
        inJSON = false;
        webResult += buff[i];
      }
      if (inJSON) {
        webResult += buff[i];
      }
    }
#ifdef DEBUG
    Serial.write(buff, cnt);
#endif
  }

  if (!webClient.connected() && dataSent) {
    webClient.stop();
    dataSent = false;
  }
    // Look for new cards
#ifdef RFCReader  
  if ( mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
    String cardId;
    for ( uint8_t i = 0; i < 4; i++) {
      byte readCard = mfrc522.uid.uidByte[i];
      char chrHex[2];
      sprintf(chrHex, "%02X", readCard);
      cardId += chrHex;
    }
    mfrc522.PICC_HaltA(); // Stop reading
    
#ifdef DEBUG
      printIPToSerial(F("connect to"),conf.serverip);
      Serial.println();
      Serial.print(F("cardid:"));
      Serial.println(cardId);
#endif
    if (webClient.connect(conf.serverip, 8080)) {
      webClient.print(F("GET "));
      String request = String(F("/forras-admin/rest/createNFCLog?readerid=%RID%&rfid=%CID%&type=RF1"));
      request.replace(F("%RID%"),getMACasString(conf.mac));
      request.replace(F("%CID%"),cardId);
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
    } else {
#ifdef DEBUG
      Serial.println(F("connection failed"));
#endif
    }
    
  }
#endif
  while (Serial.available()) {
    char buff[16];
    int cnt = Serial.readBytes(buff, sizeof(buff));
    if (cnt > 0) {
      String part = String(buff).substring(0,cnt);
      command += part;
#ifdef DEBUG
        Serial.print(F("\ncommand part:"));
        Serial.println(part);
#endif
    }
  };
  if (command.endsWith("}")) {
    Serial.print(F("Command received:"));
    Serial.println(command);
    deserializeJson(doc, command);
    if (doc["action"].as<String>().equals(F("configure"))) {
      updateConf();
      EEPROM_writeAnything(1, conf);
    } else if (doc["action"].as<String>().equals(F("readconfig"))) {
      EEPROM_readAnything(1, conf);
      printConfigToSerial();
    };
    command = "";
  };
}
