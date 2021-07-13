#include <time.h>
#include <WiFi.h>

#include "FS.h"
#include "SPIFFS.h"

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_INA260.h>
#include <Adafruit_BME280.h>

// Full API documentation is available here: https://pubsubclient.knolleary.net
// https://github.com/knolleary/pubsubclient/tree/v2.8
#include <PubSubClient.h> 


// comment out "#define DEBUG" when going into production
#define DEBUG


// WIFI network credentials
const char* ssid     = "OPTUS_E4E36E";
const char* password = "stullglaik32369";
//const char* ssid     = "RMGmob";
//const char* password = "5z2b87S9";


// MQTT connection credentials
const char* mqttServer = "m24.cloudmqtt.com"; 
int mqttPort = 16464;
#define MQTT_USER "tigoclnq"
#define MQTT_PASSWORD "Fq1VurxwHlei" 


// MQTT definitions
const char* SLAVE = "ESP32"; // message sent from master to the slave we want to do stuff
#define MASTER "W10"         // message sent from slave client to master
#define MASTER_END "W10_END" // message sent from slave client to master


// NTP Server Details and related definitions
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 9*60*60; // +9 hours 
const int   daylightOffset_sec = 3600;
struct tm tmstruct;


// Define LED GPIO pin names
#define LED1_GPIO 25
#define LED0_GPIO 26


// ISR related definitions
long debouncing_time = 15; // milliseconds
unsigned long time_now;


// Anemometer related stuff
#define WIND_GPIO 18
volatile unsigned long last_WIND;
const int nWIND=5;
volatile unsigned long aWIND[nWIND]; // array to store millis time stamps for cup anemometer spins


// Rain gauge related stuff
#define RAIN_GPIO 19
volatile unsigned long last_RAIN;
const int nRAIN=100;
volatile unsigned long aRAIN[nRAIN]; // array to store millis time stamps for rain gauge bucket tips


// gap in minutes between data collection events (assume 60 % minGap == 0)
const int minGap=15;
const int hourGap=24;


// creating objects for Adafruit I2C sensor boards
Adafruit_BME280 bme;
Adafruit_INA260 ina = Adafruit_INA260();


// Wifi and MQTT client object setups
WiFiClient wifiClient;
PubSubClient client(wifiClient);




// ISR for anemometer
void IRAM_ATTR WIND_ISR() 
{
    if ((unsigned long)(millis() - last_WIND) > debouncing_time) {
        last_WIND = millis();
        
        // push last timestamp onto the end of the aWIND[] array (element nWIND-1). aWIND[0] is lost.
        for (int j=1; j<nWIND; j++) aWIND[j-1] = aWIND[j];
        aWIND[nWIND-1] = last_WIND;
    }        
}


// ISR for rain gauge
void IRAM_ATTR RAIN_ISR() 
{
    if ((unsigned long)(millis() - last_RAIN) > debouncing_time) {
        last_RAIN = millis();
        
        // push last timestamp onto the end of the aRAIN[] array (element nRAIN-1). aRAIN[0] is lost.
        for (int j=1; j<nRAIN; j++) aRAIN[j-1] = aRAIN[j];
        aRAIN[nRAIN-1] = last_RAIN;
    }        
}


// returns current windspeed in km/h
float getWindSpeed()
{
    if ((aWIND[0]==0) || (millis()-aWIND[nWIND-1] > 10000.0)) { 
        // Set wind speed to zero until nWIND time stamps have been collected
        // or last time stamp occured more than 10 seconds ago.
        return 0.0;
    } 
    else {
        // wind speed (in km/hr) is a calibration factor of 1200.0 divided 
        // the average of last nWIND-1 trigger time gaps (in milliseconds)
        return 1200.0*(float)(nWIND-1)/(float)(aWIND[nWIND-1] - aWIND[0]);
    }
}


// returns mm of rain during last fh hours 
float getCumulativeRainfall(float fh)
{
  float ts = millis(); // current time stamp as float
  float tsh = ts - fh*1000.0*60.0*60.0; // time stamp "fh" hours ago
  if (tsh<0.0) tsh = 0.0; // no rain and initialization adjustment ;-)

  // count number of times bucket has tipped during last "fh" hours
  int wr = 0;
  for (int j=nRAIN-1; j>=0; j--) { if (aRAIN[j]>tsh) wr++; }

  // each bucket tip corresponds to 0.2794 mm of rain  
  return (float)wr*0.2794;
}


// returns wind direction in degrees 
float getWindDirection()
{
    float x;
/*  
  int iwd = analogRead(WindDirectionAnalogPin);      
  if (iwd < 72) { x = 112.5; }
  else if (iwd < 86) { x = 67.5; }
  else if (iwd < 107) { x = 90.0; }
  else if (iwd < 153) { x = 157.5; }
  else if (iwd < 212) { x = 135.0; }
  else if (iwd < 264) { x = 202.5; }
  else if (iwd < 346) { x = 180.0; }
  else if (iwd < 434) { x = 22.5; }
  else if (iwd < 531) { x = 45.0; }
  else if (iwd < 615) { x = 247.5; }
  else if (iwd < 665) { x = 225.0; }
  else if (iwd < 742) { x = 337.5; }
  else if (iwd < 806) { x = 0.0; }
  else if (iwd < 858) { x = 292.5; }
  else if (iwd < 917) { x = 315.0; }
  else if (iwd < 989) { x = 270.0; }
  else { x = -1.0; }
*/
    x = 247.5;  
    return x;
}


// used to test BME280 sensor when in DEBUG mode
void print_BME280values() {
    #ifdef DEBUG    
    Serial.print(F("Temperature = "));
    Serial.print(bme.readTemperature());
    Serial.println(F(" *C"));
    
    Serial.print(F("Pressure = "));
    Serial.print(bme.readPressure() / 100.0F);
    Serial.println(F(" hPa"));
    
    Serial.print(F("Humidity = "));
    Serial.print(bme.readHumidity());
    Serial.println(F(" %"));
    
    Serial.println();
    #endif  
}


// used to test INA260 sensor when in DEBUG mode
void print_INA260values() {
    #ifdef DEBUG    
    Serial.print(F("Current: "));
    Serial.print(ina.readCurrent());
    Serial.println(F(" mA"));
    
    Serial.print(F("Bus Voltage: "));
    Serial.print(ina.readBusVoltage());
    Serial.println(" mV");
    
    Serial.print(F("Power: "));
    Serial.print(ina.readPower());
    Serial.println(F(" mW"));

    Serial.println();
    #endif  
}


// pFlag == 0 => read all sensors and publish for MASTER to see
// pFlag == 1 => read all sensors and append to a SPIFFS internal file
void readAllSensors(int pFlag) {
        
    // read temperature, pressure and humidity from BME280 sensor into float variables 
    float rt = bme.readTemperature();
    float rp = bme.readPressure()/100.0F;
    float rh = bme.readHumidity();
    
    // read rainfall, windspeed and wind direction into float variables
    float rr = getCumulativeRainfall(1.0); // during last 1 hour
    float rs = getWindSpeed();
    float rd = getWindDirection(); // in degrees
    
    // read current, bus voltage and power from INA260 sensor into float variables
    float rc = ina.readCurrent();
    float rv = ina.readBusVoltage();
    float rw = ina.readPower(); // rw = (r)ead milli(w)atts
    
    // put these sensor readings into a space efficient comma delimited char array in buf4
    int i, j=0; 
    char buf1[10], buf2[10], buf3[10], buf4[10], buf5[10], buf6[10], buf7[10], buf8[10], buf9[10], bufa[90];
    dtostrf(rt,8,2,buf1); // temperature as string (8 chars + 0 at end => 9 chars from buffer)
    dtostrf(rp,8,2,buf2); // pressure as string
    dtostrf(rh,8,2,buf3); // humidity as string
    dtostrf(rr,8,2,buf4); // rainfall as string
    dtostrf(rs,8,2,buf5); // wind speed as string
    dtostrf(rd,8,1,buf6); // wind direction as string
    dtostrf(rc,8,1,buf7); // current as string
    dtostrf(rv,8,0,buf8); // bus voltage as string
    dtostrf(rw,8,1,buf9); // power (in milliwatts) as string
    for (i=0;i<8;i++) {buf1[i]==32 ? j++ : bufa[10*0+i-j]=buf1[i];} bufa[10*0+8-j] = 44; bufa[10*0+9-j] = 32;
    for (i=0;i<8;i++) {buf2[i]==32 ? j++ : bufa[10*1+i-j]=buf2[i];} bufa[10*1+8-j] = 44; bufa[10*1+9-j] = 32;
    for (i=0;i<8;i++) {buf3[i]==32 ? j++ : bufa[10*2+i-j]=buf3[i];} bufa[10*2+8-j] = 44; bufa[10*2+9-j] = 32;
    for (i=0;i<8;i++) {buf4[i]==32 ? j++ : bufa[10*3+i-j]=buf4[i];} bufa[10*3+8-j] = 44; bufa[10*3+9-j] = 32;
    for (i=0;i<8;i++) {buf5[i]==32 ? j++ : bufa[10*4+i-j]=buf5[i];} bufa[10*4+8-j] = 44; bufa[10*4+9-j] = 32;
    for (i=0;i<8;i++) {buf6[i]==32 ? j++ : bufa[10*5+i-j]=buf6[i];} bufa[10*5+8-j] = 44; bufa[10*5+9-j] = 32;
    for (i=0;i<8;i++) {buf7[i]==32 ? j++ : bufa[10*6+i-j]=buf7[i];} bufa[10*6+8-j] = 44; bufa[10*6+9-j] = 32;
    for (i=0;i<8;i++) {buf8[i]==32 ? j++ : bufa[10*7+i-j]=buf8[i];} bufa[10*7+8-j] = 44; bufa[10*7+9-j] = 32;
    for (i=0;i<8;i++) {buf9[i]==32 ? j++ : bufa[10*8+i-j]=buf9[i];} bufa[10*8+8-j] = 0;
    
    #ifdef DEBUG    
    Serial.println(bufa);
    #endif  

    if (pFlag==0) {
        // and publish for MASTER to see
        client.publish(SLAVE, bufa);
    }
}


// THE MQTT callback routine. Driven by client.loop();
void callback(char* topic, byte *payload, unsigned int length) {
    #ifdef DEBUG    
    Serial.print("channel: "); Serial.print(topic);
    Serial.print("  data: "); Serial.write(payload, length); 
    Serial.print("  length: "); Serial.print(length);
    Serial.println();
    #endif  

    // RESPOND TO PAYLOAD
    char b0 = payload[0];
    char b1 = payload[1]; // might be invalid

    if ((b0=='r')||(b0=='R')) {
        #ifdef DEBUG    
        Serial.println("Restarting in 5 seconds");
        #endif  
        client.publish(SLAVE, "RESTARTING SLAVE");
        delay(5000);
        ESP.restart();
    }
    
    // Control two LEDS, 00=off,off 10=on,off 01=off,on 11=on,on 
    if (((b0=='0')||(b0=='1'))&&((b1=='0')||(b1=='1'))) {
        if (b0=='0') { digitalWrite(25, LOW); }
        if (b0=='1') { digitalWrite(25, HIGH); }
        if (b1=='0') { digitalWrite(26, LOW);  }
        if (b1=='1') { digitalWrite(26, HIGH); }
        client.publish(SLAVE, "LEDS SET");
    }

    // Measure all sensors and publish for MASTER to see
    if ((b0=='m')||(b0=='M')) {
        readAllSensors(0);
    }
}


// Connects to MQTT broker and subscribes to selected topics. Reconnects to WiFi if necessary!
void connect_MQTT_WIFI() {
    // Loop until we are (re)connected to MQTT
    while (!client.connected()) {
        
        #ifdef DEBUG    
        Serial.print("(Re)connecting to MQTT...");
        #endif  
        // Attempt to connect. clientID = MQTT_USER.
        
        if (client.connect(MQTT_USER,MQTT_USER,MQTT_PASSWORD)) {
            #ifdef DEBUG    
            Serial.println("connected");
            #endif  
    
            // Once connected, publish an announcement...
            client.publish(SLAVE, "Slave (re)connected to MQTT broker");
            
            // ... and resubscribe
            //client.subscribe(SUB_TOPIC);
            client.subscribe(MASTER);
            client.subscribe(MASTER_END);
        }
        else {
            // Failed to connect to MQTT, WIFI might also be cause.
            #ifdef DEBUG    
            Serial.print("failed, rc=");
            Serial.print(client.state());
            #endif  
            // (Re)connect to Wi-Fi
            WiFi.mode(WIFI_STA);
            WiFi.disconnect();
            delay(100);
            #ifdef DEBUG    
            Serial.print("\n(Re)connecting to WIFI ");
            Serial.print(ssid); Serial.print("...");
            #endif  
            WiFi.begin(ssid, password);
            while (WiFi.status() != WL_CONNECTED){}
            #ifdef DEBUG    
            Serial.print("connected. ");
            Serial.print("IP address: ");
            Serial.println(WiFi.localIP());
            #endif  
        }
    }
}

/*
// gets local date/time. Assumes internet connection to NTP server. 
void displayLocalTime() {
    if(!getLocalTime(&timeinfo)) {
        #ifdef DEBUG    
        Serial.println("Failed to obtain time");
        #endif  
    }
//  Serial.println(&timeinfo, "%w, %d-%m-%Y %H:%M:%S");
    #ifdef DEBUG    
    Serial.println(&timeinfo, "%a %d-%m-%Y %H:%M:%S");
    #endif  
}
*/

// THE Arduino IDE setup routine
void setup() {
    // Start the I2C interface
    Wire.begin();
    
    // setup serial, used for development/debugging
    #ifdef DEBUG    
    Serial.begin(115200);
    Serial.setTimeout(500);
    #endif  

    // Set WiFi to station mode and disconnect from an AP if it was previously connected
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);
    #ifdef DEBUG    
    Serial.println("\nWIFI Setup done");
    #endif  
    
    // list the WIFI networks found
    #ifdef DEBUG    
    Serial.print("Scanning for WIFI networks...");
    int n = WiFi.scanNetworks();
    Serial.println("completed");
    if (n == 0) {
        Serial.println("no networks found");
    }
    else {
        Serial.print(n);
        Serial.println(" networks found");
        for (int i = 0; i < n; ++i) {
            // Print SSID and RSSI for each network found
            Serial.print(i + 1); Serial.print(": ");
            Serial.print(WiFi.SSID(i)); Serial.print(" (");
            Serial.print(WiFi.RSSI(i)); Serial.print(")");
            Serial.println((WiFi.encryptionType(i) == WIFI_AUTH_OPEN)?" ":"*");
            delay(10);
        }
    }
    Serial.println("");
    delay(500);
    #endif  

    // (Re)connect to WI-Fi and MQTT broker  
    client.setServer(mqttServer, mqttPort);
    client.setCallback(callback);
    connect_MQTT_WIFI();
  
    // connect to time server and update the date/time on the RTC for future use
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    delay(2000);
    tmstruct.tm_year = 0;
    getLocalTime(&tmstruct,5000);
/* TO DO **** update RTC */
    #ifdef DEBUG    
    char bufx[30];
    strftime(bufx,100,"%d-%b-%Y %H:%M:%S",&tmstruct);
    Serial.println("");
    Serial.println(bufx);
    #endif  
    
    // setup GPIO pins for LEDS  
    pinMode(LED0_GPIO, OUTPUT); digitalWrite(LED0_GPIO, LOW);
    pinMode(LED1_GPIO, OUTPUT); digitalWrite(LED1_GPIO, LOW);

    // setup BME280 sensor
    bool stat = bme.begin(0x77);  
    #ifdef DEBUG    
    Serial.println(F("\nBME280 setup and test"));
    if (!stat) {
        Serial.println("Could not find a valid BME280 sensor, check wiring!");
        while (1); // stop here forever
    }
    print_BME280values();  
    #endif

    // setup INA260 sensor
    stat = ina.begin(0x40);  
    #ifdef DEBUG    
    Serial.println(F("\nINA260 setup and test"));
    if (!stat) {
        Serial.println("Could not find a valid INA260 sensor, check wiring!");
        while (1); // stop here forever
    }
    print_INA260values();  
    #endif


    // setup Wind speed and Rain gauge sensors. setup GPIO pins for interrupts.
    pinMode(WIND_GPIO, INPUT_PULLUP); attachInterrupt(WIND_GPIO, WIND_ISR, FALLING);
    pinMode(RAIN_GPIO, INPUT_PULLUP); attachInterrupt(RAIN_GPIO, RAIN_ISR, FALLING);
}



// THE Arduino IDE loop routine
void loop() {
    // the MQTT link
    if (!client.connected()) {connect_MQTT_WIFI();}
    client.loop();

    // for testing MQTT
    #ifdef DEBUG    
    if (Serial.available() > 0) {
        char ibuf[501];
        memset(ibuf,0, 501);
        Serial.readBytesUntil( '\n',ibuf,500);
        client.publish(SLAVE, ibuf);
    }
    #endif  

/*    
    time_now = millis();
    while (millis() < time_now+1000) { }

    Serial.print(clk.getHour(h12Flag,pmFlag)); Serial.print(" ");
    Serial.print(clk.getMinute()); Serial.print(" ");
    Serial.println(clk.getSecond());
*/    
}
