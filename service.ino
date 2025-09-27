#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

// Program states for the state machine
enum ProgramState {
  STATE_MAIN_MENU,
  STATE_ASK_INTERVAL,
  STATE_SENDING_DATA,
  STATE_CONFIG_MENU
};
ProgramState currentState = STATE_MAIN_MENU;

// Config parameters (Strings to allow runtime changes)
String ssid = "WIFI_NETWORK_NAME";
String password = "WIFI_NETWORK_PASSWORD";
String server_ip = "10.188.200.6";
int server_port = 8002;
String controller_id = "ESP8266_01";
String api_endpoint = "/api/v1/measurements";

// Sensor configuration
const int AirValue = 790;
const int WaterValue = 390;
const int SensorPin = A0;

// Operation variables
unsigned long send_interval_ms = 10000; // Default value
unsigned long last_send_time = 0;
long sum_of_readings = 0;
int reading_count = 0;
bool printMenu = true; // Flag to control menu printing

// Function declarations
void printMainMenu();
void handleMainMenuInput(String input);
void printConfigMenu();
void handleConfigMenuInput(String input);
void handleSerialInput();
void connectToWiFi();
void sendDataToRestAPI(int humidity_percent);

// =================================================================
// SETUP: Runs once on boot
// =================================================================
void setup() {
  Serial.begin(115200);
  delay(1000); // Wait for serial monitor to open
  Serial.println("\n\n--- Soil Moisture Monitoring Device ---");
  Serial.println("Connecting to initial WiFi...");
  connectToWiFi();
}

// =================================================================
// LOOP: Runs continuously
// =================================================================
void loop() {
  // Handle user input from the serial port
  handleSerialInput();

  // State machine handler
  switch (currentState) {
    case STATE_MAIN_MENU:
      if (printMenu) {
        printMainMenu();
        printMenu = false; // Only print the menu once
      }
      break;

    case STATE_ASK_INTERVAL:
      if (printMenu) {
        Serial.println("\nHow many seconds between data submissions?");
        Serial.print("Enter a number and press Enter: ");
        printMenu = false;
      }
      break;
      
    case STATE_SENDING_DATA:
      // Data submission logic using millis() timer
      if (millis() - last_send_time >= send_interval_ms) {
        if (WiFi.status() != WL_CONNECTED) {
          Serial.println("WiFi connection lost. Reconnecting...");
          connectToWiFi();
        }

        if (WiFi.status() == WL_CONNECTED) {
          int average_humidity_percent = 0;
          if (reading_count > 0) {
            int average_raw_value = sum_of_readings / reading_count;
            average_humidity_percent = map(average_raw_value, AirValue, WaterValue, 0, 100);
            if (average_humidity_percent < 0) average_humidity_percent = 0;
            if (average_humidity_percent > 100) average_humidity_percent = 100;
          }
          Serial.printf("Current humidity: %d%% (average of %d readings)\n", average_humidity_percent, reading_count);
          sendDataToRestAPI(average_humidity_percent);
        } else {
          Serial.println("Connection failed. Cannot send data.");
        }
        
        sum_of_readings = 0;
        reading_count = 0;
        last_send_time = millis();
      }
      // Accumulate readings continuously
      sum_of_readings += analogRead(SensorPin);
      reading_count++;
      break;

    case STATE_CONFIG_MENU:
      if (printMenu) {
        printConfigMenu();
        printMenu = false;
      }
      break;
  }
}

// =================================================================
// SERIAL INPUT HANDLING
// =================================================================
void handleSerialInput() {
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    input.trim(); // Clear leading/trailing whitespace

    if (currentState == STATE_SENDING_DATA) {
      if (input.equalsIgnoreCase("s")) {
        Serial.println("\n>> Process stopped. Returning to main menu.");
        currentState = STATE_MAIN_MENU;
        printMenu = true;
      }
    } else if (currentState == STATE_MAIN_MENU) {
      handleMainMenuInput(input);
    } else if (currentState == STATE_ASK_INTERVAL) {
      int seconds = input.toInt();
      if (seconds > 0) {
        send_interval_ms = seconds * 1000;
        Serial.printf("\nOK. Data will be sent every %d seconds.\n", seconds);
        Serial.println(">> Starting data submission... Type 's' and press Enter to stop.");
        currentState = STATE_SENDING_DATA;
        last_send_time = millis(); // Start timer now
        sum_of_readings = 0;
        reading_count = 0;
      } else {
        Serial.println("Invalid number. Please try again.");
        printMenu = true; // Ask again
      }
    } else if (currentState == STATE_CONFIG_MENU) {
      handleConfigMenuInput(input);
    }
  }
}

// =================================================================
// MENU LOGIC
// =================================================================

void handleMainMenuInput(String input) {
  if (input == "1") {
    currentState = STATE_ASK_INTERVAL;
    printMenu = true;
  } else if (input == "2") {
    currentState = STATE_CONFIG_MENU;
    printMenu = true;
  } else {
    Serial.println("Invalid option. Please try again.");
  }
}

void handleConfigMenuInput(String input) {
  String newValue;
  if (input == "1") {
    Serial.print("Enter new SSID: ");
    while (Serial.available() == 0) {} // Wait for input
    newValue = Serial.readStringUntil('\n');
    newValue.trim();
    ssid = newValue;
    Serial.println("SSID updated. Attempting to reconnect...");
    WiFi.disconnect();
    connectToWiFi();
  } else if (input == "2") {
    Serial.print("Enter new Password: ");
    while (Serial.available() == 0) {}
    newValue = Serial.readStringUntil('\n');
    newValue.trim();
    password = newValue;
    Serial.println("Password updated. Attempting to reconnect...");
    WiFi.disconnect();
    connectToWiFi();
  } else if (input == "3") {
    Serial.print("Enter new Server IP: ");
    while (Serial.available() == 0) {}
    newValue = Serial.readStringUntil('\n');
    newValue.trim();
    server_ip = newValue;
    Serial.println("Server IP updated.");
  } else if (input == "4") {
    Serial.print("Enter new Server Port: ");
    while (Serial.available() == 0) {}
    newValue = Serial.readStringUntil('\n');
    newValue.trim();
    server_port = newValue.toInt();
    Serial.println("Server Port updated.");
  } else if (input == "5") {
    Serial.print("Enter new Controller ID: ");
    while (Serial.available() == 0) {}
    newValue = Serial.readStringUntil('\n');
    newValue.trim();
    controller_id = newValue;
    Serial.println("Controller ID updated.");
  } else if (input == "6") {
    currentState = STATE_MAIN_MENU;
  } else {
    Serial.println("Invalid option.");
  }
  printMenu = true; // Show config menu again
}

// =================================================================
// MENU PRINTING FUNCTIONS
// =================================================================
void printMainMenu() {
  Serial.println("\n===== MAIN MENU =====");
  Serial.println("1. Start Sending Data");
  Serial.println("2. Configure Parameters");
  Serial.print("Choose an option: ");
}

void printConfigMenu() {
  Serial.println("\n===== CONFIGURATION MENU =====");
  Serial.println("Current Parameters:");
  Serial.println("  - SSID: " + ssid);
  Serial.println("  - Server IP: " + server_ip);
  Serial.println("  - Port: " + String(server_port));
  Serial.println("  - Controller ID: " + controller_id);
  Serial.println("---------------------------------");
  Serial.println("1. Change SSID");
  Serial.println("2. Change Password");
  Serial.println("3. Change Server IP");
  Serial.println("4. Change Server Port");
  Serial.println("5. Change Controller ID");
  Serial.println("6. Return to Main Menu");
  Serial.print("Choose an option: ");
}

// =================================================================
// NETWORK FUNCTIONS
// =================================================================
void connectToWiFi() {
  Serial.println("Connecting to " + ssid);
  WiFi.begin(ssid.c_str(), password.c_str());
  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 30) {
    delay(500);
    Serial.print(".");
    retries++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected to WiFi network!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nCould not connect to WiFi.");
  }
}

void sendDataToRestAPI(int humidity_percent) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    WiFiClient client;
    String server_url = "http://" + server_ip + ":" + String(server_port) + api_endpoint;
    if (http.begin(client, server_url)) {
      http.addHeader("Content-Type", "application/json");
      String json_payload = "{\"id_controller\":\"" + controller_id + "\",\"soil_humidity\":" + String(humidity_percent) + "}";
      int http_response_code = http.POST(json_payload);
      if (http_response_code < 0) {
        Serial.printf("[HTTP] POST failed, error: %s\n", http.errorToString(http_response_code).c_str());
      }
    } else {
      Serial.printf("[HTTP] Connection failed to %s\n", server_url.c_str());
    }
    http.end();
  }
}