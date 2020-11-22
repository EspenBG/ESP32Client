#include <WiFi.h>
#include <SocketIoClient.h>
#include <iostream>
#include <cstdio>


///////////////////////////////////// TODO - Find out how this works
#ifdef __cplusplus
extern "C" {
#endif
uint8_t temprature_sens_read();
#ifdef __cplusplus
}
#endif
uint8_t temprature_sens_read();
////////////////////////////////


/////////////////////////////////////
////// USER DEFINED VARIABLES //////
///////////////////////////////////
/// WIFI Settings ///
//const char* ssid     = "Tony-Wifi";
//const char* password = "1F1iRr0A";

const char* ssid     = "Zhone_8BF7";
const char* password = "znid305147639";

/// Socket.IO Settings ///
char host[] = "192.168.1.12"; // Socket.IO Server Address
int port = 3000; // Socket.IO Port Address
char path[] = "/socket.io/?transport=websocket"; // Socket.IO Base Path
//char path[] = "/robot";
char emittedSensorValues;

// TODO - Configure for SSL and extra authorization
bool useSSL = false;               // Use SSL Authentication
const char * sslFingerprint = "";  // SSL Certificate Fingerprint
bool useAuth = false;              // use Socket.IO Authentication
const char * serverUsername = "socketIOUsername";
const String serverPassword = "socketIOPassword";

/// Pin Settings ///
#define LEDPin 2
int buttonPin = 0;

/// Variables for algorithms ///
int temp;
int tempSetpoint;
int oldValue = 0;
const String robotID = "001";
const String sensorID = "001";
char outgoingMessage[150];


/////////////////////////////////////
////// ESP32 Socket.IO Client ///////
/////////////////////////////////////

SocketIoClient webSocket;
WiFiClient client;


void socket_Connected(const char * payload, size_t length) {
    Serial.println("Socket.IO Connected!");

    Serial.println("Sending password to server for authentication");

    // Sending password to server for authentication
    webSocket.emit("authentication", serverPassword.c_str());
}

void socket_Disconnected(const char * payload, size_t length) {
    Serial.println("Socket.IO Disconnected!");
}

void authenticate_feedback (const char * payload, size_t length) {
    if (payload == "true") {
        Serial.println("Authentication successful!");
        webSocket.emit("robotID", robotID.c_str());
    } else if (payload == "false") {
        Serial.println("Authentication unsuccessful, wrong password");
    } else {
        Serial.println("Unrecognized feedback / corrupted payload");
    }
}

void decide_robot_function (const char * payload, size_t length) {

}

// TODO - Double check if computation to celsius is correct
// Reading internal ESP32 heat sensor and converts to integer
int readCPUTemp() {
    // Reading temp value and converts to degrees in celsius
    float x = (temprature_sens_read() - 32) / 1.8;
    // Converts data from 2 decimal float to integer
    x = (int)(x + 0.5);
    // Return temp in integer value
    return x;
}


// TODO - Figure out how to send send data as JSON - more specifically: find out how to send quotations marks
// Because of problems with sending JSON keys in string with ", we are now sending with 0 in front of each key
// to fix this issue, server is changed to interpret it correctly
void sendData (int temperature) {
    // Checking if the temperature has changed since last loop
    if (temperature != oldValue) {
        // Sending the new temperature value along with identifiers as JSON to server
        // std::sprintf(outgoingMessage, "{\"unitID\":%d,\"sensorID\":%d,\"temperature\":%d}", unitID, sensorID, temperature);
        String temperatureKey = "temperature";
        String temperatureToSend = String(temperature);

        // String JSONobject = String("{\"" + String(String("hallo")) + "\":\"" + String(73) + "\"}");
        // String JSONobject2 = String("{\"" + String(temperatureKey) + "\":\"" + temperatureToSend + "\"}");


        // Serial.println(outgoingMessage);
        //webSocket.emit("temperature", outgoingMessage);
        // webSocket.emit("test", JSONobject10.c_str());
        //webSocket.emit("temperature", outgoingMessage);
        // sprintf(resultstr, "{\"temp1\":%d,\"temp2\":%d}", temp1, temp2);

        // Troubleshooting console printing
        //std::printf("\n", outgoingMessage);

        // Sets new 'oldValue', for next comparison
        oldValue = temperature;
    } else {
        return;
    }
}

// Testing received message
void print_to_console(const char * payload, size_t length) {
    Serial.print("Melding motatt:" );
    Serial.println(payload);
}


void setup() {
    Serial.begin(9600);
    delay(10);

    pinMode(LEDPin, OUTPUT);

    // We start by connecting to a WiFi network
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);

    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());


    // Listen events for incoming data
    webSocket.on("connect", socket_Connected);
    webSocket.on("disconnect", socket_Disconnected);
    webSocket.on("authentication", authenticate_feedback);
    webSocket.on("setpoints", decide_robot_function);


    // Setup Connection
    if (useSSL) {
        webSocket.beginSSL(host, port, path, sslFingerprint);
    } else {
        webSocket.begin(host, port, path);
    }

    // Handle Authentication
    //if (useAuth) {
    //    webSocket.setAuthorization(serverUsername, serverPassword);
    //}
}



void loop() {
    temp = readCPUTemp();
    //sendData(temp);
    //Serial.println(temp);


    // sendSensorData();
    webSocket.loop();
}
