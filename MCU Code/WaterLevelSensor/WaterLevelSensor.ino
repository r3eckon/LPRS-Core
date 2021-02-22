#define TRIGGER PA1
#define ECHO PA2
#define MAX_DISTANCE 130.0f
#define MIN_DISTANCE 23.0f
#define SELF_ADDRESS 100
#define RELAY_ADDRESS 1
#define SENSOR_ID 3

long duration;
int distance;

float reldist;
float fdist;

HardwareSerial LoraSerial = HardwareSerial(USART1);
int sval;
String valstr;
String toSend;

void setup() {
  pinMode(TRIGGER, OUTPUT);
  pinMode(ECHO, INPUT);
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

  //Temporary debug code
  //Serial.begin(9600);
}

void loop() {

  //Trigger reading
  digitalWrite(TRIGGER,HIGH);
  delayMicroseconds(20);
  digitalWrite(TRIGGER,LOW);

  //Read wave duration
  duration = pulseIn(ECHO, HIGH);

  //Convert to distance measurement
  distance = duration * 0.034/2;
  fdist=distance;
  fdist = min(fdist, MAX_DISTANCE);
  fdist = max(fdist, MIN_DISTANCE);
  

  reldist = 1.0f - ((fdist - MIN_DISTANCE)/(MAX_DISTANCE-MIN_DISTANCE));


  digitalWrite(LED_BUILTIN, LOW);

  sval = reldist * 4096.0f;
  

  //Format a string to send with lora
  valstr = String(SENSOR_ID, DEC);                    //3
  valstr.concat("_");                                 //3_
  valstr.concat(String(sval, DEC));                   //3_2048
  valstr.concat("h");                                 //3_2048h
  toSend = String("AT+SEND=");                        //AT+SEND=
  toSend.concat(String(RELAY_ADDRESS, DEC));          //AT+SEND=1
  toSend.concat(",");                                 //AT+SEND=1,
  toSend.concat(String(valstr.length(), DEC));        //AT+SEND=1,7
  toSend.concat(",");                                 //AT+SEND=1,7,
  toSend.concat(valstr);                              //AT+SEND=1,7,3_2048h

  //Queue serial send and wait to complete
  LoraSerial.println(toSend);
  LoraSerial.flush();

  digitalWrite(LED_BUILTIN, HIGH);
  
  /*
  Serial.print("[");
  Serial.print(distance);
  Serial.print(" cm] [");
  Serial.print(reldist * 100.0f);
  Serial.println(" %]");
  */
  
  delay(3000);
  
}
