#include <SDI12.h>

#define SERIAL_BAUD 115200  // The baud rate for the output serial port
#define DATA_PIN 15         // The pin of the SDI-12 data bus
#define LED_PIN 4

#define ACTION_PIN 2        //pump,powerled
// Define the SDI-12 bus

SDI12 mySDI12(DATA_PIN);

/*
  '?' is a wildcard character which asks any and all sensors to respond
  'I' indicates that the command wants information about the sensor
  '!' finishes the command
*/
String myCommand = "?HELLOSLAVE!";

/*
 * set safe state even if its the sdi master; otherwise normal pcbs might turn on the action
 */
void setSafeState(){
  digitalWrite(ACTION_PIN,LOW);
  digitalWrite(LED_PIN,HIGH);
}

void setup(){
  setSafeState();
  pinMode(LED_PIN, OUTPUT);
  pinMode(ACTION_PIN, OUTPUT);
  
  Serial.begin(SERIAL_BAUD);
  while(!Serial);

  Serial.println("Opening SDI-12 bus master...");
  mySDI12.begin();
  delay(500); // allow things to settle
}

void loop(){
  /*
  mySDI12.sendCommand(myCommand);
  delay(100);                     // wait a while for a response
  while(mySDI12.available()){    // write the response to the screen
    Serial.write(mySDI12.read());
  }
  */
  if (Serial.available() > 0) {
    digitalWrite(4, LOW);
    
    /*
    while(Serial.available() > 0) {
      char t = Serial.read();
      Serial.print(t);
    }*/

    
    myCommand=Serial.readStringUntil('\n');
    
    Serial.print("Sending ");
    Serial.println(myCommand);
    
    mySDI12.sendCommand(myCommand);
    delay(100);                     // wait a while for a response
    if(mySDI12.available()){
      while(mySDI12.available()){    // write the response to the screen
        char sdiChar=mySDI12.read();
        if(sdiChar!=127){ //remove marking if present
          Serial.print(sdiChar);
        }
        //Serial.write(mySDI12.read());
      }
      Serial.print("\r\n");
    }

  
  }
  digitalWrite(4, HIGH);
  setSafeState(); //sometimes power pin is turned on without a reason. 
}
