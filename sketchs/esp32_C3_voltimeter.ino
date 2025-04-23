#include <WiFi.h>
#include <FS.h>
#include <FFat.h>

// Set web server port number to 80
WiFiServer server(80);

void setup() {
    Serial.begin(115200);
    WiFi.setSleepMode(WIFI_NONE_SLEEP);
    if (!WiFi.softAP("Voltimeter", "0123456789", 1, 0, 1))
    {
        Serial.println("Webserver failed starting...");
        return;
    }
    Serial.print("Soft-AP IP address = ");
    Serial.println(WiFi.softAPIP()); // standard ip = 192.168.4.1
    Serial.println("Webserver started...");
    server.begin();
}

void loop()
{
    WiFiClient client = server.available();
    if (client)
    {
        String headers = "";
        Serial.println("new client connected...");
        // an http request header ends with a blank line
        bool currentLineIsBlank = true;
        while (client.connected())
        {
            if (client.available())
            {
                char c = client.read();
                headers += c;
                Serial.print(c);
                // if you've gotten to the end of the line (received a newline
                // character) and the line is blank, the http request header has ended,
                // so you can send a reply
                if (c == '\n' && currentLineIsBlank)
                {
                    handleClient(client, headers);
                    break;
                }
                if (c == '\n')
                {
                    currentLineIsBlank = true;
                }
                else if (c != '\r')
                {
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
}
