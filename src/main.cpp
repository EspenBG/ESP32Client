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


/// WIFI Settings ///
const char* ssid     = "Tony-Wifi";
const char* password = "1F1iRr0A";

//const char* ssid     = "Zhone_8BF7";
//const char* password = "znid305147639";

//const char* ssid     = "LAPTOP-6FTQ2LO34274";
//const char* password = "04sJ8$28";

/// Socket.IO Settings ///
char host[] = "192.168.1.103"; // Socket.IO Server Address
int port = 3000; // Socket.IO Port Address
char path[] = "/socket.io/?transport=websocket"; // Socket.IO Base Path
//char path[] = "/robot";
char emittedSensorValues;

// TODO - Configure for SSL and extra authorization
bool useSSL = false;               // Use SSL Authentication
const char * sslFingerprint = "";  // SSL Certificate Fingerprint
bool useAuth = false;              // use Socket.IO Authentication
const char * serverUsername = "socketIOUsername";
String serverPassword = "\"123456789\"";


/// Pin Settings ///
#define TEMP_INPUT 34
#define CO2_INPUT 35
#define HEATER 4
#define VENTILATION 5


/// Variables for algorithms ///
bool surveillance_mode = false;
bool active_regulation = false;
int CPUtemp;
int temperature_setpoint;
int co2_setpoint;
int oldValue = 0;
// robotID has to be chosen based on what types and how many sensor you want
// robotID also has to correspond to the ID set in the webpage, the value stored in robot-config.json
const String robotID = "\"000001\"";
const String sensorID = "\"#####1\"";


/////////////////////////////////////
////// ESP32 Socket.IO Client ///////
/////////////////////////////////////

SocketIoClient webSocket;
WiFiClient client;


void socket_Connected(const char * payload, size_t length) {
    Serial.println("Socket.IO Connected!");

    Serial.println("Sending password to server for authentication");
    delay(1000);

    // Sending password to server for authentication
    webSocket.emit("authentication", serverPassword.c_str());
}

void socket_Disconnected(const char * payload, size_t length) {
    Serial.println("Socket.IO Disconnected!");
}

void authenticate_feedback (const char * payload, size_t length) {
    String feedback = payload;

    if (feedback == "true") {
        Serial.println("Authentication successful!");
        webSocket.emit("robotID", robotID.c_str());
    } else if (feedback == "false") {
        Serial.println("Authentication unsuccessful, wrong password");
    } else {
        Serial.println("Unrecognized feedback / corrupted payload");
    }
}


void decide_robot_function(const char *payload, size_t length) {
    // TODO - Decode the JSON to a single variable that represents the sensor value

    // Deciding robot operation based on value sent
    Serial.println(payload);
    if (payload == "surveillance-mode") {
        surveillance_mode = true;
    } else {
        active_regulation = true;
        temperature_setpoint = atoi(payload);
    }
}


// Reading internal ESP32 heat sensor and converts to integer
int readCPUTemp() {
    // Reading CPUtemp value and converts to degrees in celsius
    float x = (temprature_sens_read() - 32) / 1.8;
    // Converts data from 2 decimal float to integer
    x = (int)(x + 0.5);
    // Return CPUtemp in integer value
    return x;
}


// TODO - Figure out how to send send data as JSON - more specifically: find out how to send quotations marks
// Because of problems with sending JSON keys in string with ", we are now sending with 0 in front of each key
// to fix this issue, server is changed to interpret it correctly
void sendData (int temperature) {
    // Checking if the temperature has changed since last loop
    if (temperature != oldValue) {
        // Sending the new temperature value along with identifiers as JSON to server
        //std::sprintf(outgoingMessage, "{\"robotID\":%d,\"sensorID\":%d,\"temperature\":%d}", robotID, sensorID, temperature);
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




void setup() {
    Serial.begin(9600);
    delay(10);

    pinMode(HEATER, OUTPUT);
    pinMode(VENTILATION, OUTPUT);

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


    // Listen events for incoming data from server
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
}



void loop() {
    // Reads temperature in CPU and stores it as integer
    CPUtemp = readCPUTemp();

    // Depending on mode selected from website, activate appropriate mode and emit value to server
    //if (surveillance_mode) {
    //    //webSocket.emit("sensorData", )
    //} else if (active_regulation) {
    //    //webSocket.emit("sensorData", )
    //    if (CPUtemp < temperature_setpoint) {
    //        digitalWrite(LEDPin, HIGH);
    //    } else {
    //        digitalWrite(LEDPin, LOW);
    //    }
    //} else {
    //    surveillance_mode = false;
    //    active_regulation = false;
    //}
    webSocket.loop();



    // Troubleshooting physical connections
    //delay(2000);
    //int temp = analogRead(TEMP_INPUT);
    //Serial.println("temp:");
    //Serial.println(temp);
    //digitalWrite(HEATER, HIGH);
    //int co2 = analogRead(CO2_INPUT);
    //Serial.println("co2:");
    //Serial.println(co2);
    //digitalWrite(VENTILATION, HIGH);
}
