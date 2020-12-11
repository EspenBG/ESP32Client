#include <WiFi.h>
#include <SocketIoClient.h>
#include <ArduinoJson.h>


StaticJsonBuffer<200> jsonBuffer;


/// WIFI Settings ///
const char* SSID     = "Tony-Wifi";
const char* PASSWORD = "1F1iRr0A";

//const char* SSID     = "Zhone_8BF7";
//const char* PASSWORD = "znid305147639";

//const char* SSID     = "LAPTOP-6FTQ2LO34274";
//const char* PASSWORD = "04sJ8$28";

/// Socket.IO Settings ///
char HOST[] = "192.168.1.103";                   // Socket.IO Server Address
int PORT = 3000;                                 // Socket.IO Port Address
char PATH[] = "/socket.io/?transport=websocket"; // Socket.IO Base Path

/// Internal temperature sensor initializing ///
#ifdef __cplusplus
extern "C" {
#endif

uint8_t temprature_sens_read();

#ifdef __cplusplus
}
#endif

uint8_t temprature_sens_read();


// TODO - Configure for SSL and extra authorization
bool USE_SSL = false;               // Use SSL Authentication
const char * SSL_FINGERPRINT = "";  // SSL Certificate Fingerprint
const char * SERVER_USERNAME = "socketIOUsername";
String SERVER_PASSWORD = "\"123456789\"";


/// Pin Settings ///
int TEMP_INPUT_PIN = 35;
int CO2_INPUT_PIN = 34;
int HEATER_OUTPUT_PIN = 4;
int VENTILATION_OUTPUT_PIN = 5;


/// Variables for algorithms ///
float temperatureSetpoint;
float co2Setpoint;
float tempValue;
float internalTempValue;
float co2Value;
float oldTempValue;
float oldInternalTempValue;
float oldCo2Value;
bool authenticatedByServer = false;
// TODO - Write in documentation about reversing temperature output depending on what kind of actuator is connected
bool tempActuatorReversed = false;
bool co2ActuatorReversed = true; // All CO2 actuator is working with reversed = false

bool previousTempOutputState;
bool previousCo2OutputState;

bool surveillanceModeTemp;
bool surveillanceModeCo2;



// robotID and sensorID has to be initialized here and have to correspond with the selections on the website
const String ROBOT_ID = "\"000003\"";
const String TEMP_SENSOR_KEY = "000001";
const String CO2_SENSOR_KEY = "000002";
const String INTERNAL_TEMP_SENSOR_KEY = "000003";


// const String SENSOR_ID_X = "#####X";
// const String SENSOR_ID_Y = "#####Y";


/////////////////////////////////////
////// ESP32 Socket.IO Client ///////
/////////////////////////////////////

SocketIoClient webSocket;
WiFiClient client;


/**
 * Function that is called when the ESP32 creates a connection with the server. On connection
 * some sentences is printed to the console. Then emits a password to the server for authentication.
 * @param payload     contains the data sent from server
 * @param length      gives the size of the payload
 */
void socketConnected(const char * payload, size_t length) {
    Serial.println("Socket.IO Connected!");

    Serial.println("Sending PASSWORD to server for authentication");
    // TODO - Remove?
    delay(1000);

    // Sending PASSWORD to server for authentication
    webSocket.emit("authentication", SERVER_PASSWORD.c_str());
}

/**
 * Function that is called if connection with the server is lost. And sets variable
 * that describes the ESP32 is not authenticated.
 * @param payload     contains the data sent from server
 * @param length      gives the size of the payload
 */
void socketDisconnected(const char * payload, size_t length) {
    Serial.println("Socket.IO Disconnected!");
    authenticatedByServer = false;
}

/**
 * Function that handles the server response on the password sent for authentication. The function
 * saves the payload to a datatype that can be compared in an if statement. The if statements checks if
 * the server emitted true / false, or something invalid. If the response ia false or invalid, prints text
 * describing the error to console. If the server gives approved feedback a new emit function is called with
 * the robots ID, and sets variable that describes the ESP32 is authenticated.
 * @param payload     contains the data sent from server
 * @param length      gives the size of the payload
 */
void authenticateFeedback (const char * payload, size_t length) {
    String feedback = payload;

    if (feedback == "true") {
        Serial.println("Authentication successful!");
        authenticatedByServer = true;
        webSocket.emit("robotID", ROBOT_ID.c_str());
    } else if (feedback == "false") {
        Serial.println("Authentication unsuccessful, wrong PASSWORD");
    } else {
        Serial.println("Unrecognized feedback / corrupted payload");
    }
}


void determineMode (JsonObject& serverData) {
    if (serverData[TEMP_SENSOR_KEY] == "none") {
        surveillanceModeTemp = true;
        Serial.println("Surveillance mode for temp is activated");
    } else {
        temperatureSetpoint = serverData[TEMP_SENSOR_KEY];
        surveillanceModeTemp = false;
        Serial.println("Normal regulation mode for temp is activated");
    }

    if (serverData[CO2_SENSOR_KEY] == "none") {
        surveillanceModeCo2 = true;
        Serial.println("Surveillance mode for co2 is activated");
    } else {
        co2Setpoint = serverData[CO2_SENSOR_KEY];
        surveillanceModeCo2 = false;
        Serial.println("Normal regulation mode for co2 is activated");
    }
}

/**
 * Function to get data from the database and form newSensorData, and returns the data as an object
 * The time interval for the search is controlled by the start time and the stop time.
 * If the stop time is 0 the search return all the sensor data from the start time to the time of the search.
 * @param payload     contains the data sent from server
 * @param length      gives the size of the payload
 */
void manageServerSetpoints(const char * payload, size_t length) {

    String str_payload = payload;
    str_payload.replace("\\", "" );

    char setpoints_array[100];
    str_payload.toCharArray(setpoints_array, 100);

    Serial.println(setpoints_array);

    JsonObject& server_data = jsonBuffer.parseObject(setpoints_array);

    if(!server_data.success()) {
        Serial.println("parseObject() from index.js set-points payload failed");
    }

    determineMode(server_data);

    Serial.println(temperatureSetpoint);
    Serial.println(co2Setpoint);

}

void readSensorValue (String sensor_type) {
    float raw_co2;
    float raw_temp;

    if (sensor_type == "temperature") {
        raw_temp = analogRead(TEMP_INPUT_PIN);
        tempValue = (raw_temp / 4095.0) * 70.0;
    } else if (sensor_type == "co2") {
        raw_co2 = analogRead(CO2_INPUT_PIN);
        co2Value = (raw_co2 / 4095.0) * 2000.0;
    } else {
        Serial.println("Invalid sensor type argument given to readSensorValue function");
    }

}

bool checkSensor (float setpoint, float current_value, bool output_reversed) {
    if (output_reversed) {
        if (setpoint < current_value) {
            return true;
        } else {
            return false;
        }
    } else {
        if (setpoint < current_value) {
            return false;
        } else {
            return true;
        }
    }
}

void setOutput (int output_pin, bool output) {
    if (output) {
        digitalWrite(output_pin, HIGH);
    } else {
        digitalWrite(output_pin, LOW);
    }
}

void sendDataToServer(String typeOfData, String key1Value, float sensor_value, bool outputState) {

    if (typeOfData == "output") {
        String data = String("{\"ControlledItemID\":\"" + String(key1Value) + "\",\"value\":" + String(outputState) + "}");
        Serial.println(data);

        webSocket.emit("sensorData", data.c_str());

    } else if (typeOfData == "sensorValues") {
        String data = String("{\"SensorID\":\"" + String(key1Value) + "\",\"value\":" + String(sensor_value) + "}");
        Serial.println(data);

        webSocket.emit("sensorData", data.c_str());

    } else {
        Serial.println("Invalid type of data");
    }

}

/**
 * Function to get data from the database and form newSensorData, and returns the data as an object
 * The time interval for the search is controlled by the start time and the stop time.
 * If the stop time is 0 the search return all the sensor data from the start time to the time of the search.
 * @param startTime     start time of the search (time in ms from 01.01.1970)
 * @param stopTime      stop time for the search (time in ms from 01.01.1970)
 * @param sensorID      name of the sensor, the sensorID
 * @param dataType      Specifies the type of data to search for (e.g. SensorID or ControlledItemID)
 * @param callback      Runs the callback with the sensor data for the sensor specified
 */
void setOutputState (String typeOfData, float setpoint, float current_value, int actuator_output, bool output_reversed, String idKey) {
    bool output = checkSensor(setpoint, current_value, output_reversed);

    if (typeOfData == "temperature") {
        if (output != previousTempOutputState) {
            setOutput(actuator_output, output);
            previousTempOutputState = output;
            sendDataToServer("output", idKey, 0.0, output);
        }
    } else if (typeOfData == "co2") {
        if (output != previousCo2OutputState) {
            setOutput(actuator_output, output);
            previousCo2OutputState = output;
            sendDataToServer("output", idKey, 0.0, output);
        }
    }
}

float decimalRound(float input, int decimals) {
    float scale=pow(10,decimals);
    return round(input*scale)/scale;
}

void checkForSensorChange (String sensor_type, String idKey) {
    float rounded_value;

    if (sensor_type == "temperature") {
        rounded_value = decimalRound(tempValue, 1);

        if ((oldTempValue - 1) > rounded_value or (oldTempValue + 1) < rounded_value) {
            sendDataToServer("sensorValues", idKey, rounded_value, false);
            oldTempValue = rounded_value;
        }

    } else if (sensor_type == "co2") {
        rounded_value = decimalRound(co2Value, 1);

        if ((oldCo2Value - 50) > rounded_value or (oldCo2Value + 50) < rounded_value) {
            sendDataToServer("sensorValues", idKey, rounded_value, false);
            oldCo2Value = rounded_value;
        }
    } else if (sensor_type == "internal-temp") {

        // TODO - Remove troubleshooting console printing
        //Serial.print("Internal CPU Temperature: ");
        // Convert raw temperature in F to Celsius degrees
        //Serial.print((temprature_sens_read() - 32) / 1.8);
        //Serial.println(" C");

        internalTempValue = ((temprature_sens_read() - 32) / 1.8);
        rounded_value = decimalRound(internalTempValue, 1);

        if ((oldInternalTempValue - 1) > rounded_value or (oldInternalTempValue + 1) < rounded_value) {
            sendDataToServer("sensorValues", idKey, rounded_value, false);
            oldInternalTempValue = rounded_value;
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
    Serial.println(SSID);

    WiFi.begin(SSID, PASSWORD);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());


    // Listen events for incoming data from server
    webSocket.on("connect", socketConnected);
    webSocket.on("disconnect", socketDisconnected);
    webSocket.on("authentication", authenticateFeedback);
    webSocket.on("setpoints", manageServerSetpoints);


    // Setup Connection
    if (USE_SSL) {
        webSocket.beginSSL(HOST, PORT, PATH, SSL_FINGERPRINT);
    } else {
        webSocket.begin(HOST, PORT, PATH);
    }
}

void loop() {
    // Reads the value of the the sensor given and saves the value/values to global variable/variables
    readSensorValue("temperature");
    readSensorValue("co2");

    if (authenticatedByServer) {
        // Based on the current values and the set-points given from the server, output is set accordingly
        if (!surveillanceModeTemp) {
            setOutputState("temperature", temperatureSetpoint, tempValue, HEATER_OUTPUT_PIN, tempActuatorReversed,
                           TEMP_SENSOR_KEY);
        }

        if (!surveillanceModeCo2) {
            setOutputState("co2", co2Setpoint, co2Value, VENTILATION_OUTPUT_PIN, co2ActuatorReversed,
                           CO2_SENSOR_KEY);
        }

        // TODO - Remove troubleshooting console prints
        //Serial.print("Temp reading: ");
        //Serial.println(tempValue);
        //Serial.print("CO2 reading: ");
        //Serial.println(co2Value);

        // Checks if the current value has changed with more than 0.1 Celsius for temp-sensors, or 0.1 PPM for CO2-sensors
        // and if the value has changed since last iteration emits a new message to the server
        checkForSensorChange("temperature", TEMP_SENSOR_KEY);
        checkForSensorChange("co2", CO2_SENSOR_KEY);
        checkForSensorChange("internal-temp", INTERNAL_TEMP_SENSOR_KEY);

    }

    webSocket.loop();
}
