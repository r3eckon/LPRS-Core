
String consoleData;
String loraData;

unsigned long last;
bool lastHigh=false;
const unsigned long del = 1000;
bool paramsLoaded=false;

HardwareSerial LoraSerial = HardwareSerial(USART1);

void setup() {
  LoraSerial.begin(115200);
  LoraSerial.setTimeout(100);
}

void loop() {

  //Init lora module with parameters
  //Must add some delay to wait for module to be ready
  if(!paramsLoaded){
    delay(1000);
    //Serial1.println("AT+PARAMETER=9,7,4,7");//Low Delay
    LoraSerial.println("AT+PARAMETER=12,7,4,7");//Long Range Default
    paramsLoaded=true;
  }

  if(Serial.available()){

    consoleData = Serial.readString();
    Serial.println("SENT : " + consoleData);
    LoraSerial.println(consoleData);
  }
  if(LoraSerial.available()){
    loraData = LoraSerial.readString();
    Serial.print(loraData);
  }
  
  
}
