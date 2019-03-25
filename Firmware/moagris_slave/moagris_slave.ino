#include <SDI12.h>

#define FIRMWAREVERSION "00.00.01"

//enable (1) to activate heartbeat
#define HEARTBEATSAFESTATE 1
//recommended value is 20000UL and do heartbeat every 15000ms
#define HEARTBEATINTERVAL 150543560UL

#define SDICMDIDENTIFICATION 'I'
#define SDICMDVERSION 'V'
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



#define WAIT 0
#define INITIATE_CONCURRENT 1
#define INITIATE_MEASUREMENT 2

#define COMMANDSIZE 20
#define RESPONSESIZE 40

#define RESPENDSIZE 3
#define RESPEND "\r\n\0"

// Create object by which to communicate with the SDI-12 bus on SDIPIN
SDI12 slaveSDI12(DATA_PIN);

union unionSensorAddress{
  uint16_t w;
  char c[2];
  byte b[2];
} uSensorAddress;

union unionResponse{
  char c[RESPONSESIZE]; //whole response as char
  struct structSdiResponse{
    char address[4];
    char responseContent[RESPONSESIZE-sizeof(unionSensorAddress)];
  }sdiResponse;
} uResponse;


unsigned long hbPreviousMillis = millis(); //milliSeconds for heartbeat
unsigned long hbInterval = HEARTBEATINTERVAL; //Interval to declare heartbeat missing; save as variable to set higher via sdi //TODO do so
bool hbAvailable = true; //heartbeatflag


//helper function for hex codes
byte ctob(char c){
  if(c>47 && c<58)
    return c-48;
  else if(c>96 && c<103)
    return c-87; //-97-10
  else 
    return 0;
}
char btoc(byte b){
  if(b<10)
    return b+48;
  else if(b>=10 && b<17)
    return b+87;
}

//adds \r\n\0 to response at given address
void finishResponse(char* start){
  strncpy(start,RESPEND,3);
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

  //getting address by serialnumber
  //2 byte of 4 xored serial number words
  uSensorAddress.w=((pgm_read_word(0x0080A00C) ^ pgm_read_word(0x0080A040)) ^ pgm_read_word(0x0080A044)) ^ pgm_read_word(0x0080A048);
  //while(!Serial);
  Serial.println("Opening SDI-12 bus slave...");
  slaveSDI12.begin();
  delay(500);
  slaveSDI12.forceListen();  // sets SDIPIN as input to prepare for incoming message
  //memset(&uResponse,0,sizeof(uResponse));
}




void loop() {

  //static String commandReceived = "";  // String object to hold the incoming command; code with string would use ~ 800Bytes!
  static char commandReceived[COMMANDSIZE];
  static char *commandWritePtr=commandReceived;
  unsigned long hbCurrentMillis = millis(); //current millis for heatbeat
  

  // If a byte is available, an SDI message is queued up. Read in the entire message
  // before proceding.  It may be more robust to add a single character per loop()
  // iteration to a static char buffer; however, the SDI-12 spec requires a precise
  // response time, and this method is invariant to the remaining loop() contents.
  int avail = slaveSDI12.available();
  if (avail < 0) { slaveSDI12.clearBuffer(); } // Buffer is full; clear
  else if (avail > 0) {
    for(int a = 0; a < avail; a++){
      char charReceived = slaveSDI12.read();
      // Character '!' indicates the end of an SDI-12 command; if the current
      // character is '!', stop listening and respond to the command
      if (charReceived == '!') {

        unionSensorAddress uCmdAddr;
        char *commandReadPtr=commandReceived;
        
        if(*commandReadPtr!='?'){ //first 4 chars are address
          //convert 
          uCmdAddr.b[0]=(ctob(commandReceived[1])<<4)+ctob(commandReceived[0]);
          uCmdAddr.b[1]=(ctob(commandReceived[3])<<4)+ctob(commandReceived[2]);       
          commandReadPtr+=4;
        }else{
          uCmdAddr.w=uSensorAddress.w; //if wildcard address use own
          commandReadPtr++;
        }
        
        //only react on wildcard or sensorAddress and command is not a heartbeat
        if(uCmdAddr.w==uSensorAddress.w && *commandReadPtr!=SDICMDHEARTBEAT){

          uResponse.sdiResponse.address[0]=btoc(uSensorAddress.c[0]&0xf);
          uResponse.sdiResponse.address[1]=btoc((uSensorAddress.c[0]&0xf0)>>4);
          uResponse.sdiResponse.address[2]=btoc(uSensorAddress.c[1]&0xf);
          uResponse.sdiResponse.address[3]=btoc((uSensorAddress.c[1]&0xf0)>>4);
          
          byte finishOffset=0;
          byte intensity = 0;
          //Serial.println(commandReceived);
          switch(*commandReadPtr++){ //if/elseif instead of switch would save 8 bytes
            case SDICMDIDENTIFICATION: //xored serial number of samd11; blank responseContent                 
              finishOffset=0;
              break;
            case SDICMDVERSION:
              strcpy(uResponse.sdiResponse.responseContent,FIRMWAREVERSION);
              finishOffset=sizeof(FIRMWAREVERSION);
              break;
            case SDICMDSTATUSLED: // set status LED to value (char) [S]
              digitalWrite(LED_PIN, ctob(*commandReadPtr));
              strcpy(uResponse.sdiResponse.responseContent,"ACK");
              finishOffset=3;
              break;
            case SDICMDFAN:  // set fan to value (char) 
            case SDICMDPOWERLED: // set powerled to value (char) 
            
              //chars 2..4 are acii coded values between 0..255
              //intensity = commandReceived.substring(2,4).toInt(); //uses ~700 Bytes!
              intensity = 0xff & ((byte)*commandReadPtr++*100 + (byte)*commandReadPtr++*10 + (byte)*commandReadPtr++); //command must have 3 chars of value (leading 0s if necessary)
              
              analogWrite(ACTION_PIN, intensity); //uses ~550 Bytes!
              
              strcpy(uResponse.sdiResponse.responseContent,"ACK");
              finishOffset=3;
              break;
            case SDICMDPUMP: // set pump to value (char)
              digitalWrite(ACTION_PIN, ctob(*commandReadPtr));
              strcpy(uResponse.sdiResponse.responseContent,"ACK");
              finishOffset=3;
              break;
            default:
              strcpy(uResponse.sdiResponse.responseContent,"NAK");
              finishOffset=3;
              break;
          }

          //sending
          finishResponse((uResponse.sdiResponse.responseContent+finishOffset));
          Serial.print(uResponse.c);//no \r\n since it will be added by finishResponse
          slaveSDI12.sendResponse(uResponse.c);
        }

        //reset heartbeat on every command, also for other sensors
        hbPreviousMillis = hbCurrentMillis; //resets heartbeat
        hbAvailable=true; //new command resets heartbeat

        //reset command and ptr
        //commandReceived = "";
        memset(commandReceived,0,COMMANDSIZE);
        commandWritePtr=commandReceived;
        
        slaveSDI12.clearBuffer();
        break;
      }
      // If the current character is anything but '!', it is part of the command
      // string.  Append the commandReceived String object.
      else{
        // Append command string with new character
        if((byte)charReceived!=0){ //ignore marking/spacing
          *commandWritePtr=charReceived; //TODO check too long command
          commandWritePtr++;
        }
      }
    }
  }

  //Heartbeat - return to savestate if heart didn't beat in the last hbInterval
#if HEARTBEATSAFESTATE 
  if (hbAvailable && (hbCurrentMillis - hbPreviousMillis >= hbInterval)) {
    hbAvailable=false;
    setSafeState();
  }
#endif
}
