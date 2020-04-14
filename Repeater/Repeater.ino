#include <SPI.h>
#include <LoRa.h>


//
// This is our Repeater ID
// Is must be unique
//
#define MYID "EIZRSL"

#define CMD_UNKNOWN 0
#define CMD_PING 1
#define CMD_REPEAT 2


const long frequency = 433E6;  // LoRa Frequency

const int csPin = A5;          // LoRa radio chip select
const int resetPin = A4;        // LoRa radio reset
const int irqPin = 2;          // change for your board; must be a hardware interrupt pin

int counter = 0;



typedef struct {
  char sender[7];
  int cmd;
  char payload[255];
  bool validated;
  bool iscrc;
  char crc[3];
  bool crcgood;
} Cmd;



void parse_msg(char *msg, Cmd *cmd);
void handle_ping(Cmd *cmd, int sig);
int get_battery_voltage();
void generateCRC(char *msg, char *readablecrcvalue, boolean withbrackets);
uint8_t gencrc(char *data, size_t len);
void send_lora_msg(char *msg);

void setup() {
  Serial.begin(9600);
  while (!Serial);

  Serial.println("LoRa Radio Starting...");

  LoRa.setPins(csPin, resetPin, irqPin);
  
  if (!LoRa.begin(433E6)) {
    Serial.println("Starting LoRa Radio failed (sad face)!");
    while (1);
  }

  Serial.println("Lora Radio Started!");
  Serial.print("Current Battery Voltage:");
  Serial.print(get_battery_voltage());
  Serial.println("v");
  
}

void loop() {
  int packetsize;
  int sig;

  Cmd cmd;

  char msg[255];
  int msgcursor;


  while (true) {
    packetsize = LoRa.parsePacket();
    if (packetsize) {
      msgcursor = 0;
      while (LoRa.available()) {
        if (msgcursor<=250) {
          msg[msgcursor++] = (char)LoRa.read();  
        }
        else {
          // Dump the data (> 250 bytes) (bad I know!)
          // I think LoRa packets are always MAX 255 bytes ?
          LoRa.read();
        }
      }
      msg[msgcursor++] = '\0';
        
      sig = LoRa.packetRssi();

      Serial.printf("MSG IN: %s\r\n", msg);
        
      parse_msg(msg, &cmd);
      if (cmd.cmd != CMD_UNKNOWN && cmd.validated) {
        switch (cmd.cmd) {
          case CMD_PING:
            Serial.printf("PING command!\r\n");
            handle_ping(&cmd, sig);
            break;
          case CMD_REPEAT:
            Serial.printf("REPEAT command!\r\n");
            handle_repeat(&cmd);
            break;         
        }
      }
    }
  }
}

/*
 * Function : parse_cmd
 * --------------------
 * Parses a received messsage and looks to see if we can find a
 * valid sender callsign and command
 * 
 * Note: a valid message is something like "EI9HBB: ping EIZRSL"
 *                                          SENDER  CMD  RECIPIENT
 * 
 * Returns: nothing (void) but does popular the [Cmd] cmd structure
 *          with sender callsign, a valid command (if found) and 
 *          the remainder of the message (payload) if any
 */
void parse_msg(char *msg, Cmd *cmd) {
  int wordcounter;
  char tmpcrc[3];
  
  // Defaults (no cmd)
  strcpy(cmd->sender, "");
  cmd->cmd = CMD_UNKNOWN;
  strcpy(cmd->payload, "");
  cmd->validated = false;
  cmd->iscrc = false;
  strcpy(cmd->crc, "00");
  cmd->crcgood = false;

  // is msg len > 4 - is last 4 characters something like [DD] ?
  if (strlen(msg)>4 && msg[strlen(msg)-4] == '[' && msg[strlen(msg)-1] == ']') {
    cmd->crc[0] = msg[strlen(msg)-3];
    cmd->crc[1] = msg[strlen(msg)-2];
    cmd->crc[2] = '\0';
    cmd->iscrc = true;
    msg[strlen(msg)-4] = '\0'; // Chop off the CRC from the end of the MSG
    // Is CRC Good ?
    generateCRC(msg, tmpcrc, false);
    if (strcmp(tmpcrc, cmd->crc) == 0) {
      cmd->crcgood = true; // YEY - GOOD CRC
    }
  }
  // Split on spaces and look for sender/cmd/payload
  wordcounter = 0;
  char *tok = NULL;
  tok = strtok(msg, " ");
  while (tok != NULL) {
    // If 1st word is something like "IE9HBB:" then it looks look
    if (wordcounter == 0) {
      if (strlen(tok) == 7 && tok[6] == ':') {
        // Looks like a good sender id
        strcpy(cmd->sender, tok);
        cmd->sender[6]= '\0'; // Chop off the :
      }
      else {
        // Err: 1st word MUST be the Callsign of the sender
        cmd->validated = false;
        return;
      }
    }
    
    if (wordcounter == 1) {
      // 2nd word sould be the command
      if (strcmp(tok, "ping") == 0) {
        Serial.println("CMD is ping!");
        // PING !
        cmd->cmd = CMD_PING; // Looking good so far
      }
      else if (strcmp(tok, "repeat") == 0) {
        // REPEAT COMMAND !
        cmd->cmd = CMD_REPEAT; // No more mandatory fields for repeat command
        cmd->validated = true;
      }
      else {
        // Err: 2nd word MUST be a valid command
        cmd->validated = false;
        return;
      }
    }
    
    //If CMD == PING then 3rd word should be our 6 digit callsign  
    if (wordcounter == 2 && cmd->cmd == CMD_PING && strcmp(tok, MYID) == 0 ) {
      // Looks Good !
      cmd->validated = true;
      return; // No payload for ping required
    }
        
    // Payload (for the repeat command)
    if (strlen(cmd->payload) > 0) {
      strcat(cmd->payload, " ");
    }
    strcat(cmd->payload, tok);

    wordcounter++;
    tok = strtok(NULL, " ");
  }
}


void handle_repeat(Cmd *cmd) {
  char retstr[255]; // Max 255 return string
  char crchex[5];   // e.g. [A9] - 4 characters + \0
  char myid_search_pattern[8]; // ~ + 6chars + \0

  // Expected MSG format: "EI9HBB: RELAY HELLO WORLD~EIZRSL[FF]"
  // Note: At the end of the payload message there will be the callsign of any other repeaters
  //       tilda seperated.  If the whole message becomes larger than 255 characters
  //       the payload should be reduced in size.

  if (!cmd->iscrc) {
    // No CRC - Refusing to REPEAT
    sprintf(retstr, "%s: relay error! No CRC in receivd message (CRC is mandatory for repeat command). Replying to %s.", MYID, cmd->sender);    
  }
  else {
    if (!cmd->crcgood) {
      // Invalid CRC - Refusing to REPEAT
      sprintf(retstr, "%s: relay error! Invalid CRC in receivd message (maybe message is corrupt?). RECV CRC: %s. Replying to %s.", MYID, cmd->crc, cmd->sender);
    }
    else {
      // Am I already in the relay list ?
      sprintf(myid_search_pattern, "~%s", MYID);
      if (strstr(cmd->payload, myid_search_pattern)) {
         // We are already in the list - DONT repeat
         Serial.printf("REPEAT: NOT Repeating because are in the repeater list\r\n");
         return;
      }
      else {
        // Add our selves to the message
        strcpy(retstr, cmd->payload);
        strcat(retstr, myid_search_pattern);
      }
    }
  }
     
  // CRC on our response  
  generateCRC(retstr, crchex, true);
  strcat(retstr, crchex);
   
  // Send response over-the-air
  send_lora_msg(retstr);
}



void handle_ping(Cmd *cmd, int sig) {
  char retstr[255]; // Max 255 return string
  char crchex[5];   // e.g. [A9] - 4 characters + \0
  
  Serial.println("ping received!");

  // Response
  sprintf(retstr, "%s: pong!  Replying to %s.  I see you @ %d dBm.  My battery is @ %dmV right now.", MYID, cmd->sender, sig, get_battery_voltage());
  generateCRC(retstr, crchex, true);
  strcat(retstr,crchex);

  // Send response over-the-air
  send_lora_msg(retstr);
}

int get_battery_voltage() {
  analogRead(6);
  bitSet(ADMUX, 3);
  delayMicroseconds(250);
  bitSet(ADCSRA, ADSC);
  while (bit_is_set(ADCSRA, ADSC)) {
  }
  word x = ADC;
  return x ? ((1100L * 1023) / x) : -1;
}

void generateCRC(char *msg, char *readablecrcvalue, boolean withbrackets) {
  uint8_t crc;
 
  crc = gencrc(msg, strlen(msg));
  if (withbrackets) {
    sprintf(readablecrcvalue, "[%02X]", crc);
  }
  else {
    sprintf(readablecrcvalue, "%02X", crc);
  }
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

void send_lora_msg(char *msg) {
  LoRa.beginPacket();
  LoRa.print(msg);
  LoRa.endPacket();
  Serial.printf("MSG OUT: %s\r\n", msg);
}
