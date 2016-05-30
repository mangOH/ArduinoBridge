/*
  Multi-sensor input connected to mangOH platform
  The program detects the following sensors:
  a. Temperature humidity using DHT22 on pin D2 (http://www.seeedstudio.com/wiki/Grove_-_Temperature_and_Humidity_Sensor_Pro)
  b. Dust sensor using Grove Dust sensor on pin D8 (http://www.seeedstudio.com/wiki/Grove_-_Dust_Sensor)
  c. Oxygen sensor using Grove Oxygen sensor on pin A0 (http://www.seeedstudio.com/wiki/Grove_-_Gas_Sensor(O%E2%82%82))
  d. Light sensor using Grove Light sensor on pin A1 (http://www.seeedstudio.com/wiki/Grove_-_Light_Sensor)
  e. Sound sensor using Grove Sound Sensor  on pin A2 ( http://www.seeedstudio.com/wiki/Grove_-_Sound_Sensor)
  f. Water sensor using Grove water sensor on pin D6  (http://www.seeedstudio.com/wiki/Grove_-_Water_Sensor)
  Written by CTO office Sierra
*/


#include <SwiBridge.h>
#include <DHT.h>
#include <Bridge.h>
#include <BridgeClient.h>
#include <BridgeServer.h>
#include <BridgeSSLClient.h>
#include <BridgeUdp.h>
#include <Console.h>
#include <FileIO.h>
#include <HttpClient.h>
#include <Mailbox.h>
#include <Process.h>
#include <YunClient.h>
#include <YunServer.h>

#define AV_URL                "na.airvantage.net" // replace with url of your airvantage accounr
#define AV_PASSWORD           "SwiBridge" // you can change this here and make it the same as the one on application model
#define APP_NAME              "Greenhouse"
#define APP_TEMPERATURE       "temperature"
#define APP_HUMIDITY          "humidity"
#define APP_LUMINOSITY        "luminosity"
#define APP_NOISE_LEVEL       "noise"
#define APP_WATER             "water"
#define APP_DUST              "dust"
#define APP_OXYGEN            "oxygen"
#define APP_LIGHT             "light"
#define APP_AIRCONDITIONER    "aircondition"

#define Version     10 // choose as per board provided
#define DHTPIN A0     // what pin we're connected to
#define DUSTPIN 8    // Dust sensor is connected to digital pin 8
#define OXYGENPIN A3 //Oxygen sensor is connected to pin A3
#define LIGHTPIN A1  // Light sensor is connected to pin A1
#define SOUNDPIN A2 // Sound sensor is connected pin A2
#define WATERPIN 6  // Water sensor is on pin 6

// Uncomment whatever type you're using!
#define DHTTYPE DHT11   // DHT 11 
//#define DHTTYPE DHT22   // DHT 22  (AM2302)
//#define DHTTYPE DHT21   // DHT 21 (AM2301)

// Readings for DHT sensor
DHT dht(DHTPIN, DHTTYPE);

//  Readings for Oxygen sensor
#if Version==11
const int AMP   = 121;
#elif Version==10
const int AMP   = 201;
#endif
float sensorValue;
float sensorVoltage;
float value_O2;
const float K_O2    = 6.64;

// Readings for Dust sensor
unsigned long duration;
unsigned long starttime;
unsigned long sampletime_ms = 30000;
unsigned long lowpulseoccupancy = 0;
float ratio = 0;
float concentration = 0;


// Readings for light sensor
int LightSensorValue;
float Rsensor;

// Readings for sound sensor
int SoundSensorValue;

// Reading for watersenor
bool waterValue;


void setup() {
  Serial.begin(9600);
  Serial.println("mangOH multi-sensors tutorial");

  Serial.println("Starting the bridge");
  Bridge.begin(115200);

  dht.begin();

  starttime = millis();//get the current time;


  Serial.print("Start Session: ");
  Serial.print(AV_URL);
  Serial.print(" ");
  Serial.println(AV_PASSWORD);
  SwiBridge.startSession(AV_URL, AV_PASSWORD, 1, CACHE);

}

void loop() {
  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  float h = dht.readHumidity();
  float t = dht.readTemperature();

  // Oxygen sensor calculations
  sensorValue = analogRead(OXYGENPIN);
  sensorVoltage = (sensorValue / 1024.0) * 5.0;
  sensorVoltage = sensorVoltage / (float)AMP * 10000.0;
  value_O2 = sensorVoltage / K_O2;

  Serial.print("Oxygen level is  = ");
  Serial.println(value_O2);

  // check if returns are valid, if they are NaN (not a number) then something went wrong!
  if (isnan(t) || isnan(h)) {
    Serial.println("Failed to read from DHT");
  } else {
    Serial.print("Humidity: ");
    Serial.print(h);
    Serial.print(" %\t");
    Serial.print("Temperature: ");
    Serial.print(t);
    Serial.println(" *C");


    Serial.print("Quality of O2 is ");
    Serial.print(value_O2, 1);
    Serial.println("%");
    duration = pulseIn(DUSTPIN, LOW);
    lowpulseoccupancy = lowpulseoccupancy + duration;
    if ((millis() - starttime) >= sampletime_ms) //if the sample time = = 30s
    {
      ratio = lowpulseoccupancy / (sampletime_ms * 10.0); // Integer percentage 0=&gt;100
      concentration = 1.1 * pow(ratio, 3) - 3.8 * pow(ratio, 2) + 520 * ratio + 0.62; // using spec sheet curve
      //Serial.print(lowpulseoccupancy);
      // Serial.print(",");
      //Serial.print(ratio);
      //Serial.print(",");
      Serial.print("dust level is  = ");
      Serial.print(concentration);
      Serial.println(" pcs/0.01cf");
      // Serial.println("\n");
      lowpulseoccupancy = 0;
      starttime = millis();
    }


    // Light sensor readings
    LightSensorValue = analogRead(LIGHTPIN);
    Rsensor = (float)(1023 - LightSensorValue) * 10 / LightSensorValue ;
    Serial.print("the Light sensor reading is ");
    Serial.println(LightSensorValue);
    Serial.print("the Light sensor resistance is ");
    Serial.println(Rsensor, DEC); //show the light intensity on the serial monitor

    // Sound sensor readings
    SoundSensorValue = analogRead(SOUNDPIN);
    Serial.print("the Sound sensor reading is ");
    Serial.println(SoundSensorValue);
    Serial.println("\n");

    // Water sensor reading
    if (digitalRead(WATERPIN) == LOW)
    {
      waterValue = true;
    }
    else
    {
      waterValue = false;
    }
    Serial.print(" Water present is ");
    Serial.println( waterValue);

    SwiBridge.pushFloat(APP_NAME "." APP_HUMIDITY, 3, h);
    SwiBridge.pushFloat(APP_NAME "." APP_TEMPERATURE, 3, t);
    SwiBridge.pushFloat(APP_NAME "." APP_DUST, 3, concentration);
    SwiBridge.pushFloat(APP_NAME "." APP_OXYGEN, 3, value_O2);
    SwiBridge.pushInteger(APP_NAME "." APP_LUMINOSITY, LightSensorValue);
    SwiBridge.pushInteger(APP_NAME "." APP_NOISE_LEVEL, SoundSensorValue);
    SwiBridge.pushBoolean(APP_NAME "." APP_WATER, waterValue);


  }

  delay(20000);
}

