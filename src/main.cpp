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
const char* ssid     = "Tony-Wifi";
const char* password = "1F1iRr0A";

/// Socket.IO Settings ///
char host[] = "192.168.1.103"; // Socket.IO Server Address
int port = 3000; // Socket.IO Port Address
char path[] = "/socket.io/?transport=websocket"; // Socket.IO Base Path
char emittingSensorValues;

// TODO - Configure for SSL and extra authorization
bool useSSL = false;               // Use SSL Authentication
const char * sslFingerprint = "";  // SSL Certificate Fingerprint
bool useAuth = false;              // use Socket.IO Authentication
const char * serverUsername = "socketIOUsername";
const char * serverPassword = "socketIOPassword";

/// Pin Settings ///
int LEDPin = 2;
int buttonPin = 0;

/// Variables for algorithms ///
int temp;
int oldValue = 0;
int unitID = 001;
int sensorID = 001;
char outgoingMessage[50];






/////////////////////////////////////
////// ESP32 Socket.IO Client //////
///////////////////////////////////

SocketIoClient webSocket;
WiFiClient client;


void socket_Connected(const char * payload, size_t length) {
    Serial.println("Socket.IO Connected!");
}

void socket_Disconnected(const char * payload, size_t length) {
    Serial.println("Socket.IO Disconnected!");
}

// Reading internal ESP32 heat sensor and converts to integer
int readSensor() {
    // Reading temp value and converts to degrees in celsius
    float x = (temprature_sens_read() - 32) / 1.8;
    // Converts data from 2 decimal float to integer
    int y = (int)(x + 0.5);
    // Return temp in integer value
    return y;
}

void sendData (int temperature) {
    // Checking if the temperature has changed since last loop
    if (temperature != oldValue) {
        // Sending the new temperature value along with identifiers as JSON to server
        sprintf(outgoingMessage, "{\"unitID\": %d, \"sensorID\": %d, \"temperature\": %d}", unitID, sensorID, temperature);
        webSocket.emit("temperature", outgoingMessage);

        // Troubleshooting console printing
        std::printf("Emitted message: %s \n", outgoingMessage);

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
    pinMode(buttonPin, INPUT);

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
    webSocket.on("test", print_to_console);

    // Setup Connection
    if (useSSL) {
        webSocket.beginSSL(host, port, path, sslFingerprint);
    } else {
        webSocket.begin(host, port, path);
    }

    // Handle Authentication
    if (useAuth) {
        webSocket.setAuthorization(serverUsername, serverPassword);
    }
}


void loop() {
    delay(1000);
    temp = readSensor();
    sendData(temp);
    Serial.println(temp);


    // sendSensorData();
    webSocket.loop();
}
