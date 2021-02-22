#define SELF_ADDRESS 1000
#define RELAY_ADDRESS 1

HardwareSerial LoraSerial = HardwareSerial(USART1);

char * token;

char buff[1024];//1k buffer

int lastLen;

String inString;

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  //Open serial port to lora chip
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

  if(LoraSerial.available() > 0)
  {
    digitalWrite(LED_BUILTIN, LOW);
    
    
    
    //Gather full lora string
    inString = LoraSerial.readString();
    
    //Detect the +RCV part of lora incoming message
    //so as to not relay anything but received msg
    if(inString.indexOf("+RCV") >= 0)
    {
      //Turn to char and extract part of msg after =
      inString.toCharArray(buff, 1024);
      token = strtok(buff, "=");
      token = strtok(NULL,"=");

      //Get length
      lastLen = strlen(token);

      //Format and send message to relay
      LoraSerial.print("AT+SEND=");
      LoraSerial.print(RELAY_ADDRESS);
      LoraSerial.print(",");
      LoraSerial.print(lastLen);
      LoraSerial.print(",");
      LoraSerial.println(token);
      LoraSerial.flush();
    }

    digitalWrite(LED_BUILTIN, HIGH);
  }
  
  
  
  
}
