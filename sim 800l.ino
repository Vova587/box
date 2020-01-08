#include <SoftwareSerial.h>
#include <MQ135.h>
#include <MQ9.h>
#include "DHT.h"
#include <SFE_BMP180.h>

#define MQ135Pin A2
#define MQ9Pin A3
#define DHTPIN 3

//CONSTANTS
const String API_KEY = "jwNV369TwuUBF8Y42ZFqN0pHe53UGMoxebCakZpo0ZU=";
const String SERVER_URL = "clearskymapssimplestaging.azurewebsites.net";
const double MOL_CO2 = 44.01;
const float MOL_LPG = 0;
const float MOL_CO = 28.01;
const float MOL_CH4 = 16.04;

SFE_BMP180 pressure; // Объявляем переменную для доступа к SFE_BMP180

MQ135 mq135 = MQ135(MQ135Pin); // инициализация объекта датчика
MQ9 mq9(MQ9Pin);
DHT dht(DHTPIN, DHT11);
/*Для пыли*/
int pin = 8;
unsigned long duration;
unsigned long starttime;
unsigned long sampletime_ms = 30000;
unsigned long lowpulseoccupancy = 0;
float ratio = 0;
float concentration = 0;
/*---------*/


SoftwareSerial SIM800(6, 7); // 8 - RX Arduino (TX SIM800L), 9 - TX Arduino (RX SIM800L)

void setup()
{
  FirstInitGSMModule();
  FirstInitSensors();
}

void loop()
{
  Serial.println("Loop");
  SetupGSMModule();
  String data = GetData();
  SendData(data);
}

void FirstInitGSMModule()
{
  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  Serial.println("Start!");
  SIM800.begin(9600);
  delay(20000);
  SIM800.println("AT+IPR=9600\r"); // Скорость порта GSM модуля (на всякий случай);
  Serial.println(ReadGSM());
  delay(2000);
  SIM800.println("AT\r");
  Serial.println(ReadGSM());
  delay(3000);
  SIM800.println("AT+COPS?\r");
  Serial.println(ReadGSM());
  delay(3000);
  SIM800.println("AT+CPAS\r");
  Serial.println(ReadGSM());
  delay(3000);
  SIM800.println("AT+CREG?\r");
  Serial.println(ReadGSM());
  delay(3000);
  SIM800.println("ATI\r");
  Serial.println(ReadGSM());
  delay(3000);
  SIM800.println("AT+CMGF=1\r"); // Configuring TEXT mode
  Serial.println(ReadGSM());
  delay(3000);
  SIM800.println("AT+CMGS=\"80298245148\"\r"); //change ZZ with country code and xxxxxxxxxxx with phone number to sms
  Serial.println(ReadGSM());
  SIM800.print("AT+CMGS=\"\r");
  SIM800.print("80298245148");
  SIM800.write(0x22);
  SIM800.write(0x0D);
  SIM800.write(0x0A);
  delay(10000);
  SIM800.print("ok");
  Serial.println(ReadGSM());
  delay(6000);
  SIM800.write(26);
  Serial.println(ReadGSM());
  delay(4000);
  delay(3000);
}

void FirstInitSensors()
{

  Serial.begin(9600); // последовательный порт для отображения данных
  mq9.calibrate();
  mq9.getRo();
  dht.begin();
  pressure.begin(); // Инициализация датчика
  pinMode(8, INPUT);
  starttime = millis(); //задержка для измерерния пыли
}

void SetupGSMModule()
{
  Serial.println("Trying to setup module");
  bool isSetup = false;
  while (SIM800.available() && !isSetup)
  {
    SIM800.println("AT+IPR=9600\r");
    if (ReadGSM().length() > 0)
    {
      isSetup = true;
    }
  }
  Serial.println("Module has been setup");
}


String GetData() {
  duration = pulseIn(pin, LOW);
  lowpulseoccupancy = lowpulseoccupancy + duration;
  if ((millis() - starttime) > sampletime_ms)
  {

    ratio = lowpulseoccupancy / (sampletime_ms * 10.0); // Integer percentage 0=>100
    concentration = 1.1 * pow(ratio, 3) - 3.8 * pow(ratio, 2) + 520 * ratio + 0.62;
    lowpulseoccupancy = 0;
    starttime = millis();
  }

  //Readings
  double T, P, Ppa, Pmm;
  float latitude, longitude;

  pressure.startTemperature();
  delay(100);
  pressure.getTemperature(T);
  pressure.startPressure(3);
  delay(100);
  pressure.getPressure(P, T);
  float H = dht.readHumidity();
  double co2 = mq135.getPPM(); // чтение данных концентрации CO2
  double co2PDK = Conversion(MOL_CO2, co2, T, P);
  float lpg = mq9.readLPG();           // чтение данных концентрации LPG
  float co = mq9.readCarbonMonoxide(); // чтение данных концентрации CO
  double coPDK = Conversion(MOL_CO, co, T, P);
  float ch4 = mq9.readMethane(); // чтение данных концентрации Метан (CH4)
  double ch4PDK = Conversion(MOL_CH4, ch4, T, P);

  //JSON Convert
  String jsonString = "{\"data\":\"" + API_KEY + "; " + String(T) + "; " + String(H) + "; " + String(P) + "; " + String(co2PDK) + "; " + String(lpg) + "; " + String(coPDK) + "; " + String(ch4PDK) + "; " + String(concentration) + "; " + String(longitude) + "; " // Longitude
                      + String(latitude)                                                                                                                                                                                                                             // Latitude
                      + "\"}";
  Serial.println("Data:" + jsonString);
  return jsonString;
}

void SendData(String data)
{
  SIM800.print("AT+CMGS=\"");
  SIM800.print("80298245148");
  SIM800.write(0x22);
  SIM800.write(0x0D);
  SIM800.write(0x0A);
  delay(10000);
  SIM800.print("ok");
  delay(6000);
  Serial.println(ReadGSM());
  SIM800.write(26);
  SIM800.println("AT+CIPSHUT\r"); //RESPONSE= OK
  Serial.println(ReadGSM());
  delay(2000);
  SIM800.println("AT+CIPMUX=0\r"); //RESPONSE= OK
  Serial.println(ReadGSM());
  delay(4000);
  SIM800.println("AT+CGATT=1\r"); //RESPONSE= OK
  Serial.println(ReadGSM());
  delay(2000);
  SIM800.println("AT+CSTT=\"MTS Internet\",\"\",\"\"\r"); //RESPONSE= OK
  Serial.println(ReadGSM());
  delay(10000);
  SIM800.println("AT+CIICR\r"); //RESPONSE= OK
  Serial.println(ReadGSM());
  delay(10000);
  SIM800.println("AT+CIFSR\r"); //RESPONSE= Returns an IP
  Serial.println(ReadGSM());
  delay(10000);
  SIM800.println("AT+CIPSTART=\"UDP\",\"" + SERVER_URL + "\", 80\r"); //RESPONSE= CONNECTED OK
  Serial.println(ReadGSM());
  delay(6000);
  SIM800.println("AT+CIPSEND\r"); //RESPONSE= >
  Serial.println(ReadGSM());
  delay(1000);
  SIM800.println("POST http://" + SERVER_URL + "/api/integration HTTP/1.1");
  Serial.println(ReadGSM());
  delay(1000);
  SIM800.println("Host: clearskymapssimplestaging.azurewebsites.net");
  Serial.println(ReadGSM());
  delay(1000);
  SIM800.println("Content-Type: application/json");
  Serial.println(ReadGSM());
  delay(1000);
  SIM800.println("Content-Length: " + data.length);
  Serial.println(ReadGSM());
  delay(1000);
  SIM800.println(data);
  Serial.println(ReadGSM());
  delay(1000);
  SIM800.write(0x1A); // Ctrl Z
  Serial.println(ReadGSM());
  delay(20000);
  /*
    After sending all these instructions, I get the following response,
    OK
    HTTP/1.1 200 OK
    Friday December, 22
    +TCPCLOSE=0
    OK
  */
  SIM800.println("AT+CIPCLOSE"); //RESPONSE= OK
  Serial.println(ReadGSM());
  delay(2000);
  SIM800.println("AT+CIPSHUT"); //RESPONSE= OK
  Serial.println(ReadGSM());
  delay(2000);
  delay(4000);
}

String ReadGSM()
{ //функция чтения данных от GSM модуля
  int c;
  String v;
  while (SIM800.available())
  { //сохраняем входную строку в переменную v
    c = SIM800.read();
    v += char(c);
    delay(20);
  }
  return v;
}

double Conversion(double a, double b, double c, double d)
{
  return ((a * b) / ((22.4 * (273 + c) * 101325000) / (273 * d * 100)));
}
