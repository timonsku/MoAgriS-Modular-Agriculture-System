#include <SDI12.h>

/**
   -Avoided String type since it uses way too much flash
*/

#define FIRMWAREVERSION "00.00.02"

//recommended value is 20000UL and do heartbeat every 15000ms
#define HEARTBEATINTERVAL 150543560UL //long heartbeat for development

//defines the timeout this sensor waits for another sensors response
#define SDIWAITTIMEOUT 1000UL

#define SDICMDIDENTIFICATION 'I'
#define SDICMDVERSION 'V'
#define SDICMDCHANGEADDR 'C'
#define SDICMDHEARTBEAT 'H'
#define SDICMDSETHEARTBEATINTERVALL 'J'
#define SDICMDSETSDIGROUP 'G'
#define SDICMDGETSDIGROUP 'K'
#define SDICMDTEST 'T'
#define SDICMDSTATUSLED 'S'
#define SDICMDPOWERLED 'L'
#define SDICMDPUMP 'P'
#define SDICMDFAN 'F'

//Groups must be upper case
#define SDIGROUPNONE 'N'
#define SDIGROUPDEFAULT SDIGROUPNONE

#define LED_PIN 4           //status led
#define ACTION_PIN 2        //pump,powerled
#define DATA_PIN 15         //SDI-12 data bus



#define WAIT 0
#define INITIATE_CONCURRENT 1
#define INITIATE_MEASUREMENT 2

#define SDICMDSIZE 20
#define RESPONSESIZE 40

#define RESPENDSIZE 3
#define RESPEND "\r\n\0"

// Create object by which to communicate with the SDI-12 bus on SDIPIN
SDI12 slaveSDI12(DATA_PIN);

union unionSensorAddress {
  uint16_t w;
  char c[2];
  byte b[2];
} uSensorAddress;

union unionResponse {
  char c[RESPONSESIZE]; //whole response as char
  struct structSdiResponse {
    char address[4];
    char responseContent[RESPONSESIZE - sizeof(unionSensorAddress)];
  } sdiResponse;
} uResponse;

//static String commandReceived = "";  // String object to hold the incoming command; code with string type would use ~ 800Bytes!
static char commandReceived[SDICMDSIZE];
static char *commandWritePtr;
static char *commandReadPtr;
static bool sdiWaitUntilFinish = false;
static unsigned long sdiWaitTimeoutMillis = 0; //millis for timer to timeout waiting for other sensor to finish command
static unsigned long hbPreviousMillis = millis(); //milliSeconds for heartbeat
static unsigned long hbInterval = HEARTBEATINTERVAL; //Interval to declare heartbeat missing; save as variable to set higher via sdi //TODO do so
static bool hbAvailable = true; //heartbeatflag
static char sdiGroup = SDIGROUPDEFAULT;


//helper function for hex codes
byte ctob(char c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  //return c-48;
  else if (c >= 'a' && c <= 'f')
    return c - 87; //-'a'-10
  else
    return 0;
}
char btoc(byte b) {
  if (b < 10)
    return b + '0';
  else if (b >= 10 && b < 17)
    return b + 87;
  else
    return 0;
}

//adds \r\n\0 to response at given address
void finishResponse(char* start) {
  strncpy(start, RESPEND, 3);
}

void setSafeState() {
  digitalWrite(ACTION_PIN, LOW);
  analogWrite(ACTION_PIN, 0);
  digitalWrite(LED_PIN, HIGH);
}



void resetSdiBuffer() {
  memset(commandReceived, 0, SDICMDSIZE);
  commandWritePtr = commandReceived;
  commandReadPtr = commandReceived;
  slaveSDI12.clearBuffer();
}

void setup() {
  setSafeState();
  pinMode(LED_PIN, OUTPUT);
  pinMode(ACTION_PIN, OUTPUT);
  Serial.begin(115200);
  //while(!Serial);

  //getting address by serialnumber
  //2 byte of 4 xored serial number words
  uSensorAddress.w = ((pgm_read_word(0x0080A00C) ^ pgm_read_word(0x0080A040)) ^ pgm_read_word(0x0080A044)) ^ pgm_read_word(0x0080A048);
  //while(!Serial);
  Serial.println("Opening SDI-12 bus slave...");
  slaveSDI12.begin();
  delay(500);
  slaveSDI12.forceListen();  // sets SDIPIN as input to prepare for incoming message
  resetSdiBuffer();
}


void loop() {

  //unsigned long hbCurrentMillis = millis(); //current millis for heatbeat


  //if sensor is waiting and timeout occurs then reset buffer
  if (sdiWaitUntilFinish && (millis() - sdiWaitTimeoutMillis) >= SDIWAITTIMEOUT) {
    //Serial.print("timeout: ");Serial.print(millis());Serial.print("-");Serial.println(sdiWaitTimeoutMillis);
    sdiWaitUntilFinish = false;
    resetSdiBuffer();
  }

  int avail = slaveSDI12.available();
  if (avail < 0) {
    slaveSDI12.clearBuffer(); // Buffer is full; clear
  } else if (avail > 0) {
    for (int a = 0; a < avail; a++) {
      char charReceived = slaveSDI12.read();

      //null seems to be a character - ignore it
      if(charReceived == 0){
        continue;
      }

      //Serial.print("charReceived:"); Serial.println(charReceived);
      // Append command string with new character
      *commandWritePtr = charReceived; //TODO check too long command
  
      
      //if sensor is waiting and the other sensor finished its command then reset buffer
      if (sdiWaitUntilFinish && *(commandWritePtr - 1) == '\r' && *(commandWritePtr) == '\n') {
        sdiWaitUntilFinish = false;
        resetSdiBuffer();
        continue; //no command can be sent in between so we can continue here
      }
      
      commandWritePtr++;

      // Character '!' indicates the end of an SDI-12 command; if the current
      // character is '!', stop listening and respond to the command
      if (charReceived == '!') {
        unionSensorAddress uCmdAddr;
        bool isGroupCmd = false;

        //Either first char is wildcard ? or an upper case letter for group A-Z
        //or the first 4 chars are address
        if (*commandReadPtr == '?' || (commandReceived[0] >= 'A' && commandReceived[0] <= 'Z')) {
          //Serial.print("b.0");
          isGroupCmd = true;
          if (*commandReadPtr == '?' || *commandReadPtr == sdiGroup) {
            uCmdAddr.w = uSensorAddress.w;
          }
          commandReadPtr++;
        } else {
          //convert address
          uCmdAddr.b[0] = (ctob(commandReceived[1]) << 4) + ctob(commandReceived[0]);
          uCmdAddr.b[1] = (ctob(commandReceived[3]) << 4) + ctob(commandReceived[2]);
          commandReadPtr += 4;
        }

        //only act if command is not a heartbeat
        if (*commandReadPtr != SDICMDHEARTBEAT) {
          //only if this sensor is addressed by wildcard or sensorAddress
          if (uCmdAddr.w == uSensorAddress.w) {
            uResponse.sdiResponse.address[0] = btoc(uSensorAddress.c[0] & 0xf);
            uResponse.sdiResponse.address[1] = btoc((uSensorAddress.c[0] & 0xf0) >> 4);
            uResponse.sdiResponse.address[2] = btoc(uSensorAddress.c[1] & 0xf);
            uResponse.sdiResponse.address[3] = btoc((uSensorAddress.c[1] & 0xf0) >> 4);

            byte finishOffset = 0;
            byte intensity = 0;
            switch (*commandReadPtr++) { //if/elseif instead of switch would save 8 bytes
              case SDICMDIDENTIFICATION: //xored serial number of samd11; blank responseContent
                finishOffset = 0;
                break;
              case SDICMDVERSION:
                strcpy(uResponse.sdiResponse.responseContent, FIRMWAREVERSION);
                finishOffset = sizeof(FIRMWAREVERSION);
                break;
              case SDICMDSTATUSLED: // set status LED to value (char) [S]
                digitalWrite(LED_PIN, ctob(*commandReadPtr));
                strcpy(uResponse.sdiResponse.responseContent, "ACK");
                finishOffset = 3;
                break;
              case SDICMDFAN:  // set fan to value (char)
              case SDICMDPOWERLED: // set powerled to value (char)

                //chars 2..4 are acii coded values between 000..255 (leading 0s); not using atoi since it uses 500bytes
                intensity = 0xff & ((byte)(*commandReadPtr++ - '0') * 100 + (byte)(*commandReadPtr++ - '0') * 10 + (byte)(*commandReadPtr++ - '0')); 

                analogWrite(ACTION_PIN, intensity); //uses ~550 Bytes!

                strcpy(uResponse.sdiResponse.responseContent, "ACK");
                finishOffset = 3;
                break;
              case SDICMDPUMP: // set pump to value (char)
                digitalWrite(ACTION_PIN, ctob(*commandReadPtr));
                strcpy(uResponse.sdiResponse.responseContent, "ACK");
                finishOffset = 3;
                break;
              case SDICMDSETHEARTBEATINTERVALL: //set new heartbeat intervall in s
                hbInterval = (*commandReadPtr++ - '0') * 1000 + (*commandReadPtr++ - '0') * 100 + (*commandReadPtr++ - '0') * 10 + (*commandReadPtr++ - '0'); //command must have 4 chars of value (leading 0s if necessary)
                hbInterval*=1000;
                strcpy(uResponse.sdiResponse.responseContent, "ACK");
                finishOffset = 3;
                break;
              case SDICMDSETSDIGROUP:
                sdiGroup = *commandReadPtr;
                strcpy(uResponse.sdiResponse.responseContent, "ACK");
                finishOffset = 3;
                break;
              case SDICMDGETSDIGROUP:
                *uResponse.sdiResponse.responseContent=sdiGroup;
                finishOffset=1;
                break;
              default:
                strcpy(uResponse.sdiResponse.responseContent, "NAK");
                finishOffset = 3;
                break;
            }

            //sending via sdi if it's not a group command
            if (!isGroupCmd) {
              finishResponse((uResponse.sdiResponse.responseContent + finishOffset));
              slaveSDI12.sendResponse(uResponse.c);
            } else {
              Serial.print("Not sent on sdi since group cmd: "); //little serial helper text
              isGroupCmd = false;
            }
            Serial.print(uResponse.c);
            
            //reset command and ptr
            resetSdiBuffer();
          } else {
            //start listening to \r\n and start timeout timer to reset buffer
            sdiWaitUntilFinish = true;
            sdiWaitTimeoutMillis = millis();
          }
        }

        //reset heartbeat on every command, also on commands for other sensors
        hbPreviousMillis = millis(); //resets heartbeat
        hbAvailable = true; //new command resets heartbeat



      } 
    }
  }

  //Heartbeat - return to savestate if heart didn't beat in the last hbInterval
  if (hbAvailable && (millis() - hbPreviousMillis >= hbInterval)) { //only jump in here if heartbeat as been available till now (hbAvailable)
    hbAvailable = false;
    setSafeState();
  }
  
}
