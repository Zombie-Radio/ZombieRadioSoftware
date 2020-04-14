#include <SPI.h>
#include <LoRa.h>

/*
 * Test for the lead-acid LORA thing
 */

#define MYID "EI9HBB"
 
const long frequency = 433E6;  // LoRa Frequency

const int csPin = 10;          // LoRa radio chip select
const int resetPin = 5;        // LoRa radio reset
const int irqPin = 2;          // change for your board; must be a hardware interrupt pin

int counter = 0;

void generate_crc(char *msg, char *readablecrcvalue);
uint8_t gencrc(char *data, size_t len);


void setup() {
  char msg[255];
  char msgcrc[5]; // [XX]\0
  
  Serial.begin(9600);
  while (!Serial);

  Serial.println("LoRa Starting...");

  LoRa.setPins(csPin, resetPin, irqPin);
  
  if (!LoRa.begin(433E6)) {
    Serial.println("Starting LoRa failed!");
    while (1);
  }

  Serial.println("Lora Started!");

  Serial.println("Waiting for 3 seconds...");


  /**
  Serial.println("Sending PING...");
  LoRa.beginPacket();
  LoRa.print(MYID);
  LoRa.print(": ping EIZRSL");
  LoRa.endPacket();
  Serial.println("Done.");
  **/


  // Ping a repeater message
  //sprintf(msg, "%s: ping EIZRSL", MYID);



  // Example of a message that hasn't been repeated yet...
  sprintf(msg, "%s: repeat Hello World", MYID);

 
  // Example of a message that's been repeated via 3 repeaters...
  //sprintf(msg, "%s: repeat Hello World~EIZTST~EIZRSL~EIZ333", MYID);


  // Repeating requires a CRC (e.g. [0d]) add to the end of the message
  // We use this to be sure the message isn't corrupted over-the-air
  generate_crc(msg, msgcrc);
  strcat(msg, msgcrc);


  // BAD CRC MESSAGE
  //sprintf(msg, "%s: repeat Hello World[00]", MYID);
  Serial.print("Sending: ");
  Serial.println(msg);
  
  LoRa.beginPacket();
  LoRa.print(msg);
  LoRa.endPacket();
  Serial.println("Done.");


  
}
void generate_crc(char *msg, char *readablecrcvalue) {
  uint8_t crc;
 
  crc = gencrc(msg, strlen(msg));
  sprintf(readablecrcvalue, "[%02X]", crc);
}

uint8_t gencrc(char *data, size_t len) {
  uint8_t crc = 0xff;
  size_t i, j;
  for (i = 0; i < len; i++) {
    crc ^= data[i];
    for (j = 0; j < 8; j++) {
      if ((crc & 0x80) != 0) {
        crc = (uint8_t)((crc << 1) ^ 0x31);
      }
      else {
        crc <<= 1;
      }
    }
  }
  return crc;
}

void loop() {
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    // received a packet
    Serial.print("Received packet '");

    // read packet
    while (LoRa.available()) {
      Serial.print((char)LoRa.read());
    }

    // print RSSI of packet
    Serial.print("' with RSSI ");
    Serial.println(LoRa.packetRssi());
  } 
  /** 
  if (counter%10 == 0) {
    Serial.print("Counter is ");
    Serial.print(counter);
    Serial.println(" so sending PING request to IEZRSL now...");
    LoRa.beginPacket();
    LoRa.print("IE9HBB PING IEZRSL");
    LoRa.endPacket();
  }
  counter++;
  delay(1000);
  **/
}
