#include "STM32LowPower.h"

#define SENSOR PA2
#define TIME 5000//300000
#define SELF_ADDRESS 100
#define RELAY_ADDRESS 1
#define SENSOR_ID 5

HardwareSerial LoraSerial = HardwareSerial(USART1);
int sval;

String valstr;
String toSend;

void setup() {
  LowPower.begin();//Init low power library

  //Set pinmodes
  pinMode(SENSOR, INPUT);
  pinMode(LED_BUILTIN, OUTPUT);

  //Opening serial port to lora chip
  //and setting up lora parameters
  digitalWrite(LED_BUILTIN, LOW);
  LoraSerial.begin(115200);
  delay(3000);
  LoraSerial.println("AT+NETWORKID=1");
  delay(1000);
  LoraSerial.print("AT+ADDRESS=");
  LoraSerial.println(SELF_ADDRESS);
  delay(1000);
  LoraSerial.println("AT+PARAMETER=12,7,1,4");
  digitalWrite(LED_BUILTIN, HIGH);
}

void loop() {

  while(!LoraSerial){
      delay(1000);
      //Retrying to init lora serial port
      LoraSerial.begin(115200);
  }

  digitalWrite(LED_BUILTIN, LOW);

  //Read sensor value
  sval = analogRead(SENSOR);

  //Format a string to send with lora
  valstr = String(SENSOR_ID, DEC);                    //5
  valstr.concat("_");                                 //5_
  valstr.concat(String(sval, DEC));                   //5_2048
  valstr.concat("h");                                 //5_2048h
  toSend = String("AT+SEND=");                        //AT+SEND=
  toSend.concat(String(RELAY_ADDRESS, DEC));          //AT+SEND=1
  toSend.concat(",");                                 //AT+SEND=1,
  toSend.concat(String(valstr.length(), DEC));        //AT+SEND=1,7
  toSend.concat(",");                                 //AT+SEND=1,7,
  toSend.concat(valstr);                              //AT+SEND=1,7,5_2048h

  //Queue serial send and wait to complete
  LoraSerial.println(toSend);
  LoraSerial.flush();

  //Debug code
  /*
  if(LoraSerial.available() > 0)
  {
    Serial.println(LoraSerial.readString());
    Serial.flush();
  }
  */
  digitalWrite(LED_BUILTIN, HIGH);

  LowPower.deepSleep(TIME);
}
