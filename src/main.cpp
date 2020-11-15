#include <Arduino.h>


// #include <WiFi.h>//Imports the needed WiFi libraries
// #include <WiFiMulti.h> //We need a second one for the ESP32 (these are included when you have the ESP32 libraries)

#include <SocketIoClient.h> //Import the Socket.io library, this also imports all the websockets
#include <WiFi.h> //Imports the needed WiFi libraries
#include <WiFiMulti.h> //We need a second one for the ESP32 (these are included when you have the ESP32 libraries)

#define USE_SERIAL Serial

int unitID = 001;
int sensorID = 001;
int tempPin = A0;   //This is the Arduino Pin that will read the sensor output
int sensorInput;    //The variable we will use to store the sensor input
double temp;        //The variable we will use to store temperature in degrees.
char payload = '{ "unit ID": "' + char(unitID)  + '", "Sensor ID": ' + char(sensorID) + '", "Temperatur": ' + char(temp) + '}';

/// Socket.IO Settings ///
SocketIoClient webSocket;
WiFiMulti WiFiMulti;

char host[] = "192.168.1.2";                     // Socket.IO Server Address
int port = 80;                                   // Socket.IO Port Address
char path[] = "/socket.io/?transport=websocket"; // Socket.IO Base Path



void findSensorValue();

void event(const char * payload, size_t length) { //Default event, what happens when you connect
    Serial.printf("got message: %s\n", payload);
}


void setup() {
    USE_SERIAL.begin(115200);
    USE_SERIAL.setDebugOutput(true);
    USE_SERIAL.println();
    USE_SERIAL.println();
    USE_SERIAL.println();

    // Establishing socketio connection
    Serial.println();
    Serial.print("Connecting to ");

    WiFiMulti.addAP("SSID", "passwd"); //Add a WiFi hotspot (addAP = add AccessPoint) (put your own network name and password in here)

    while(WiFiMulti.run() != WL_CONNECTED) { //Here we wait for a successful WiFi connection until we do anything else
        Serial.println("Not connected to wifi...");
        delay(100);
    }

    Serial.println("Connected to WiFi successfully!"); //When we have connected to a WiFi hotspot

    // event value and payload according to standard defined in oneNote - test prosjekt - Arduino
    webSocket.emit("sensorValue", payload);

    // listens for message sent by server
    webSocket.on("message_for_esp32", event);
}


void loop() {
    // Reading sensor value with ESP32 and converts to degrees
    findSensorValue();

    Serial.print("Current Temperature: ");
    Serial.println(temp);

    // message = '{ "sensorID": "' + String(sensorID)  + '", "temperature": ' + String(temp) + '}';

    // Runs every webSocket function when run in main loop
    webSocket.loop();                        //Processes the websocket. Called in Arduino main loop.
}

//TODO ESP32 got 3.3V instead of 5V
void findSensorValue() {
    sensorInput = analogRead(tempPin);       //read the analog sensor and store it
    temp = (double)sensorInput / 1024;       //find percentage of input reading
    temp *= 5;                         //multiply by 5V to get voltage
    temp -=  0.5;                       //Subtract the offset
    temp *= 100;                       //Convert to degrees
}



