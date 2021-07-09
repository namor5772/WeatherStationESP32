#include <time.h>
#include <WiFi.h>

#include "FS.h"
#include "SPIFFS.h"

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

// Full API documentation is available here: https://pubsubclient.knolleary.net
// https://github.com/knolleary/pubsubclient/tree/v2.8
#include <PubSubClient.h> 


// comment out "#define DEBUG" when going into production
#define DEBUG


// WIFI network credentials
const char* ssid     = "*****";
const char* password = "*****";


// MQTT connection credentials
const char* mqttServer = "m24.cloudmqtt.com"; 
int mqttPort = 16464;
#define MQTT_USER "******"
#define MQTT_PASSWORD "*****"


// MQTT definitions
const char* SLAVE = "ESP32"; // message sent from master to the slave we want to do stuff
#define MASTER "W10"         // message sent from slave client to master
#define MASTER_END "W10_END" // message sent from slave client to master


// NTP Server Details
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 9*60*60; // +9 hours 
const int   daylightOffset_sec = 3600;


// Define GPIO pin names
#define LED1_GPIO 25
#define LED0_GPIO 26
#define WIND_GPIO 18
#define RAIN_GPIO 19


// ISR related definitions
long debouncing_time = 15; // milliseconds
volatile unsigned long last_micros_WIND;
volatile unsigned long last_micros_RAIN;



WiFiClient wifiClient;
PubSubClient client(wifiClient);

Adafruit_BME280 bme; // I2C


struct Button {
    const uint8_t PIN;
    uint32_t numberKeyPresses;
    bool pressed;
};

Button WINDbutton = {WIND_GPIO, 0, false};
Button RAINbutton = {RAIN_GPIO, 0, false};


// ISR for anemometer
void IRAM_ATTR WIND_ISR() 
{
    if ((unsigned long)(micros() - last_micros_WIND) > debouncing_time*1000) {
        WINDbutton.numberKeyPresses += 1;
        WINDbutton.pressed = true;

        last_micros_WIND = micros();
    }        
}


// ISR for rain gauge
void IRAM_ATTR RAIN_ISR() 
{
    if ((unsigned long)(micros() - last_micros_RAIN) > debouncing_time*1000) {
        RAINbutton.numberKeyPresses += 1;
        RAINbutton.pressed = true;

        last_micros_RAIN = micros();
    }        
}


// used to test BME280 sensor when in DEBUG mode
void print_BME280values() {
    #ifdef DEBUG    
    Serial.print("Temperature = ");
    Serial.print(bme.readTemperature());
    Serial.println(" *C");
    
    Serial.print("Pressure = ");
    Serial.print(bme.readPressure() / 100.0F);
    Serial.println(" hPa");
    
    Serial.print("Humidity = ");
    Serial.print(bme.readHumidity());
    Serial.println(" %");
    
    Serial.println();
    #endif  
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

    if (b0=='r') {
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

    // measure all sensors and publish for MASTER to see
    if ((b0=='m')&&(b1=='a')) {
        
        // read BME280 sensor into float variables
        float rt = bme.readTemperature();
        float rp = bme.readPressure()/100.0F;
        float rh = bme.readHumidity();

        // put these sensor readings into a space efficient comma delimited char array in buf4
        int i, j=0; 
        char buf1[10], buf2[10], buf3[10], buf4[30];
        dtostrf(rt,8,2,buf1); // temperature as string (8 chars + 0 at end => 9 chars from buffer)
        dtostrf(rp,8,2,buf2); // pressure as string
        dtostrf(rh,8,2,buf3); // humidity as string
        for (i=0;i<8;i++) {buf1[i]==32 ? j++ : buf4[10*0+i-j]=buf1[i];} buf4[10*0+8-j] = 44; buf4[10*0+9-j] = 32;
        for (i=0;i<8;i++) {buf2[i]==32 ? j++ : buf4[10*1+i-j]=buf2[i];} buf4[10*1+8-j] = 44; buf4[10*1+9-j] = 32;
        for (i=0;i<8;i++) {buf3[i]==32 ? j++ : buf4[10*2+i-j]=buf3[i];} buf4[10*2+8-j] = 0;

        // and publish for MASTER to see
        client.publish(SLAVE, buf4);
        
        #ifdef DEBUG    
        Serial.println(buf4);
        #endif  
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


// gets locat date/time. Assumes internet connection to NTP server. 
void displayLocalTime() {
    struct tm timeinfo;
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


// THE Arduino IDE setup routine
void setup() {
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
  
    // setup connection to server and get the date/time
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    displayLocalTime();
    
    // setup GPIO pins for LEDS  
    pinMode(LED0_GPIO, OUTPUT); digitalWrite(LED0_GPIO, LOW);
    pinMode(LED1_GPIO, OUTPUT); digitalWrite(LED1_GPIO, LOW);

    // setup BME280 sensor
    bool stat;
    stat = bme.begin(0x77);  
    #ifdef DEBUG    
    Serial.println(F("\nBME280 setup and test"));
    if (!stat) {
        Serial.println("Could not find a valid BME280 sensor, check wiring!");
        while (1); // stop here forever
    }
    print_BME280values();  
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

    
    if (WINDbutton.pressed) {
        Serial.printf("WIND button has been pressed %u times\n", WINDbutton.numberKeyPresses);
        WINDbutton.pressed = false;
    }    
    if (RAINbutton.pressed) {
        Serial.printf("RAIN button has been pressed %u times\n", RAINbutton.numberKeyPresses);
        RAINbutton.pressed = false;
    }    
}
