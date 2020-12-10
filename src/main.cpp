#include <WiFi.h>
#include <SocketIoClient.h>
#include <iostream>
#include <ArduinoJson.h>
#include <cstdio>

StaticJsonBuffer<200> jsonBuffer;


/// WIFI Settings ///
const char* ssid     = "Tony-Wifi";
const char* password = "1F1iRr0A";

//const char* ssid     = "Zhone_8BF7";
//const char* password = "znid305147639";

//const char* ssid     = "LAPTOP-6FTQ2LO34274";
//const char* password = "04sJ8$28";

/// Socket.IO Settings ///
char host[] = "192.168.1.103";                   // Socket.IO Server Address
int port = 3000;                                 // Socket.IO Port Address
char path[] = "/socket.io/?transport=websocket"; // Socket.IO Base Path
//char path[] = "/robot";

// TODO - Configure for SSL and extra authorization
bool useSSL = false;               // Use SSL Authentication
const char * sslFingerprint = "";  // SSL Certificate Fingerprint
const char * serverUsername = "socketIOUsername";
String serverPassword = "\"123456789\"";


/// Pin Settings ///
int TEMP_INPUT_PIN = 34;
int CO2_INPUT_PIN = 35;
int HEATER_OUTPUT_PIN = 4;
int VENTILATION_OUTPUT_PIN = 5;


/// Variables for algorithms ///
float temperature_setpoint;
float co2_setpoint;
float temp_value;
float co2_value;
float old_temp_value;
float old_co2_value;
bool authenticated_by_server = false;


// robotID and sensorID has to be initialized here and have to correspond with the selections on the website
const String ROBOT_ID = "\"000003\"";
const String TEMP_SENSOR_KEY = "000001";
const String CO2_SENSOR_KEY = "000002";

// const String SENSOR_ID_X = "#####X";
// const String SENSOR_ID_Y = "#####Y";


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
    authenticated_by_server = false;
}

void authenticate_feedback (const char * payload, size_t length) {
    String feedback = payload;

    if (feedback == "true") {
        Serial.println("Authentication successful!");
        authenticated_by_server = true;
        webSocket.emit("robotID", ROBOT_ID.c_str());
    } else if (feedback == "false") {
        Serial.println("Authentication unsuccessful, wrong password");
    } else {
        Serial.println("Unrecognized feedback / corrupted payload");
    }
}

void manage_server_setpoints(const char * payload, size_t length) {

    String str_payload = payload;
    str_payload.replace("\\", "" );

    char setpoints_array[100];
    str_payload.toCharArray(setpoints_array, 100);

    Serial.println(setpoints_array);

    JsonObject& server_data = jsonBuffer.parseObject(setpoints_array);

    if(!server_data.success()) {
        Serial.println("parseObject() from index.js set-points payload failed");
    }

    temperature_setpoint = server_data[TEMP_SENSOR_KEY];
    co2_setpoint = server_data[CO2_SENSOR_KEY];
    Serial.println(temperature_setpoint);
    Serial.println(co2_setpoint);

}

void read_sensor_value (String sensor_type) {
    float raw_co2;
    float raw_temp;

    if (sensor_type == "temperature") {
        raw_temp = analogRead(TEMP_INPUT_PIN);
        temp_value = (raw_temp / 4095.0) * 70.0;
    } else if (sensor_type == "co2") {
        raw_co2 = analogRead(CO2_INPUT_PIN);
        co2_value = (raw_co2 / 4095.0) * 200.0;
    } else {
        Serial.println("Invalid sensor type argument given to readSensorValue function");
    }

}

void set_output_state (int setpoint, int current_value, int actuator_output) {
    if (setpoint > current_value) {
        digitalWrite(actuator_output, HIGH);
    } else {
        digitalWrite(actuator_output, LOW);
    }
}

void send_sensor_value(String sensor_key, float sensor_value) {

    String sensor_data = String("{\"SensorID\":\"" + String(sensor_key) + "\",\"value\":" + String(sensor_value) + "}");
    Serial.println(sensor_data);

    webSocket.emit("sensorData", sensor_data.c_str());

}

float decimal_round(float input, int decimals) {
    float scale=pow(10,decimals);
    return round(input*scale)/scale;
}

void check_for_value_change (String sensor_type) {
    float rounded_value;

    if (sensor_type == "temperature") {
        rounded_value = decimal_round(temp_value, 1);

        if (rounded_value != old_temp_value) {
            send_sensor_value(TEMP_SENSOR_KEY, rounded_value);
            old_temp_value = rounded_value;
        }

    } else if (sensor_type == "co2") {
        rounded_value = decimal_round(co2_value, 1);

        if (rounded_value != old_co2_value) {
            send_sensor_value(CO2_SENSOR_KEY, rounded_value);
            old_co2_value = rounded_value;
        }
    }
}



void setup() {
    Serial.begin(9600);
    delay(10);

    pinMode(HEATER_OUTPUT_PIN, OUTPUT);
    pinMode(VENTILATION_OUTPUT_PIN, OUTPUT);

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
    webSocket.on("setpoints", manage_server_setpoints);


    // Setup Connection
    if (useSSL) {
        webSocket.beginSSL(host, port, path, sslFingerprint);
    } else {
        webSocket.begin(host, port, path);
    }
}

void loop() {
    // Reads the value of the the sensor given and saves the value/values to global variable/variables
    read_sensor_value("temperature");
    read_sensor_value("co2");

    if (authenticated_by_server) {
        // Based on the current values and the set-points given from the server, output is set accordingly
        set_output_state(temperature_setpoint, temp_value, HEATER_OUTPUT_PIN);
        set_output_state(co2_setpoint, co2_value, VENTILATION_OUTPUT_PIN);

        // TODO - Remove troubleshooting console prints
        Serial.print("Temp reading: ");
        Serial.println(temp_value);
        Serial.print("CO2 reading: ");
        Serial.println(co2_value);

        // Checks if the current value has changed with more than 0.1 Celsius for temp-sensors, or 0.1 PPM for CO2-sensors
        // and if the value has changed since last iteration emits a new message to the server
        check_for_value_change("temperature");
        check_for_value_change("co2");
    }
    webSocket.loop();
}
