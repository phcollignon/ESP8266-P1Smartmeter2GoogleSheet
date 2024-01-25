#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include "HTTPSRedirect.h"

// ** User defined **
const char *WIFI_SSID = "SkyObject";          // Your WiFi SSID
const char *WIFI_PASSWORD = "phico4wifi7ok";  // Your WiFi password
// Google script ID, please look readme to create Google Sheet, App Script and genereate Google Script Deployment ID
const char *GOOGLE_SCRIPT_ID = "AKfycbzfaVDOb8MshUzbTP7UMtbDO60Z6rLogYCGPp-_lQ64tM0Apv1lMXrRb0GWs9W8xjDxZw";
const int UPDATE_INTERVAL = 60000;  // in milliseconds
// ** End of user defined **

const int WIFI_TIMEOUT = 30000;
const int BAUD_RATE = 115200;
const int P1_MAXLINELENGTH = 1050;
long last_update_sent = 0;
char telegram[P1_MAXLINELENGTH];

long consumption_low_tarif;
long consumption_high_tarif;
long returndelivery_low_tarif;
long returndelivery_high_tarif;
long actual_consumption;
long actual_returndelivery;
long gas_meter_m3;
long l1_instant_power_usage;
long l2_instant_power_usage;
long l3_instant_power_usage;
long l1_instant_power_current;
long l2_instant_power_current;
long l3_instant_power_current;
long l1_voltage;
long l2_voltage;
long l3_voltage;
long actual_tarif;
long short_power_outages;
long long_power_outages;
long short_power_drops;
long short_power_peaks;

unsigned int current_crc = 0;

const char *GOOGLE_SCRIPT_HOST = "script.google.com";
const int GOOGLE_SCRIPT_PORT = 443;
WiFiClient esp_client;
HTTPSRedirect *client = nullptr;

void setup() {
  Serial.begin(BAUD_RATE);

  setupP1Serial();
  setupWiFi();
  setupGoogleClient();
}

void setupWiFi() {
  Serial.println("Connecting to WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Failed to connect to WiFi. Please check your credentials.");
    return;
  }

  Serial.println("\nWiFi connected");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

void setupP1Serial() {
  Serial.begin(BAUD_RATE, SERIAL_8N1, SERIAL_FULL);
  Serial.setRxBufferSize(1024);
  Serial.println("");
  Serial.println("Swapping UART0 RX to inverted");
  Serial.flush();
  USC0(UART0) = USC0(UART0) | BIT(UCRXI);
  Serial.println("Serial port is ready to recieve.");
}

void setupGoogleClient() {

  if (client != nullptr) {
    delete client;
    client = nullptr;
  }

  client = new HTTPSRedirect(GOOGLE_SCRIPT_PORT);
  client->setInsecure();
  client->setPrintResponseBody(true);
  client->setContentTypeHeader("application/json");

  Serial.print("Connecting to ");
  Serial.println(GOOGLE_SCRIPT_HOST);

  bool isConnected = false;
  for (int attempt = 0; attempt < 5; ++attempt) {
    if (client->connect(GOOGLE_SCRIPT_HOST, GOOGLE_SCRIPT_PORT)) {
      isConnected = true;
      Serial.println("Connected to Google Sheets");
      break;  // Exit loop on successful connection
    } else {
      Serial.println("Connection failed. Retrying...");
      delay(1000);  // Delay between retries
    }
  }

  if (!isConnected) {
    Serial.print("Could not connect to server: ");
    Serial.println(GOOGLE_SCRIPT_HOST);
  }
}



void loop() {
  long now = millis();
  if (now - last_update_sent > UPDATE_INTERVAL) {
    read_p1_hardwareserial();
  }
}

void send_data_to_google() {
  String jsonMessage = "{";
  jsonMessage += "\"consumption_low_tarif\":" + String(consumption_low_tarif) + ", ";
  jsonMessage += "\"consumption_high_tarif\":" + String(consumption_high_tarif) + ", ";
  jsonMessage += "\"returndelivery_low_tarif\":" + String(returndelivery_low_tarif) + ", ";
  jsonMessage += "\"returndelivery_high_tarif\":" + String(returndelivery_high_tarif) + ", ";
  jsonMessage += "\"actual_consumption\":" + String(actual_consumption) + ", ";
  jsonMessage += "\"actual_returndelivery\":" + String(actual_returndelivery) + ", ";
  jsonMessage += "\"l1_instant_power_usage\":" + String(l1_instant_power_usage) + ", ";
  jsonMessage += "\"l2_instant_power_usage\":" + String(l2_instant_power_usage) + ", ";
  jsonMessage += "\"l3_instant_power_usage\":" + String(l3_instant_power_usage) + ", ";
  jsonMessage += "\"l1_instant_power_current\":" + String(l1_instant_power_current) + ", ";
  jsonMessage += "\"l2_instant_power_current\":" + String(l2_instant_power_current) + ", ";
  jsonMessage += "\"l3_instant_power_current\":" + String(l3_instant_power_current) + ", ";
  jsonMessage += "\"l1_voltage\":" + String(l1_voltage) + ", ";
  jsonMessage += "\"l2_voltage\":" + String(l2_voltage) + ", ";
  jsonMessage += "\"l3_voltage\":" + String(l3_voltage) + ", ";
  jsonMessage += "\"gas_meter_m3\":" + String(gas_meter_m3) + ", ";
  jsonMessage += "\"actual_tarif_group\":" + String(actual_tarif) + ", ";
  jsonMessage += "\"short_power_outages\":" + String(short_power_outages) + ", ";
  jsonMessage += "\"long_power_outages\":" + String(long_power_outages) + ", ";
  jsonMessage += "\"short_power_drops\":" + String(short_power_drops) + ", ";
  jsonMessage += "\"short_power_peaks\":" + String(short_power_peaks);
  jsonMessage += "}";

  Serial.println(jsonMessage);

  String payload = jsonMessage;

  Serial.println("Publishing data to Google ...");
  Serial.println(payload);
  String url = String("/macros/s/") + GOOGLE_SCRIPT_ID + "/exec";
  setupGoogleClient();
  if (client->POST(url, GOOGLE_SCRIPT_HOST, payload)) {
    // Publish successful
  } else {
    Serial.println("Error while requesting :" + url);
    
  }

  delay(5000);
}

// P1 parsing code based on https://github.com/daniel-jong/esp8266_p1meter

unsigned int CRC16(unsigned int crc, unsigned char *buf, int len) {
  for (int pos = 0; pos < len; pos++) {
    crc ^= (unsigned int)buf[pos];
    for (int i = 8; i != 0; i--) {
      if ((crc & 0x0001) != 0) {
        crc >>= 1;
        crc ^= 0xA001;
      } else {
        crc >>= 1;
      }
    }
  }
  return crc;
}

bool isNumber(char *res, int len) {
  for (int i = 0; i < len; i++) {
    if (((res[i] < '0') || (res[i] > '9')) && (res[i] != '.' && res[i] != 0))
      return false;
  }
  return true;
}

int findCharInArrayRev(char array[], char c, int len) {
  for (int i = len - 1; i >= 0; i--) {
    if (array[i] == c)
      return i;
  }
  return -1;
}

long getValue(char *buffer, int maxlen, char startchar, char endchar) {
  int s = findCharInArrayRev(buffer, startchar, maxlen - 2);
  int l = findCharInArrayRev(buffer, endchar, maxlen - 2) - s - 1;

  char res[16];
  memset(res, 0, sizeof(res));

  if (strncpy(res, buffer + s + 1, l)) {
    if (endchar == '*') {
      if (isNumber(res, l))
        return (1000 * atof(res));
    } else if (endchar == ')') {
      if (isNumber(res, l))
        return atof(res);
    }
  }
  return 0;
}

bool decode_telegram(int len) {
  int start_char = findCharInArrayRev(telegram, '/', len);
  int end_char = findCharInArrayRev(telegram, '!', len);
  bool valid_crc_found = false;

  for (int cnt = 0; cnt < len; cnt++) {
    Serial.print(telegram[cnt]);
  }
  Serial.print("\n");

  if (start_char >= 0) {
    current_crc = CRC16(0x0000, (unsigned char *)telegram + start_char, len - start_char);
  } else if (end_char >= 0) {
    current_crc = CRC16(current_crc, (unsigned char *)telegram + end_char, 1);
    char message_crc[5];
    strncpy(message_crc, telegram + end_char + 1, 4);
    message_crc[4] = 0;
    valid_crc_found = (strtol(message_crc, NULL, 16) == current_crc);
    if (valid_crc_found)
      Serial.println("CRC Valid!");
    else
      Serial.println("CRC Invalid!");
    current_crc = 0;
  } else {
    current_crc = CRC16(current_crc, (unsigned char *)telegram, len);
  }

  if (strncmp(telegram, "1-0:1.8.1", strlen("1-0:1.8.1")) == 0) {
    consumption_low_tarif = getValue(telegram, len, '(', '*');
  }

  if (strncmp(telegram, "1-0:1.8.2", strlen("1-0:1.8.2")) == 0) {
    consumption_high_tarif = getValue(telegram, len, '(', '*');
  }

  if (strncmp(telegram, "1-0:2.8.1", strlen("1-0:2.8.1")) == 0) {
    returndelivery_low_tarif = getValue(telegram, len, '(', '*');
  }

  if (strncmp(telegram, "1-0:2.8.2", strlen("1-0:2.8.2")) == 0) {
    returndelivery_high_tarif = getValue(telegram, len, '(', '*');
  }

  if (strncmp(telegram, "1-0:1.7.0", strlen("1-0:1.7.0")) == 0) {
    actual_consumption = getValue(telegram, len, '(', '*');
  }

  if (strncmp(telegram, "1-0:2.7.0", strlen("1-0:2.7.0")) == 0) {
    actual_returndelivery = getValue(telegram, len, '(', '*');
  }

  if (strncmp(telegram, "1-0:21.7.0", strlen("1-0:21.7.0")) == 0) {
    l1_instant_power_usage = getValue(telegram, len, '(', '*');
  }

  if (strncmp(telegram, "1-0:41.7.0", strlen("1-0:41.7.0")) == 0) {
    l2_instant_power_usage = getValue(telegram, len, '(', '*');
  }

  if (strncmp(telegram, "1-0:61.7.0", strlen("1-0:61.7.0")) == 0) {
    l3_instant_power_usage = getValue(telegram, len, '(', '*');
  }

  if (strncmp(telegram, "1-0:31.7.0", strlen("1-0:31.7.0")) == 0) {
    l1_instant_power_current = getValue(telegram, len, '(', '*');
  }

  if (strncmp(telegram, "1-0:51.7.0", strlen("1-0:51.7.0")) == 0) {
    l2_instant_power_current = getValue(telegram, len, '(', '*');
  }

  if (strncmp(telegram, "1-0:71.7.0", strlen("1-0:71.7.0")) == 0) {
    l3_instant_power_current = getValue(telegram, len, '(', '*');
  }

  if (strncmp(telegram, "1-0:32.7.0", strlen("1-0:32.7.0")) == 0) {
    l1_voltage = getValue(telegram, len, '(', '*');
  }

  if (strncmp(telegram, "1-0:52.7.0", strlen("1-0:52.7.0")) == 0) {
    l2_voltage = getValue(telegram, len, '(', '*');
  }

  if (strncmp(telegram, "1-0:72.7.0", strlen("1-0:72.7.0")) == 0) {
    l3_voltage = getValue(telegram, len, '(', '*');
  }

  if (strncmp(telegram, "0-1:24.2.1", strlen("0-1:24.2.1")) == 0) {
    gas_meter_m3 = getValue(telegram, len, '(', '*');
  }

  if (strncmp(telegram, "0-0:96.14.0", strlen("0-0:96.14.0")) == 0) {
    actual_tarif = getValue(telegram, len, '(', ')');
  }

  if (strncmp(telegram, "0-0:96.7.21", strlen("0-0:96.7.21")) == 0) {
    short_power_outages = getValue(telegram, len, '(', ')');
  }

  if (strncmp(telegram, "0-0:96.7.9", strlen("0-0:96.7.9")) == 0) {
    long_power_outages = getValue(telegram, len, '(', ')');
  }

  if (strncmp(telegram, "1-0:32.32.0", strlen("1-0:32.32.0")) == 0) {
    short_power_drops = getValue(telegram, len, '(', ')');
  }

  if (strncmp(telegram, "1-0:32.36.0", strlen("1-0:32.36.0")) == 0) {
    short_power_peaks = getValue(telegram, len, '(', ')');
  }

  return valid_crc_found;
}

void read_p1_hardwareserial() {
  if (Serial.available()) {
    memset(telegram, 0, sizeof(telegram));

    while (Serial.available()) {
      ESP.wdtDisable();
      int len = Serial.readBytesUntil('\n', telegram, P1_MAXLINELENGTH);
      ESP.wdtEnable(1);

      processLine(len);
    }
  }
}

void processLine(int len) {
  telegram[len] = '\n';
  telegram[len + 1] = 0;
  yield();

  bool result = decode_telegram(len + 1);
  if (result) {
    send_data_to_google();
    last_update_sent = millis();
  }
}
