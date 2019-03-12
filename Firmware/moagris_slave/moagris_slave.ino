#include <SDI12.h>

#define HEARTBEATSAFESTATE 1
//recommended value is 20000UL and do heartbeat every 15000ms
#define HEARTBEATINTERVAL 150543560UL

#define SDICMDIDENTIFICATION 'I'
#define SDICMDCHANGEADDR 'C'
#define SDICMDHEARTBEAT 'H'
#define SDICMDTEST 'T'
#define SDICMDSTATUSLED 'S'
#define SDICMDPOWERLED 'L'
#define SDICMDPUMP 'P'
#define SDICMDFAN 'F'

#define LED_PIN 4
#define ACTION_PIN 2        //pump,powerled
#define DATA_PIN 15         // The pin of the SDI-12 data bus

char sensorAddress = '5'; //TODO read from eeprom
int state = 0;

#define WAIT 0
#define INITIATE_CONCURRENT 1
#define INITIATE_MEASUREMENT 2

#define RESPONSESIZE 40

/*
//define response delimiter size to avoid sizeof()
#define RESPDELIMITERSIZE 3
//define response delimiter to (maybe!) reduce flash size
static char respDelimiter[] = "\r\n\0";
*/

// Create object by which to communicate with the SDI-12 bus on SDIPIN
SDI12 slaveSDI12(DATA_PIN);

union unionResponse{
  char c[RESPONSESIZE]; //whole response as char
  struct structIdentification{ //I
    char address;
    unsigned char serialnumber[4];
  }sIdentification;
} uResponse;

unsigned long hbPreviousMillis = millis(); //milliSeconds for heartbeat
unsigned long hbInterval = HEARTBEATINTERVAL; //Interval to declare heartbeat missing; save as variable to set higher via sdi //TODO do so
bool hbAvailable = true; //heartbeatflag

//adds \r\n\0 to response at given address
void finishResponse(char* start){
  strncpy(start,"\r\n\0",3);
}

void setSafeState(){
  digitalWrite(ACTION_PIN,LOW);
  digitalWrite(LED_PIN,HIGH);
}

void setup() {
  setSafeState();
  pinMode(LED_PIN, OUTPUT);
  pinMode(ACTION_PIN, OUTPUT);
  Serial.begin(115200);
  //while(!Serial);

  Serial.println("Opening SDI-12 bus slave...");
  slaveSDI12.begin();
  delay(500);
  slaveSDI12.forceListen();  // sets SDIPIN as input to prepare for incoming message
  //memset(&uResponse,0,sizeof(uResponse));
}



void loop() {

  static String commandReceived = "";  // String object to hold the incoming command
  unsigned long hbCurrentMillis = millis(); //current millis for heatbeat
  

  // If a byte is available, an SDI message is queued up. Read in the entire message
  // before proceding.  It may be more robust to add a single character per loop()
  // iteration to a static char buffer; however, the SDI-12 spec requires a precise
  // response time, and this method is invariant to the remaining loop() contents.
  int avail = slaveSDI12.available();
  if (avail < 0) { slaveSDI12.clearBuffer(); } // Buffer is full; clear
  else if (avail > 0) {
    //digitalWrite(4, HIGH);
    for(int a = 0; a < avail; a++){
      char charReceived = slaveSDI12.read();
      //Serial.println(charReceived);
      // Character '!' indicates the end of an SDI-12 command; if the current
      // character is '!', stop listening and respond to the command
      if (charReceived == '!') {

        //only react on wildcard or sensorAddress and command is not a heartbeat
        if((commandReceived[0]=='?' || commandReceived[0]==sensorAddress) && commandReceived.charAt(1)!=SDICMDHEARTBEAT){

          Serial.print("rcvd end of cmd ");
          Serial.println(commandReceived);
        
          //commandReceived.remove(0,1); //removing address for further processing
          
          uResponse.sIdentification.address=sensorAddress;
          byte finishOffset=0;
          byte intensity = 0;
          //Serial.println(commandReceived);
          switch(commandReceived.charAt(1)){ //if/elseif instead of switch would save 8 bytes
            case SDICMDIDENTIFICATION: //serial number of samd11
              //TODO should be 8 chars long
              uResponse.sIdentification.serialnumber[0]=pgm_read_word(0x0080A00C);
              uResponse.sIdentification.serialnumber[1]=pgm_read_word(0x0080A040);
              uResponse.sIdentification.serialnumber[2]=pgm_read_word(0x0080A044);
              uResponse.sIdentification.serialnumber[3]=pgm_read_word(0x0080A048);
              finishOffset=sizeof(uResponse.sIdentification)-1;
              break;
            case SDICMDTEST: //test
              strcpy(uResponse.c+1,"abcd");
              finishOffset=3;
              break;
            case SDICMDSTATUSLED: // set status LED to value (char)
              digitalWrite(LED_PIN, (byte)commandReceived.charAt(2) - 48);
              strcpy(uResponse.c+1,"ACK");
              finishOffset=1;
              break;
            case SDICMDFAN:  
            case SDICMDPOWERLED: // set powerled to value (char) 
            
              //chars 2..4 are acii coded values between 0..255
              //intensity = commandReceived.substring(2,4).toInt(); //uses ~700 Bytes!
              intensity = 0xff & ((byte)commandReceived.charAt(2)*100 + (byte)commandReceived.charAt(3)*10 + (byte)commandReceived.charAt(4)); //command must have 3 chars of value (leading 0s if necessary)
              
              analogWrite(ACTION_PIN, intensity); //uses ~550 Bytes!
              
              strcpy(uResponse.c+1,"ACK");
              finishOffset=1;
              break;
            case SDICMDPUMP: // set pump to value (char)
              digitalWrite(ACTION_PIN, (byte)commandReceived.charAt(2) - 48);
              strcpy(uResponse.c+1,"ACK");
              finishOffset=1;
              break;
            case SDICMDCHANGEADDR: // change sdi address
              //TODO write to eeprom
              Serial.println("set new address");
              sensorAddress=commandReceived.charAt(2);
              uResponse.sIdentification.address=sensorAddress;
              strcpy(uResponse.c+1,"ACK");
              finishOffset=1;
              break;

            //not needed
            /*
            case SDICMDHEARTBEAT: //do nothing but send ACK and reset heartbeatcounter afterwards as usual
              strcpy(uResponse.c+1,"ACK");
              finishOffset=1;
              break;*/
            /*case SDICMDFAN:
              //chars 2..4 are acii coded values between 0..255
              intensity = commandReceived.substring(2,4).toInt();
              analogWrite(ACTION_PIN, intensity);
              strcpy(uResponse.c+1,"ACK");
              finishOffset=1;
              break;*/
            default:
              strcpy(uResponse.c+1,"NAK");
              finishOffset=1;
              break;
          }

          //sending
          finishResponse((char*)(&uResponse+finishOffset));
          Serial.println(uResponse.c);
          slaveSDI12.sendResponse(uResponse.c);
        }

        //reset heartbeat on every command, also for other sensors
        hbPreviousMillis = hbCurrentMillis; //resets heartbeat
        hbAvailable=true; //new command resets heartbeat
        
        commandReceived = "";
        slaveSDI12.clearBuffer();
        break;
      }
      // If the current character is anything but '!', it is part of the command
      // string.  Append the commandReceived String object.
      else{
        // Append command string with new character
        commandReceived += String(charReceived);
      }
    }
  }

  //Heartbeat - return to savestate if heart didn't beat in the last hbInterval
#if HEARTBEATSAFESTATE 
  if (hbAvailable && (hbCurrentMillis - hbPreviousMillis >= hbInterval)) {
    //Serial.println("heartbeat missing");
    hbAvailable=false;
    setSafeState();
  }
#endif
}
