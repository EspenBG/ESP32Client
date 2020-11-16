#include <Arduino.h>


// #include <WiFi.h>//Imports the needed WiFi libraries
// #include <WiFiMulti.h> //We need a second one for the ESP32 (these are included when you have the ESP32 libraries)

#include <SocketIoClient.h> //Import the Socket.io library, this also imports all the websockets
#include <WiFi.h> //Imports the needed WiFi libraries
#define USE_SERIAL Serial

const char* ssid     = "Tony-Wifi";
const char* password = "1F1iRr0A";
int unitID = 001;
int sensorID = 001;
int tempPin = A0;   //This is the Arduino Pin that will read the sensor output
// int sensorInput;    //The variable we will use to store the sensor input
double temp;        //The variable we will use to store temperature in degrees.
// char payload = '{ "unit ID": "' + char(unitID)  + '", "Sensor ID": ' + char(sensorID) + '", "Temperatur": ' + char(temp) + '}';

/// Socket.IO Settings ///
SocketIoClient webSocket;
WiFiClient Wifi;

char host[] = "192.168.1.103";                     // Socket.IO Server Address
int port = 3000;                                   // Socket.IO Port Address
char path[] = "/socket.io/?transport=websocket";   // Socket.IO Base Path



void findSensorValue();

/*void event(const char * payload, size_t length) { //Default event, what happens when you connect
    Serial.printf("got message: %s\n", payload);
    webSocket.emit("test", "tralalala");
}*/

void socket_Connected(const char * payload, size_t length) {
    Serial.println("Socket.IO Connected!");
}

void socket_Disconnected(const char * payload, size_t length) {
    Serial.println("Socket.IO Disconnected...");
}

void serverMessage(const char * payload, size_t length) {
    Serial.print("Server sendet en melding: ");
    Serial.print(payload);
    Serial.println();
}


void setup() {
    delay(10);
    USE_SERIAL.begin(9600);
    USE_SERIAL.setDebugOutput(true);
    USE_SERIAL.println();
    USE_SERIAL.println();
    USE_SERIAL.println();

    // Establishing socketio connection
    Serial.println();
    Serial.print("Connecting to ");

    WiFi.begin(ssid, password);

    //Here we wait for a successful WiFi connection until we do anything else
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("Connected to WiFi successfully!"); //When we have connected to a WiFi hotspot

    Serial.println("Looper");
    // webSocket.emit("sensorValue", payload);
    webSocket.emit("test", "tralalala");

    // listens for message sent by server
//    webSocket.on("connect", socket_Connected);
//    webSocket.on("disconnect", socket_Disconnected);
//    webSocket.on("servertest", serverMessage);

    webSocket.begin(host, port, path);
}


void loop() {
    // Reading sensor value with ESP32 and converts to degrees
    // findSensorValue();

    // Serial.print("Current Temperature: ");
    // Serial.println(temp);

    // message = '{ "sensorID": "' + String(sensorID)  + '", "temperature": ' + String(temp) + '}';

    webSocket.loop();                        //Processes the websocket. Called in Arduino main loop.
}

//TODO ESP32 got 3.3V instead of 5V
//void findSensorValue() {
//    sensorInput = analogRead(tempPin);       //read the analog sensor and store it
//    temp = (double)sensorInput / 1024;       //find percentage of input reading
//    temp *= 5;                         //multiply by 5V to get voltage
//    temp -=  0.5;                       //Subtract the offset
//    temp *= 100;                       //Convert to degrees
//}


