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
float previousTempValue;
float previousInternalTempValue;
float previousCo2Value;
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




/////////////////////////////////////
////// ESP32 Socket.IO Client ///////
/////////////////////////////////////

SocketIoClient webSocket;
WiFiClient client;


/**
 * Function that is called when the ESP32 creates a connection with the server. On connection
 * some sentences is printed to the console. Then emits a password to the server for authentication.
 * @param payload     contains the data sent from server with event "connect"
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
 * Function that is called if connection with the server is lost. And sets variable that describes the ESP32 is not
 * authenticated.
 * @param payload     contains the data sent from server with event "disconnect"
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
 * @param payload     contains the data sent from server with event "authentication"
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

/**
 * Function that checks the value of each key that is received as set-points, has a float number or the value of none.
 * If the value is none, then surveillance-mode for that sensor and corresponding actuator is set. If a float for a
 * set-point is received, then that sensor and corresponding actuator is in normal regulation mode. The mode selected is
 * also printed to the console.
 * @param serverData     contains the server sensor keys and corresponding set-points sent from the server in JSON format
 */
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
 * Function that manipulated the payload data into a string that can be formatted with ArduinoJSON methods. Saves the
 * unique keys and corresponding set-points and saves it in JSON format. Error checking if the formatting worked, if
 * not prints a status message to console for troubleshooting. Calls the function determineMode with the JSON object
 * for determining the operation mode of the sensors and corresponding actuators.
 * @param payload     contains the data sent from server with event "setpoints"
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

    // TODO - Remove troubleshooting console printing
    Serial.println(temperatureSetpoint);
    Serial.println(co2Setpoint);

}

/**
 * Function that depending on if is called with sensor type as temperature or co2, and what ESP32 pin number, reads
 * the analog value of the relevant pin and scales the value appropriately. Then returns the value as a float number to
 * the global variable that holds the sensor value for the relevant sensor.
 * @param sensorType     specifies what type of sensor is connected to the inputPin
 * @param inputPin       specifies the ESP32 pin that the analog value is read from
 */
float readSensorValue (String sensorType, int inputPin) {
    float raw_co2;
    float raw_temp;

    if (sensorType == "temperature") {
        raw_temp = analogRead(inputPin);
        return (raw_temp / 4095.0) * 70.0;
    } else if (sensorType == "co2") {
        raw_co2 = analogRead(inputPin);
        return (raw_co2 / 4095.0) * 2000.0;
    } else {
        Serial.println("Invalid sensor type argument given to readSensorValue function");
    }

}

/**
 * Function that checks what kind of output actuator type is used for the regulation, and a logic statement that
 * checks depending on if the output actuator is reversed, if the current value of the sensor is lower / higher than
 * the set point. Returns either true or false, which respectively turns the output HIGH or LOW.
 * @param setPoint             the value of the set-point given from the server
 * @param currentValue         the most recent process value of the sensor
 * @param outputReversed       determines what kind of regulation is used for the actuator (direct / reverse control)
 */
bool checkSensor (float setPoint, float currentValue, bool outputReversed) {
    if (outputReversed) {
        if (setPoint < currentValue) {
            return true;
        } else {
            return false;
        }
    } else {
        if (setPoint < currentValue) {
            return false;
        } else {
            return true;
        }
    }
}

/**
 * Function that takes in a bool value for the output and the outputPin that should be controlled. Uses these values to
 * turn on or off the output. An input or true gives HIGH output, and input of false gives LOW output.
 * @param outputPin     specifies the output pin of the ESP32 that is used to control the actuator
 * @param output        the value of what the output should be in bool values
 */
void setOutput (int outputPin, bool output) {
    if (output) {
        digitalWrite(outputPin, HIGH);
    } else {
        digitalWrite(outputPin, LOW);
    }
}

/**
 * Function that first checks what kind of data that should be sent, could be output states or sensor values, as this
 * function is used for all sending of data to server. Further formats the output data to JSON format, with sensor values
 * if the type of data is "sensorValues". Or sends the output state if type of data is "output". Then sends the values
 * with websockets using event "sensorData".
 * @param typeOfData       argument that determines if the data should be sent as output states or sensor values
 * @param idKey            name of the sensor / actuator ID
 * @param sensorValue      the current value of the sensor [only used when this function is used for sending sensor values]
 * @param outputState      the current value of the output [only used when this function is used for sending output states]
 */
void sendDataToServer(String typeOfData, String idKey, float sensorValue, bool outputState) {
    if (typeOfData == "output") {
        String data = String("{\"ControlledItemID\":\"" + String(idKey) + "\",\"value\":" + String(outputState) + "}");
        // TODO - remove console printing for troubleshooting
        Serial.println(data);
        webSocket.emit("sensorData", data.c_str());

    } else if (typeOfData == "sensorValues") {
        String data = String("{\"SensorID\":\"" + String(idKey) + "\",\"value\":" + String(sensorValue) + "}");
        // TODO - remove console printing for troubleshooting
        Serial.println(data);
        webSocket.emit("sensorData", data.c_str());

    } else {
        Serial.println("Invalid type of data");
    }

}

/**
 * Function that finds out what state the output should be in by calling the checkSensor functions with relevant
 * parameters. Depending on what kind of sensor / actuator pair of regulation is called with this function, goes into
 * the appropriate if statement condition. Then checks if the output state has changed since the last iteration, if the
 * output has changed the if statement is entered, and output is set. Further variables that holds the previous output
 * state is set again to that value, for future iterations to check against. Then this output state is sent to the server
 * using the function sendDataToServer.
 * @param typeOfData          argument that states the output actuator type
 * @param setpoint            the value of the set-point given from the server
 * @param currentValue        the most recent process value of the sensor
 * @param outputPin           specifies the output pin of the ESP32 that is used to control the actuator
 * @param outputReversed      determines what kind of regulation is used for the actuator (direct / reverse control)
 * @param idKey               name of the sensor / actuator ID
 */
void setOutputState (String typeOfData, float setpoint, float currentValue, int outputPin, bool outputReversed, String idKey) {
    bool output = checkSensor(setpoint, currentValue, outputReversed);

    if (typeOfData == "temperature") {
        if (output != previousTempOutputState) {
            setOutput(outputPin, output);
            previousTempOutputState = output;
            sendDataToServer("output", idKey, 0.0, output);
        }
    } else if (typeOfData == "co2") {
        if (output != previousCo2OutputState) {
            setOutput(outputPin, output);
            previousCo2OutputState = output;
            sendDataToServer("output", idKey, 0.0, output);
        }
    }
}

/**
 * Function that takes in a float number and rounds the number to a specified number of decimal places, which is
 * given in the second parameter, decimals.
 * @param input           input number in float that will be manipulated to a rounded decimal number
 * @param decimals        specifies how many decimal places the rounding algorithm will perform
 */
float decimalRound(float input, int decimals) {
    float scale=pow(10,decimals);
    return round(input*scale)/scale;
}

/**
 * Function that first checks what kind of process value is being processed. Then finds the rounded value of the
 * process current value called with the function. Further uses this value to check if the process value has changed
 * enough to warrant a message sent with websockets to the server. The limits for how much the process value has to
 * change depends on what kind of type of sensor is called. When the data is sent, a new previous sensor value is set
 * for future iterations to check against.
 * @param typeOfData         argument that determines if the data should be sent as output states or sensor values
 * @param idKey              name of the sensor / actuator ID
 * @param currentValue       the most recent process value of the sensor
 */
void checkForSensorChange (String typeOfData, String idKey, float currentValue) {
    float rounded_value;

    if (typeOfData == "temperature") {
        rounded_value = decimalRound(currentValue, 1);

        if ((previousTempValue - 1) > rounded_value or (previousTempValue + 1) < rounded_value) {
            sendDataToServer("sensorValues", idKey, rounded_value, false);
            previousTempValue = rounded_value;
        }

    } else if (typeOfData == "co2") {
        rounded_value = decimalRound(currentValue, 1);

        if ((previousCo2Value - 50) > rounded_value or (previousCo2Value + 50) < rounded_value) {
            sendDataToServer("sensorValues", idKey, rounded_value, false);
            previousCo2Value = rounded_value;
        }
    } else if (typeOfData == "internal-temp") {

        // TODO - Remove troubleshooting console printing
        //Serial.print("Internal CPU Temperature: ");
        // Convert raw temperature in F to Celsius degrees
        //Serial.print((temprature_sens_read() - 32) / 1.8);
        //Serial.println(" C");

        internalTempValue = ((temprature_sens_read() - 32) / 1.8);
        rounded_value = decimalRound(internalTempValue, 1);

        if ((previousInternalTempValue - 1) > rounded_value or (previousInternalTempValue + 1) < rounded_value) {
            sendDataToServer("sensorValues", idKey, rounded_value, false);
            previousInternalTempValue = rounded_value;
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


    // Listen events for all websockets events from raspberryPiServer
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
    tempValue = readSensorValue("temperature", TEMP_INPUT_PIN);
    co2Value = readSensorValue("co2", CO2_INPUT_PIN);

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
        checkForSensorChange("temperature", TEMP_SENSOR_KEY, tempValue);
        checkForSensorChange("co2", CO2_SENSOR_KEY, co2Value);

        // TODO - Figure out why the internal temp gives ~80 degrees celsius
        checkForSensorChange("internal-temp", INTERNAL_TEMP_SENSOR_KEY, 0);

    }

    webSocket.loop();
}
