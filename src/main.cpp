#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <FirebaseClient.h>
#include <secrets.h>

#define ROOM_NAME "library"

#define REED_SWITCH_PIN 14

WiFiClientSecure ssl_client;
using AsyncClient = AsyncClientClass;
AsyncClient async_client(ssl_client);

ServiceAuth sa_auth(CLIENT_EMAIL, PROJECT_ID, PRIVATE_KEY);
FirebaseApp app;
Firestore::Documents Docs;

AsyncResult firestoreResult;

bool lastReedState = false;
unsigned long lastChangeTime = 0;

void processData(AsyncResult& aResult);
void updateFirestoreStatus(bool isOpen, String timestampString);
void logSwitchChange(bool newState, String timestampString);
time_t getTime();
String getTimestampString(time_t seconds);

void setup() {
    Serial.begin(115200);

    pinMode(REED_SWITCH_PIN, INPUT_PULLUP);
    lastReedState = digitalRead(REED_SWITCH_PIN);

    WiFi.begin(AP_SSID, AP_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(300);
        Serial.print(".");
    }
    Serial.println("\nWiFi Connected");

    ssl_client.setInsecure();
    configTime(0, 0, "pool.ntp.org");

    app.setTime(getTime());

    initializeApp(async_client, app, getAuth(sa_auth), processData, "AuthTask");
    app.getApp<Firestore::Documents>(Docs);
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