#include <WiFi.h>
#include <FS.h>
#include <FFat.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

const int analogPin = 0;               // GPIO 0
const int sdaPin = 8;                  // A0
const int sclPin = 9;

// for 128x64 displays:
Adafruit_SSD1306 display(124, 64, &Wire, -1);

// Set web server port number to 80
WiFiServer server(80);

// Stores the files info during the saving process
struct RecordingSlot {
  String filePath = "";
  unsigned long time = 0;
  unsigned long delay = 0;
  bool recording = false;
};

struct Calibration {
  float lc = 0;  //linear coefficient
  float ac = 1;  //angular coefficient
};

RecordingSlot slot;
Calibration cal;

String ip;
const String SSID = "Choose a SSID";
const String calPath = "/cal/";
const String recordsPath = "/records/";
const String pagesPath = "/pages/";

const long updateDisplayDelay = 2000;
long displayTimer = 0;
bool connected = false;

//percent of free storage
unsigned long storage = 0;

void setCalibration() {
  String path;
  path = calPath + "lc.txt";
  if (FFat.exists(path)) {
    File file = FFat.open(path);
    float lc = file.readString().toFloat();
    file.close();
    if (lc > 0) cal.lc = lc;
  }
  path = calPath + "ac.txt";
  if (FFat.exists(path)) {
    File file = FFat.open(path);
    float ac = file.readString().toFloat();
    file.close();
    if (ac > 0) cal.ac = ac;
  }
}

void updateStorageInfo() {
  storage = (unsigned long)100 * FFat.usedBytes() / FFat.totalBytes();
}

void checkForPrintDisplay() {
  long time = millis() - displayTimer;
  if (time >= updateDisplayDelay) {
    displayTimer = millis();
    printInDisplay();
  }
}

void printInDisplay() {void printInDisplay() {
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
  //printing SSID
  display.setCursor(0, 1);
  display.print(SSID);
  //printing ip
  display.setCursor(0, 9);
  display.print(ip);
  if (slot.recording) {
    //printing if recording
    display.setCursor(75, 9);
    display.print("R");
  }
  if (connected) {
    //printing if connected
    display.setCursor(85, 9);
    display.print("C");
  }
  //print storage percentage
  display.setCursor(100, 9);
  display.print(String(storage) + "%");
  //printing potential
  display.setTextSize(2);
  display.setCursor(0, 18);
  int v = getPotentialInMilliVolts();
  display.print(String(v) + " mV");
  display.display();
}


void hasClientConnectedToWiFi() {
  if (WiFi.softAPgetStationNum() > 0) {
    digitalWrite(connectionIndicatorPin, HIGH);
    connected = true;
    return;
  }
  digitalWrite(connectionIndicatorPin, LOW);
  connected = false;
}

int getPotentialInMilliVolts() {
  int sum = 0;
  int n = 50;
  // gets the average value for 50 replica
  for (int i = 0; i < n; i++) {
    sum += analogRead(analogPin);
    delay(1);
  }
  int vadc = (int)(sum / n) * 3300 / 4096;
  //uses calibration stored in lc and ac txt files
  int vm = (int)(vadc - cal.lc) / cal.ac;
  return vm;
}

void recording() {
  String path = slot.filePath;
  int v = getPotentialInMilliVolts();
  String content = String(slot.time) + ";" + String(v) + "\n";
  File file = (!FFat.exists(path)) ? FFat.open(path, "a", true) : FFat.open(path, "a");
  file.println(content);
  file.close();
}

void checkForRecording() {
  long time = millis() - slot.time;
  if (slot.recording && time >= slot.delay) {
    slot.time += time;
    recording();
  }
}

String extractFromString(String str, String startChars, String endChars) {
  int i = str.indexOf(startChars);
  int j = str.indexOf(endChars);
  String result = str.substring(i + startChars.length(), j);
  return result;
}

// for basic responses
void okResponse(WiFiClient client) {
  String content = "{\"response\" : \"OK\" }";
  String responseHeaders = "HTTP/1.1 200 OK\n";
  responseHeaders += "Access-Control-Allow-Origin: *\n";
  responseHeaders += "Connection: Keep-Alive\n";
  responseHeaders += "Keep-Alive: timeout=10, max=200\n";
  responseHeaders += "Content-type: application/json \n";
  responseHeaders += "Content-Length:" + String(content.length()) + "\n";
  responseHeaders += "\n";
  Serial.println(responseHeaders + "\n");
  client.print(responseHeaders);
  client.print(content);
}

// for files
void okResponse(WiFiClient client, String filePath, String contentType) {
  File file = FFat.open(filePath, "r");
  String responseHeaders = "HTTP/1.1 200 OK\n";
  responseHeaders += "Access-Control-Allow-Origin: *\n";
  responseHeaders += "Connection: Keep-Alive\n";
  responseHeaders += "Keep-Alive: timeout=10, max=200\n";
  responseHeaders += "Content-type: " + contentType + " \n";
  responseHeaders += "Content-Length:" + String(file.size()) + "\n";
  responseHeaders += "\n";
  Serial.println(responseHeaders);
  client.print(responseHeaders);
  long count = 0;
  while (count < file.size()) {
    char c = file.read();
    client.print(c);
    count++;
  }
  file.close();
}

// for JSON
void okResponse(WiFiClient client, String content) {
  String responseHeaders = "HTTP/1.1 200 OK\n";
  responseHeaders += "Access-Control-Allow-Origin: *\n";
  responseHeaders += "Connection: Keep-Alive\n";
  responseHeaders += "Keep-Alive: timeout=10, max=200\n";
  responseHeaders += "Content-type: application/json \n";
  responseHeaders += "Content-Length:" + String(content.length()) + "\n";
  responseHeaders += "\n";
  Serial.println(responseHeaders + "\n");
  client.print(responseHeaders);
  client.print(content);
}

void badRequest(WiFiClient client) {
  String content = "{\"response\" : \"Bad Request\"}";
  String responseHeaders = "HTTP/1.1 400 Bad Request \n";
  responseHeaders += "Access-Control-Allow-Origin: *\n";
  responseHeaders += "Content-type: application/json \n";
  responseHeaders += "Content-Length:" + String(content.length()) + "\n";
  responseHeaders += "\n";
  Serial.println(responseHeaders + "\n");
  client.print(responseHeaders + content);
}

void forbidden(WiFiClient client) {
  String content = "{\"response\" : \" Forbidden\"}";
  String responseHeaders = "HTTP/1.1 403 Forbidden \n";
  responseHeaders += "Access-Control-Allow-Origin: *\n";
  responseHeaders += "Content-type: application/json \n";
  responseHeaders += "Content-Length:" + String(content.length()) + "\n";
  responseHeaders += "\n";
  Serial.println(responseHeaders + "\n");
  client.print(responseHeaders + content);
}

void notFoundResponse(WiFiClient client) {
  String content = "{\"response\" : \"Not Found\"}";
  String responseHeaders = "HTTP/1.1 404 Not Found \n";
  responseHeaders += "Access-Control-Allow-Origin: *\n";
  responseHeaders += "Content-type: application/json \n";
  responseHeaders += "Content-Length:" + String(content.length()) + "\n";
  responseHeaders += "\n";
  Serial.println(responseHeaders + "\n");
  client.print(responseHeaders + content);
}

void preFlightResponse(WiFiClient client) {
  String responseHeaders = "HTTP/1.1 204 No Content\n";
  responseHeaders += "Connection: Keep-Alive\n";
  responseHeaders += "Keep-Alive: timeout=10, max=200\n";
  responseHeaders += "Access-Control-Allow-Origin: *\n";
  responseHeaders += "Access-Control-Allow-Methods: POST, GET\n";
  responseHeaders += "Access-Control-Allow-Headers: access-control-allow-origin,content-disposition,content-type\n";
  responseHeaders += "\n";
  Serial.println(responseHeaders + "\n");
  client.print(responseHeaders);
}

long uploadFile(WiFiClient client, String fileName) {
  String path = pagesPath + fileName;
  long bytes = 0;
  if (FFat.exists(path)) FFat.remove(path);
  File file = FFat.open(path, "w", true);
  while (client.available()) {
    char c = client.read();
    file.print(c);
    bytes++;
    //Serial.print(c);
    //if (!client.available())
    //break;
  }
  file.close();
  return bytes;
}

void handleClient(WiFiClient client, String requestHeaders) {
  //preflight request
  if (requestHeaders.indexOf("OPTIONS /upload/") != -1) {
    preFlightResponse(client);
    return;
  }
  // upload pages
  if (requestHeaders.indexOf("POST /upload/") != -1) {
    if (requestHeaders.indexOf("text/html") == -1 || requestHeaders.indexOf("filename=") == -1) {
      badRequest(client);
      return;
    }
    String name = extractFromString(requestHeaders, "/upload/", " HTTP");
    long bytes = uploadFile(client, name);
    String content = "{\"bytes\" : " + String(bytes) + "}";
    okResponse(client, content);
    return;
  }
  // delivery html index page
  if (requestHeaders.indexOf("GET / ") != -1) {
    String path = pagesPath + "index.html";
    if (FFat.exists(path)) {
      okResponse(client, path, "text/html");
      return;
    }
  }
  // devilery the others page
  if (requestHeaders.indexOf("GET /pages/") != -1) {
    Serial.println("pages");
    String name = extractFromString(requestHeaders, "/pages/", " HTTP");
    String path = pagesPath + name;
    Serial.println(path);
    if (FFat.exists(path)) {
      okResponse(client, path, "text/html");
      return;
    }
  }
  // list all pages
  if (requestHeaders.indexOf("GET /list/pages ") != -1) {
    String content = "[";
    File root = FFat.open(pagesPath, "r");
    if (root && root.isDirectory()) {
      File file = root.openNextFile();
      if (file) {
        content += "\"";
        content += String(file.name());
        content += "\"";
        file = root.openNextFile();
        while (file) {
          content += ", \"";
          content += String(file.name());
          content += "\"";
          file = root.openNextFile();
        }
      }
    }
    content += "]";
    okResponse(client, content);
    return;
  }
  // clear all pages
  if (requestHeaders.indexOf("GET /clear/pages ") != -1) {
    File root = FFat.open(pagesPath, "r");
    if (root && root.isDirectory()) {
      File file = root.openNextFile();
      while (file) {
        String name = String(file.name());
        String path = pagesPath + name;
        FFat.remove(path);
        file = root.openNextFile();
      }
    }
    okResponse(client);
    return;
  }
  // list all record
  if (requestHeaders.indexOf("GET /records ") != -1) {
    String content = "[";
    File root = FFat.open(recordsPath, "r");
    if (root && root.isDirectory()) {
      File file = root.openNextFile();
      if (file) {
        content += "\"";
        content += String(file.name());
        content += "\"";
        file = root.openNextFile();
        while (file) {
          content += ", \"";
          content += String(file.name());
          content += "\"";
          file = root.openNextFile();
        }
      }
    }
    content += "]";
    okResponse(client, content);
    return;
  }
  // get a record (txt file)
  if (requestHeaders.indexOf("GET /records/get?name=") != -1) {
    String name = extractFromString(requestHeaders, "?name=", " HTTP");
    String path = recordsPath + name;
    if (FFat.exists(path)) {
      okResponse(client, path, "text/plain");
      return;
    }
  }
  // delete a record
  if (requestHeaders.indexOf("GET /records/del?name=") != -1) {
    String name = extractFromString(requestHeaders, "?name=", " HTTP");
    String path = recordsPath + name;
    if (path.equals(slot.filePath)) {
      forbidden(client);
      return;
    }
    if (FFat.exists(path)) {
      FFat.remove(path);
      okResponse(client);
      return;
    }
  }
  // start recording
  if (requestHeaders.indexOf("GET /start?name=") != -1 && requestHeaders.indexOf("&time=") != -1) {
    String name = extractFromString(requestHeaders, "?name=", "&");
    int delay = extractFromString(requestHeaders, "&time=", " HTTP").toInt();
    if (slot.recording) {
      forbidden(client);
      return;
    }
    if (name.isEmpty() || name.length() > 20 || delay < 5000) {
      badRequest(client);
      return;
    }
    String path = recordsPath + name;
    if (FFat.exists(path)) {
      forbidden(client);
      return;
    }
    slot.filePath = path;
    slot.delay = delay;
    slot.recording = true;
    recording();
    okResponse(client);
    return;
  }
  // stop recording
  if (requestHeaders.indexOf("GET /stop") != -1) {
    if (!slot.recording) {
      badRequest(client);
      return;
    }
    slot.filePath = "";
    slot.time = 0;
    slot.delay = 0;
    slot.recording = false;
    okResponse(client);
    return;
  }
  //get readings
  if (requestHeaders.indexOf("GET /readings") != -1) {
    String content = "{\"storage\" : " + String(storage);
    content += ", \"recording\" : " + String((slot.recording) ? "true" : "false");
    content += ", \"millivolts\" : " + String(getPotentialInMilliVolts());
    content += "}";
    okResponse(client, content);
    return;
  }
  //get calibration coefficients
  if (requestHeaders.indexOf("GET /cal/get") != -1) {
    String content = "{\"lc\" : " + String(cal.lc);
    content += ", \"ac\" : " + String(cal.ac);
    content += "}";
    okResponse(client, content);
    return;
  }
  //reset calibration
  if (requestHeaders.indexOf("GET /cal/reset") != -1) {
    String path;
    path = calPath + "ac.txt";
    if (FFat.exists(path)) FFat.remove(path);
    path = calPath + "lc.txt";
    if (FFat.exists(path)) FFat.remove(path);
    setCalibration();
    okResponse(client);
    return;
  }
  //set calibration coefficients
  if (requestHeaders.indexOf("GET /cal/set?ac=") != -1 && requestHeaders.indexOf("&lc=") != -1) {
    float ac = extractFromString(requestHeaders, "?ac=", "&").toFloat();
    float lc = extractFromString(requestHeaders, "&lc=", " HTTP").toFloat();
    //lc can be zero, but ac can`t
    if(ac == 0) {
      badRequest(client);
      return;
    }
    String path;
    File file;
    path = calPath + "lc.txt";
    if(FFat.exists(path)) file = FFat.open(path, "w");
    else file = FFat.open(path, "w", true);
    file.println(lc);
    file.close();
    path = calPath + "ac.txt";
    if(FFat.exists(path)) file = FFat.open(path, "w");
    else file = FFat.open(path, "w", true);
    file.println(ac);
    file.close();
    setCalibration();
    okResponse(client);
    return;
  }
  notFoundResponse(client);
}

void setup() {
  hasClientConnectedToWiFi();
  Serial.begin(115200);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("Display initialization failed");
    while (true)
      ;
  }
  //init display timer
  displayTimer = millis();
  display.clearDisplay();
  display.display();
  if (!FFat.begin(true)) {
    Serial.println("FFat Mount Failed");
    while (true)
      ;
  }
  WiFi.setSleep(false);
  if (!WiFi.softAP(SSID, "0123456789", 1, 0, 1)) {
    Serial.println("Webserver failed starting...");
    return;
  }
  ip = WiFi.softAPIP().toString();
  Serial.print("Soft-AP IP address = ");
  Serial.println(ip);  // standard ip = 192.168.4.1
  Serial.println("Webserver started...");
  server.begin();
  setCalibration();
}

void loop() {
  hasClientConnectedToWiFi();
  WiFiClient client = server.available();
  if (client) {
    String headers = "";
    Serial.println("new client connected...");
    // an http request header ends with a blank line
    bool currentLineIsBlank = true;
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        headers += c;
        Serial.print(c);
        // if you've gotten to the end of the line (received a newline
        // character) and the line is blank, the http request header has ended,
        // so you can send a reply
        if (c == '\n' && currentLineIsBlank) {
          handleClient(client, headers);
          break;
        }
        if (c == '\n') {
          currentLineIsBlank = true;
        } else if (c != '\r') {
          currentLineIsBlank = false;
        }
      }
    }
    // give time to the browser
    delay(500);
    // close the connection:
    client.stop();
    Serial.println("client disconnected...");
  }
  updateStorageInfo();
  checkForPrintDisplay();
  checkForRecording();
  delay(100);
}
