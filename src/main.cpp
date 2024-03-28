#include <M5Core2.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include "WiFi.h"
#include "FS.h"                 // SD Card ESP32
#include <EEPROM.h>             // read and write from flash memory
#include <NTPClient.h>          // Time Protocol Libraries
#include <WiFiUdp.h>            // Time Protocol Libraries
#include <Adafruit_VCNL4040.h>  // Sensor libraries
#include "Adafruit_SHT4x.h"     // Sensor libraries
#include <cstdlib>

////////////////////////////////////////////////////////////////////
// TODO 1: Enter your URL addresses
////////////////////////////////////////////////////////////////////
const String URL_GCF_UPLOAD = "https://us-central1-egr425-project4-rsk.cloudfunctions.net/RawDataStore";
const String URL_GCF_RETRIEVE = "https://us-central1-egr425-project4-rsk.cloudfunctions.net/AvgData";

////////////////////////////////////////////////////////////////////
// Variables
////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////
// TODO 2: Enter your WiFi Credentials
////////////////////////////////////////////////////////////////////
String wifiNetworkName = "CBU-LANCERS";
String wifiPassword = "LiveY0urPurp0se";

// Initialize library objects (sensors and Time protocols)
Adafruit_VCNL4040 vcnl4040 = Adafruit_VCNL4040();
Adafruit_SHT4x sht4 = Adafruit_SHT4x();
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

// Time variables
unsigned long lastTime = 0;
unsigned long timerDelay = 1000; 

// Variables
bool gotNewDetails = false;
unsigned long timeDuration = 0;
String timeDurStr = "";
String dataType = "";
String selectId = "";

// Screen states
enum Screen { S_CLOUD, S_SELECT_AVG, S_DATA_RESULT};
static Screen screen = S_CLOUD;
static bool stateChangedThisLoop = true;

//Username
String userId = "userId2"; // Dummy User ID

//buttons
//x,y,width, height, bool, text, oncolours, off colours
ButtonColors offCol = {BLACK, WHITE, WHITE};
ButtonColors onColTut = {TFT_LIGHTGREY, BLACK, NODRAW};
Button userID1_BTN(20, 30, 60, 40, false, "ID 1", onColTut, offCol); 
Button userID2_BTN(100, 30, 60, 40, false, "ID 2", onColTut, offCol);
Button userIDall_BTN(180, 30, 60, 40, false, "All", onColTut, offCol);
Button timeDuration_30_BTN(20, 110, 60, 40, false, "30 sec", onColTut, offCol);
Button timeDuration_2_BTN(100, 110, 60, 40, false, "2 min", onColTut, offCol);
Button timeDuration_5_BTN(180, 110, 60, 40, false, "5 min", onColTut, offCol);
Button dataType_T_BTN(20, 190, 60, 40, false, "Temp", onColTut, offCol);
Button dataType_H_BTN(85, 190, 60, 40, false, "Hum", onColTut, offCol);
Button dataType_W_BTN(160, 190, 70, 40, false, "W-Light", onColTut, offCol);
Button SELECT_BTN(250, 190, 60, 40, false, "->", onColTut, offCol);


////////////////////////////////////////////////////////////////////
// Device Details Structure
////////////////////////////////////////////////////////////////////
struct deviceDetails {
    int prox;
    int ambientLight;
    int whiteLight;
    double rHum;
    double temp;
    double accX;
    double accY;
    double accZ;
    long long timeCaptured;
    long long cloudUploadTime;
};

struct avgDetails {
    String dataType;
    long long minTime;
    long long maxTime;
    int numDataPoints;
    double avg;
    double rate;
};

avgDetails avgDocDetails;

// Device details
deviceDetails details;

////////////////////////////////////////////////////////////////////
// Method header declarations
////////////////////////////////////////////////////////////////////
int httpGetWithHeaders(String serverURL, String *headerKeys, String *headerVals, int numHeaders);
bool gcfGetWithHeader(String serverUrl, String userId, time_t time, deviceDetails *details);
String generateM5DetailsHeader(String userId, time_t time, deviceDetails *details);
int httpPostFile(String serverURL, String *headerKeys, String *headerVals, int numHeaders, String filePath);
bool gcfPostFile(String serverUrl, String filePathOnSD, String userId, time_t time, deviceDetails *details);
String writeDataToFile(byte * fileData, size_t fileSizeInBytes);
int getNextFileNumFromEEPROM();
double convertFintoC(double f);
double convertCintoF(double c);
String generateUserIdHeader(String userId, unsigned long time, String dataType);
bool gcfGetWithUserHeader(String serverUrl, String userId, unsigned long time, String dataType, avgDetails *avgDocDetails);
int httpGetLatestWithHeaders(String serverURL, String *headerKeys, String *headerVals, int numHeaders, avgDetails *details);
void drawS_CLOUD();
String time_to_timestamp(long millis);
String currTimestamp();
void drawS_SELECT_AVG();
void drawS_DATA_RESULT();
void hideButtons();
void selectTapped(Event& e);

void id1Tapped(Event& e);
void id2Tapped(Event& e);
void allTapped(Event& e);

void thirtySecTapped(Event& e);
void twoMinTapped(Event& e);
void fiveMinTapped(Event& e);

void tempTapped(Event& e);
void humTapped(Event& e);
void lightTapped(Event& e);


///////////////////////////////////////////////////////////////
// Put your setup code here, to run once
///////////////////////////////////////////////////////////////
void setup()
{
    ///////////////////////////////////////////////////////////
    // Initialize the device
    ///////////////////////////////////////////////////////////
    M5.begin();
    M5.IMU.Init();

    ///////////////////////////////////////////////////////////
    // Initialize Sensors
    ///////////////////////////////////////////////////////////

    // Initialize VCNL4040
    if (!vcnl4040.begin()) {
        Serial.println("Couldn't find VCNL4040 chip");
        while (1) delay(1);
    }
    Serial.println("Found VCNL4040 chip");

    // Initialize SHT40
    if (!sht4.begin())
    {
        Serial.println("Couldn't find SHT4x");
        while (1) delay(1);
    }
    Serial.println("Found SHT4x sensor");
    sht4.setPrecision(SHT4X_HIGH_PRECISION);
    sht4.setHeater(SHT4X_NO_HEATER);

    ///////////////////////////////////////////////////////////
    // Connect to WiFi
    ///////////////////////////////////////////////////////////
    WiFi.begin(wifiNetworkName.c_str(), wifiPassword.c_str());
    Serial.printf("Connecting");
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }
    Serial.print("\n\nConnected to WiFi network with IP address: ");
    Serial.println(WiFi.localIP());

    ///////////////////////////////////////////////////////////
    // Init time connection
    ///////////////////////////////////////////////////////////
    timeClient.begin();
    timeClient.setTimeOffset(3600 * -7);

    SELECT_BTN.addHandler(selectTapped, E_TAP);
    userID1_BTN.addHandler(id1Tapped, E_TAP);
    userID2_BTN.addHandler(id2Tapped, E_TAP);
    userIDall_BTN.addHandler(allTapped, E_TAP);
    timeDuration_30_BTN.addHandler(thirtySecTapped, E_TAP);
    timeDuration_2_BTN.addHandler(twoMinTapped, E_TAP);
    timeDuration_5_BTN.addHandler(fiveMinTapped, E_TAP);
    dataType_T_BTN.addHandler(tempTapped, E_TAP);
    dataType_H_BTN.addHandler(humTapped, E_TAP);
    dataType_W_BTN.addHandler(lightTapped, E_TAP);
}

///////////////////////////////////////////////////////////////
// Put your main code here, to run repeatedly
///////////////////////////////////////////////////////////////
void loop()
{
    // Read in button data and store
    M5.update();

    // tap diff buttons to get to the diff screens
    if (M5.BtnA.wasPressed() && screen != S_CLOUD) {
        screen = S_CLOUD;
        stateChangedThisLoop = true;
        lastTime = millis();
    }
    if (M5.BtnB.wasPressed() && screen != S_SELECT_AVG) {
        selectId = "";
        dataType = "";
        timeDuration = 0;
        timeDurStr = "";
        screen = S_SELECT_AVG;
        stateChangedThisLoop = true;
        lastTime = millis();
    }

    ///////////////////////////////////////////////////////////
    // Read Sensor Values
    ///////////////////////////////////////////////////////////
    // Read VCNL4040 Sensors
    uint16_t prox = vcnl4040.getProximity();
    uint16_t ambientLight = vcnl4040.getLux();
    uint16_t whiteLight = vcnl4040.getWhiteLight(); //vcnl4040.getWhiteLight();

    // Read SHT40 Sensors
    sensors_event_t rHum, temp;
    sht4.getEvent(&rHum, &temp); // populate temp and humidity objects with fresh data

    // Read M5's Internal Accelerometer (MPU 6886)
    float accX;
    float accY;
    float accZ;
    M5.IMU.getAccelData(&accX, &accY, &accZ);
    accX *= 9.8;
    accY *= 9.8;
    accZ *= 9.8;

    ///////////////////////////////////////////////////////////
    // Setup data to upload to Google Cloud Functions
    ///////////////////////////////////////////////////////////
    // Get current time as timestamp of last update
    timeClient.update();
    unsigned long epochTime = timeClient.getEpochTime();

    ///////////////////////////////////////////////////////////
    // Post data
    ///////////////////////////////////////////////////////////
    if ((millis() - lastTime) > timerDelay && screen == S_CLOUD) {
        details.prox = prox;
        details.ambientLight = ambientLight;
        details.whiteLight = whiteLight;
        details.temp = temp.temperature;
        details.rHum = rHum.relative_humidity;
        details.accX = accX;
        details.accY = accY;
        details.accZ = accZ;
        details.timeCaptured = epochTime;
        details.cloudUploadTime = 0;
        
        gcfGetWithHeader(URL_GCF_UPLOAD, userId, epochTime, &details);
        drawS_CLOUD();
        stateChangedThisLoop = true;
        lastTime = millis();
    }

    // Changing to and from screens
    if (stateChangedThisLoop) {
        if (screen == S_CLOUD) {
            if (gotNewDetails) {
                drawS_CLOUD();
            }
        } else if(screen == S_SELECT_AVG){
            //TODO
            drawS_SELECT_AVG();
            // if select button is clicked, set screen to S_DATA_RESULT to display the results of the call
        }
        
    }
    stateChangedThisLoop = false;
    gotNewDetails = false;
}

////////////////////////////////////////////////////////////////////
//Drawing Screens
////////////////////////////////////////////////////////////////////

void selectTapped(Event& e) {
    if (!selectId.isEmpty() && !dataType.isEmpty() && timeDuration != 0) {
        gcfGetWithUserHeader(URL_GCF_RETRIEVE, selectId, timeDuration, dataType, &avgDocDetails);
        screen = S_DATA_RESULT;
        Serial.println("Attempting to average data");
        SELECT_BTN.hide();
        drawS_DATA_RESULT();
        lastTime = millis();
    }
}

void id1Tapped(Event& e) {
    selectId = "userId1";
    stateChangedThisLoop = true;
}
void id2Tapped(Event& e) {
    selectId = "userId2";
    stateChangedThisLoop = true;
}
void allTapped(Event& e) {
    selectId = "All";
    stateChangedThisLoop = true;
}

void thirtySecTapped(Event& e) {
    timeDuration = 30000;
    timeDurStr = "30Sec";
    stateChangedThisLoop = true;
}

void twoMinTapped(Event& e) {
    timeDuration = 120000;
    timeDurStr = "2min";
    stateChangedThisLoop = true;
}

void fiveMinTapped(Event& e) {
    timeDuration = 300000;
    timeDurStr = "5min";
    stateChangedThisLoop = true;
}

void tempTapped(Event& e) {
    dataType = "temp";
    stateChangedThisLoop = true;
}

void humTapped(Event& e) {
    dataType = "rHum";
    stateChangedThisLoop = true;
}

void lightTapped(Event& e) {
    dataType = "rwl";
    stateChangedThisLoop = true;
}

void drawS_CLOUD() {
    M5.Lcd.clearDisplay();
    hideButtons();
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setCursor(120, 20);
    M5.Lcd.setTextColor(TFT_GREEN);
    M5.Lcd.setTextSize(1);
    M5.Lcd.print("Live Data");
    M5.Lcd.setCursor(10, 60);
    M5.Lcd.print("User Id: ");
    M5.Lcd.setTextColor(TFT_WHITE);
    M5.Lcd.print(userId);
    M5.Lcd.setCursor(10, 80);
    M5.Lcd.setTextColor(TFT_GREEN);
    M5.Lcd.print("Temp: ");
    M5.Lcd.setTextColor(TFT_WHITE);
    M5.Lcd.printf("%.2fC",details.temp);
    M5.Lcd.setCursor(10, 100);
    M5.Lcd.setTextColor(TFT_GREEN);
    M5.Lcd.print("Humidity: ");
    M5.Lcd.setTextColor(TFT_WHITE);
    M5.Lcd.printf("%.2f%%", details.rHum);
    M5.Lcd.setCursor(10, 120);
    M5.Lcd.setTextColor(TFT_GREEN);
    M5.Lcd.print("Raw White Light: ");
    M5.Lcd.setTextColor(TFT_WHITE);
    M5.Lcd.printf("%.2flux",details.whiteLight);
    M5.Lcd.setCursor(10, 140);
    M5.Lcd.setTextColor(TFT_GREEN);
    M5.Lcd.print("Ambient Light: ");
    M5.Lcd.setTextColor(TFT_WHITE);
    M5.Lcd.printf("%.2flux",details.ambientLight);
    M5.Lcd.setCursor(10, 160);
    M5.Lcd.setTextColor(TFT_GREEN);
    M5.Lcd.print("Accel: ");
    M5.Lcd.setTextColor(TFT_WHITE);
    M5.Lcd.printf("%.2fx, %.2fy, %.2fz", details.accX, details.accY, details.accZ);

    M5.Lcd.setCursor(60, 200);
    timeClient.update();
    unsigned long epochTime = timeClient.getEpochTime();
    unsigned long long epochMillis = ((unsigned long long)epochTime)*1000;
    M5.Lcd.print(currTimestamp()); // in seconds
}

String currTimestamp() {
    timeClient.update();
    unsigned long epochTime = timeClient.getEpochTime();
    unsigned long long epochMillis = ((unsigned long long)epochTime)*1000;
    struct tm *ptm = gmtime ((time_t *)&epochTime);
    String dateStr = String(ptm->tm_mon+1) + "/" + String(ptm->tm_mday) + "/" + String(ptm->tm_year+1900);
    String timeStr = String(timeClient.getHours() % 12) + ":" + String(timeClient.getMinutes()) + ":" + String(timeClient.getSeconds()) + String(timeClient.getHours() < 12 ? "AM" : "PM");
    return dateStr + " " + timeStr;
}

String time_to_timestamp(long time) { 
    struct tm *ptm = gmtime ((time_t *)&time);
    String dateStr = String(ptm->tm_mon+1) + "/" + String(ptm->tm_mday) + "/" + String(ptm->tm_year+1900);
    
    String timeStr = String((time  % 86400L) / 3600) + ":" + String((time % 3600) / 60) + ":" + String(time % 60) + String(((time  % 86400L) / 3600) < 12 ? "AM" : "PM");
    
    return dateStr + " " + timeStr;
}

void drawS_SELECT_AVG() {
    M5.Lcd.clearDisplay();
    M5.Lcd.setCursor(20,20);
    M5.Lcd.setTextColor(TFT_GREEN);
    M5.Lcd.print("User: ");
    M5.Lcd.setTextColor(TFT_WHITE);
    M5.Lcd.print(selectId);
    M5.Lcd.setCursor(20,100);
    M5.Lcd.setTextColor(TFT_GREEN);
    M5.Lcd.print("Time Duration: ");
    M5.Lcd.setTextColor(TFT_WHITE);
    M5.Lcd.print(timeDurStr);
    M5.Lcd.setCursor(20,180);
    M5.Lcd.setTextColor(TFT_GREEN);
    M5.Lcd.print("Data Type: ");
    M5.Lcd.setTextColor(TFT_WHITE);
    M5.Lcd.print(dataType);
    userID1_BTN.draw();
    userID2_BTN.draw();
    userIDall_BTN.draw();
    timeDuration_30_BTN.draw();
    timeDuration_2_BTN.draw();
    timeDuration_5_BTN.draw();
    dataType_T_BTN.draw();
    dataType_H_BTN.draw();
    dataType_W_BTN.draw();
    SELECT_BTN.draw();

    if (userID1_BTN == true){
        Serial.print("buttons contain a boolean");
    }
}


void drawS_DATA_RESULT() {
    M5.Lcd.clearDisplay();
    hideButtons();
    // TODO: retrieve all the relevant data from the cloud function in the future    hideButtons();   
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setCursor(60, 20);
    M5.Lcd.setTextColor(TFT_GREEN);
    M5.Lcd.setTextSize(1);
    M5.Lcd.print("Results of Averaged Data");
    M5.Lcd.setCursor(10, 50);
    M5.Lcd.print("User id: ");
    String userid = "";

    if (userID1_BTN) {
        userid = "userId1";
    } else if (userID2_BTN) {
        userid = "userId2";
    } else if (userIDall_BTN) {
        userid = "userId1, userId2";
    }
    //Changed
    M5.Lcd.setTextColor(TFT_WHITE);
    M5.Lcd.println(selectId);

    M5.Lcd.setCursor(150, 50);
    M5.Lcd.setTextColor(TFT_GREEN);
    M5.Lcd.print("Data Type: ");
    String avgDataType = avgDocDetails.dataType;
    M5.Lcd.setTextColor(TFT_WHITE);
    M5.Lcd.println(avgDataType);
    
    M5.Lcd.setCursor(10, 90);
    M5.Lcd.setTextColor(TFT_GREEN);
    M5.Lcd.print("Averaged data: ");
    String units = "";
    if (avgDataType.equalsIgnoreCase("temp")) {
        units = "C";
    } else if (avgDataType.equalsIgnoreCase("rhum")) {
        units = "%";
    } else {
        units = "lux";
    }

    M5.Lcd.setTextColor(TFT_WHITE);
    M5.Lcd.printf("%.2f %s", avgDocDetails.avg, units); // TODO: get the averaged data back w/ corresponding units
    
    M5.Lcd.setCursor(10, 120);
    M5.Lcd.setTextColor(TFT_GREEN);
    M5.Lcd.print("Range: ");
    String minRangeTime = time_to_timestamp(avgDocDetails.minTime);
    String maxRangeTime = time_to_timestamp(avgDocDetails.maxTime);
    Serial.println(minRangeTime);
    Serial.println(maxRangeTime);
    M5.Lcd.setTextColor(TFT_WHITE);
    M5.Lcd.print(minRangeTime);
    M5.Lcd.println(" - ");
    M5.Lcd.println(maxRangeTime);
    
    M5.Lcd.setCursor(10, 180);
    M5.Lcd.setTextColor(TFT_GREEN);
    M5.Lcd.print("# data points: ");
    M5.Lcd.setTextColor(TFT_WHITE);
    M5.Lcd.println(avgDocDetails.numDataPoints);
    
    M5.Lcd.setCursor(150, 180);
    M5.Lcd.setTextColor(TFT_GREEN);
    M5.Lcd.print("Rate: ");
    M5.Lcd.setTextColor(TFT_WHITE);
    M5.Lcd.printf("%.2f pts/sec", avgDocDetails.rate);
}

void hideButtons(){
    userID1_BTN.hide();
    userID2_BTN.hide();
    userIDall_BTN.hide();
    timeDuration_30_BTN.hide();
    timeDuration_2_BTN.hide();
    timeDuration_5_BTN.hide();
    dataType_T_BTN.hide();
    dataType_H_BTN.hide();
    dataType_W_BTN.hide();
    SELECT_BTN.hide();
}


/**/
////////////////////////////////////////////////////////////////////
// This method takes in a user ID, time and structure describing
// device details and makes a GET request with the data. 
////////////////////////////////////////////////////////////////////
bool gcfGetWithHeader(String serverUrl, String userId, time_t time, deviceDetails *details) {
    // Allocate arrays for headers
	const int numHeaders = 1;
    String headerKeys [numHeaders] = {"M5-Details"};
    String headerVals [numHeaders];

    // Add formatted JSON string to header
    headerVals[0] = generateM5DetailsHeader(userId, time, details);
    
    // Attempt to post the file
    int resCode = httpGetWithHeaders(serverUrl, headerKeys, headerVals, numHeaders);
    
    // Return true if received 200 (OK) response
    return (resCode == 200);
}

bool gcfGetWithUserHeader(String serverUrl, String userId, unsigned long time, String dataType, avgDetails *avgDocDetails) {
    // Allocate arrays for headers
	const int numHeaders = 1; 
    String headerKeys [numHeaders] = {"AVG-DETAIL-INFO"};
    String headerVals [numHeaders];

    // Add formatted JSON string to header
    headerVals[0] = generateUserIdHeader(userId, time, dataType);
    
    // Attempt to post the file
    Serial.println("Attempting post data.");
    int resCode = httpGetLatestWithHeaders(serverUrl, headerKeys, headerVals, numHeaders, avgDocDetails);
    
    // Return true if received 200 (OK) response
    return (resCode == 200);
}

String generateUserIdHeader(String userId, unsigned long time, String dataType) {
    // Allocate M5-Details Header JSON object
    StaticJsonDocument<650> objHeaderUserIdDetails; //DynamicJsonDocument  objHeaderGD(600);
    
    // Add VCNL details
    JsonObject avgDetails = objHeaderUserIdDetails.createNestedObject("avgDetails");
    avgDetails["userId"] = userId;
    avgDetails["timeDuration"] = time;
    avgDetails["dataType"] = dataType;

    // Convert JSON object to a String which can be sent in the header
    size_t jsonSize = measureJson(objHeaderUserIdDetails) + 1;
    char cHeaderUserIdDetails [jsonSize];
    serializeJson(objHeaderUserIdDetails, cHeaderUserIdDetails, jsonSize);
    String strHeadeUserIdDetails = cHeaderUserIdDetails;

    // Return the header as a String
    return strHeadeUserIdDetails ;
}

////////////////////////////////////////////////////////////////////
// Generates the JSON header with all the sensor details and user
// data and serializes to a String.
////////////////////////////////////////////////////////////////////
String generateM5DetailsHeader(String userId, time_t time, deviceDetails *details) {
    // Allocate M5-Details Header JSON object
    StaticJsonDocument<650> objHeaderM5Details; //DynamicJsonDocument  objHeaderGD(600);
    
    // Add VCNL details
    JsonObject objVcnlDetails = objHeaderM5Details.createNestedObject("vcnlDetails");
    objVcnlDetails["prox"] = details->prox;
    objVcnlDetails["al"] = details->ambientLight;
    objVcnlDetails["rwl"] = details->whiteLight;

    // Add SHT details
    JsonObject objShtDetails = objHeaderM5Details.createNestedObject("shtDetails");
    objShtDetails["temp"] = details->temp;
    objShtDetails["rHum"] = details->rHum;

    // Add M5 Sensor details
    JsonObject objM5Details = objHeaderM5Details.createNestedObject("m5Details");
    objM5Details["ax"] = details->accX;
    objM5Details["ay"] = details->accY;
    objM5Details["az"] = details->accZ;

    // Add Other details
    JsonObject objOtherDetails = objHeaderM5Details.createNestedObject("otherDetails");
    objOtherDetails["timeCaptured"] = time;
    objOtherDetails["userId"] = userId;

    // Convert JSON object to a String which can be sent in the header
    size_t jsonSize = measureJson(objHeaderM5Details) + 1;
    char cHeaderM5Details [jsonSize];
    serializeJson(objHeaderM5Details, cHeaderM5Details, jsonSize);
    String strHeaderM5Details = cHeaderM5Details;
    //Serial.println(strHeaderM5Details.c_str()); // Debug print

    // Return the header as a String
    return strHeaderM5Details;
}

////////////////////////////////////////////////////////////////////
// This method takes in a serverURL and array of headers and makes
// a GET request with the headers attached and then returns the response.
////////////////////////////////////////////////////////////////////
int httpGetWithHeaders(String serverURL, String *headerKeys, String *headerVals, int numHeaders) {
    // Make GET request to serverURL
    HTTPClient http;
    http.begin(serverURL.c_str());
    
    for (int i = 0; i < numHeaders; i++)
        http.addHeader(headerKeys[i].c_str(), headerVals[i].c_str());

    int httpResCode = http.GET();

    // Free resources and return response code
    http.end();
    return httpResCode;
}

int httpGetLatestWithHeaders(String serverURL, String *headerKeys, String *headerVals, int numHeaders, avgDetails *details) {
    // Make GET request to serverURL
    HTTPClient http;
    Serial.println("Starting Http");
    http.begin(serverURL.c_str());
    
    for (int i = 0; i < numHeaders; i++)
    http.addHeader(headerKeys[i].c_str(), headerVals[i].c_str());
    Serial.println("Added Headers");

    // Post the headers (NO FILE)
    int httpResCode = http.GET();
    String result = http.getString();

    if (httpResCode == 200) {
        Serial.println("GOOD RESPONSE");
    }

    Serial.println("Result:");
    Serial.println(result);

    if (httpResCode == 200) {
        
    Serial.println("deserializing");
    StaticJsonDocument<850> objHeaderM5Details;

    DeserializationError error = deserializeJson(objHeaderM5Details, result);
    if (error) {
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.c_str());
    }

    Serial.println("deserialized");
    String dataTypeLoc = objHeaderM5Details["AVG-INFO"]["Details"]["dataType"];
    Serial.print("data type:");
    Serial.println(dataTypeLoc);
    unsigned long minTime = objHeaderM5Details["AVG-INFO"]["Details"]["minTime"];
    Serial.print("Min Time:");
    Serial.println(minTime);
    unsigned long maxTime = objHeaderM5Details["AVG-INFO"]["Details"]["maxTime"];
    Serial.print("Max Time:");
    Serial.println(maxTime);
    int numDataPoints = objHeaderM5Details["AVG-INFO"]["Details"]["numDataPoints"];
    Serial.print("Num Data Points");
    Serial.println(numDataPoints);
    double avg = objHeaderM5Details["AVG-INFO"]["Details"]["avg"];
    Serial.print("AVG:");
    Serial.println(avg);
    double rate = objHeaderM5Details["AVG-INFO"]["Details"]["rate"];
    Serial.print("rate:");
    Serial.println(rate);

    Serial.println("converting details:");

    details->dataType = dataTypeLoc;
    details->minTime = minTime;
    details->maxTime = maxTime;
    details->numDataPoints = numDataPoints;
    details->avg = avg;
    details->rate = rate;
    }
    Serial.println("Done converting");
    // Free resources and return response code
    http.end();
    Serial.println("Ended HTTP");
    gotNewDetails = true;
    return httpResCode;
}

////////////////////////////////////////////////////////////////////
// Given an array of bytes, writes them out to the SD file system.
////////////////////////////////////////////////////////////////////
String writeDataToFile(byte * fileData, size_t fileSizeInBytes) {
    // Print status
    Serial.println("Attempting to write file to SD Card...");

    // Obtain file system from SD card
    fs::FS &sdFileSys = SD;

    // Generate path where new picture will be saved on SD card and open file
    int fileNumber = getNextFileNumFromEEPROM();
    String path = "/file_" + String(fileNumber) + ".txt";
    File file = sdFileSys.open(path.c_str(), FILE_WRITE);

    // If file was opened successfully
    if (file)
    {
        // Write image bytes to the file
        Serial.printf("\tSTATUS: %s FILE successfully OPENED\n", path.c_str());
        file.write(fileData, fileSizeInBytes); // payload (file), payload length
        Serial.printf("\tSTATUS: %s File successfully WRITTEN (%d bytes)\n\n", path.c_str(), fileSizeInBytes);

        // Update picture number
        EEPROM.write(0, fileNumber);
        EEPROM.commit();
    }
    else {
        Serial.printf("\t***ERROR: %s file FAILED OPEN in writing mode\n***", path.c_str());
        return "";
    }

    // Close file
    file.close();

    // Return file name
    return path;
}

////////////////////////////////////////////////////////////////////
// TODO 7: Implement Method
// Get file number by reading last file number in EEPROM (non-volatile
// memory area).
////////////////////////////////////////////////////////////////////
int getNextFileNumFromEEPROM() {
    #define EEPROM_SIZE 1             // Number of bytes you want to access
    EEPROM.begin(EEPROM_SIZE);
    int fileNumber = 0;               // Init to 0 in case read fails
    fileNumber = EEPROM.read(0) + 1;  // Variable to represent file number
    return fileNumber;
}

////////////////////////////////////////////////////////////////////
// TODO 8: Implement Method
// This method takes in an SD file path, user ID, time and structure
// describing device details and POSTs it. 
////////////////////////////////////////////////////////////////////
bool gcfPostFile(String serverUrl, String filePathOnSD, String userId, time_t time, deviceDetails *details) {
    // Allocate arrays for headers
    const int numHeaders = 3;
    String headerKeys [numHeaders] = { "Content-Type", "Content-Disposition", "M5-Details"};
    String headerVals [numHeaders];

    // Content-Type Header
    headerVals[0] = "text/plain";
    
    // Content-Disposition Header
    String filename = filePathOnSD.substring(filePathOnSD.lastIndexOf("/") + 1);
    String headerCD = "attachment; filename=" + filename;
    headerVals[1] = headerCD;

    // Add formatted JSON string to header
    headerVals[2] = generateM5DetailsHeader(userId, time, details);
    
    // Attempt to post the file
    int numAttempts = 1;
    Serial.printf("Attempting upload of %s...\n", filename.c_str());
    int resCode = httpPostFile(serverUrl, headerKeys, headerVals, numHeaders, filePathOnSD);
    
    // If first attempt failed, retry...
    while (resCode != 200) {
        // ...up to 9 more times (10 tries in total)
        if (++numAttempts >= 10)
            break;

        // Re-attempt
        Serial.printf("*Re-attempting upload (try #%d of 10 max tries) of %s...\n", numAttempts, filename.c_str());
        resCode = httpPostFile(serverUrl, headerKeys, headerVals, numHeaders, filePathOnSD);
    }

    // Return true if received 200 (OK) response
    return (resCode == 200);
}

////////////////////////////////////////////////////////////////////
// TODO 9: Implement Method
// This method takes in a serverURL and file path and makes a 
// POST request with the file (to upload) and then returns the response.
////////////////////////////////////////////////////////////////////
int httpPostFile(String serverURL, String *headerKeys, String *headerVals, int numHeaders, String filePath) {
    // Make GET request to serverURL
    HTTPClient http;
    http.begin(serverURL.c_str());
    
    // Add all the headers supplied via parameter
    for (int i = 0; i < numHeaders; i++)
        http.addHeader(headerKeys[i].c_str(), headerVals[i].c_str());
    
    // Open the file, upload and then close
    fs::FS &sdFileSys = SD;
    File file = sdFileSys.open(filePath.c_str(), FILE_READ);
    int httpResCode = http.sendRequest("POST", &file, file.size());
    file.close();

    // Print the response code and message
    Serial.printf("\tHTTP%scode: %d\n\t%s\n\n", httpResCode > 0 ? " " : " error ", httpResCode, http.getString().c_str());

    // Free resources and return response code
    http.end();
    return httpResCode;
}

/////////////////////////////////////////////////////////////////
// Convert between F and C temperatures
/////////////////////////////////////////////////////////////////
double convertFintoC(double f) { return (f - 32) * 5.0 / 9.0; }
double convertCintoF(double c) { return (c * 9.0 / 5.0) + 32; }