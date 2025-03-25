#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <FirebaseClient.h>
#include <secrets.h>

#define ROOM_NAME "library"

#define REED_SWITCH_PIN 14
#define LED_BUILTIN 2

TaskHandle_t ledTaskHandle = NULL;
volatile bool flashLED = false;

WiFiClientSecure ssl_client;
using AsyncClient = AsyncClientClass;
AsyncClient async_client(ssl_client);

ServiceAuth sa_auth(CLIENT_EMAIL, PROJECT_ID, PRIVATE_KEY);
FirebaseApp app;
Firestore::Documents Docs;

AsyncResult firestoreResult;

bool lastReedState = false;
unsigned long lastChangeTime = 0;

void ledFlashTask(void* parameter);
void WiFiEvent(WiFiEvent_t event);
void processData(AsyncResult& aResult);
void updateFirestoreStatus(bool isOpen, String timestampString);
void logSwitchChange(bool newState, String timestampString);
time_t getTime();
String getTimestampString(time_t seconds);

void setup() {
    Serial.begin(115200);

    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);

    pinMode(REED_SWITCH_PIN, INPUT_PULLUP);
    lastReedState = digitalRead(REED_SWITCH_PIN);

    xTaskCreatePinnedToCore(ledFlashTask, "LED_Flash", 2048, NULL, 1, &ledTaskHandle, 0);

    WiFi.onEvent(WiFiEvent);
    WiFi.begin(AP_SSID, AP_PASSWORD);
}

void loop() {
    app.loop();
    bool currentReedState = !digitalRead(REED_SWITCH_PIN);
    if (currentReedState != lastReedState) {
        String timestampString = getTimestampString(getTime());
        updateFirestoreStatus(!currentReedState, timestampString);
        logSwitchChange(!currentReedState, timestampString);

        lastReedState = currentReedState;
        lastChangeTime = millis();
    }

    processData(firestoreResult);
    delay(500);
}

void ledFlashTask(void* parameter) {
    pinMode(LED_BUILTIN, OUTPUT);

    for (;;) {
        if (flashLED) {
            digitalWrite(LED_BUILTIN, HIGH);
            vTaskDelay(pdMS_TO_TICKS(500));
            digitalWrite(LED_BUILTIN, LOW);
            vTaskDelay(pdMS_TO_TICKS(500));
        }
        else {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

void WiFiEvent(WiFiEvent_t event) {
    switch (event) {
    case ARDUINO_EVENT_WIFI_STA_START:
        Serial.println("WiFi connecting...");
        flashLED = true;
        break;

    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
        Serial.println("WiFi connected!");
        flashLED = false;
        delay(500); // Without this, sometimes the LED doesn't turn on after previously losing connection, race condition probably
        digitalWrite(LED_BUILTIN, HIGH);
        break;

    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
        Serial.println("WiFi ready! IP: " + WiFi.localIP().toString());
        digitalWrite(LED_BUILTIN, HIGH);

        ssl_client.setInsecure();

        configTime(0, 0, "pool.ntp.org");
        app.setTime(getTime());
        
        initializeApp(async_client, app, getAuth(sa_auth), processData, "AuthTask");
        app.getApp<Firestore::Documents>(Docs);
        break;

    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
        Serial.println("WiFi lost connection");
        flashLED = true;
        WiFi.reconnect();
        break;
    }
}

void processData(AsyncResult& aResult) {
    if (!aResult.isResult()) return;

    if (aResult.isError()) {
        Serial.printf("Firebase Error: %s (Code: %d)\n",
            aResult.error().message().c_str(),
            aResult.error().code());
    }

    if (aResult.available()) {
        Serial.printf("Firebase Response: %s\n", aResult.c_str());
    }
}

void updateFirestoreStatus(bool isOpen, String timestampString) {
    Document<Values::Value> stateDoc;
    stateDoc.add("roomName", Values::Value(Values::StringValue(ROOM_NAME)));
    stateDoc.add("state", Values::Value(Values::StringValue(isOpen ? "open" : "closed")));
    stateDoc.add("isOpen", Values::Value(Values::BooleanValue(isOpen)));
    stateDoc.add("updatedAt", Values::Value(Values::TimestampValue(timestampString)));

    PatchDocumentOptions patchOptions(DocumentMask("roomName,state,isOpen,updatedAt"), DocumentMask(), Precondition());

    String docPath = "status/" + String(ROOM_NAME);
    Docs.patch(async_client, Firestore::Parent(PROJECT_ID), docPath, patchOptions, stateDoc, processData, "StatusUpdateTask");

    Serial.printf("Status Update: %s\n", isOpen ? "Open" : "Closed");
}

void logSwitchChange(bool newState, String timestampString) {
    Document<Values::Value> logDoc;
    logDoc.add("roomName", Values::Value(Values::StringValue(ROOM_NAME)));
    logDoc.add("newState", Values::Value(Values::StringValue(newState ? "open" : "closed")));
    logDoc.add("isOpen", Values::Value(Values::BooleanValue(newState)));
    logDoc.add("timestamp", Values::Value(Values::TimestampValue(timestampString)));

    Docs.createDocument(async_client, Firestore::Parent(PROJECT_ID), "logs", DocumentMask(), logDoc, processData, "LogCreationTask");
}

time_t getTime() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        Serial.println("Failed to obtain time");
        return 0;
    }
    return mktime(&timeinfo);
}

String getTimestampString(time_t seconds) {
    struct tm timeinfo;
    localtime_r(&seconds, &timeinfo);
    char buffer[80];
    strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
    return String(buffer);
}