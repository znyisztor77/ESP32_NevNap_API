#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <time.h>

// Wi-Fi beállítások
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// OLED beállítások
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// NTP beállítások (Magyarország: UTC+1, nyári időszámítás UTC+2)
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600;   // UTC+1
const int daylightOffset_sec = 3600;  // Nyári időszámítás (+1 óra)

// HTTPS támogatás
WiFiClientSecure client;

// Idő változók
int lastCheckedDay = -1;  // Elmenti az utolsó lekért napot

void setup() {
    Serial.begin(115200);

    // Wi-Fi csatlakozás
    WiFi.begin(ssid, password);
    Serial.print("WiFi csatlakozás...");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi csatlakozva!");

    // NTP idő beállítása
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

    // OLED inicializálása
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println("OLED inicializálás sikertelen!");
        while (1);
    }
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 10);
    display.println("Nevnap keresese...");
    display.display();

    // HTTPS kliens inicializálása (tanúsítványellenőrzés kikapcsolása)
    client.setInsecure();
}

void loop() {
    // Aktuális dátum lekérdezése
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        Serial.println("NTP idő lekérése sikertelen!");
        return;
    }

    int month = timeinfo.tm_mon + 1;  // Hónap (0-alapú, ezért +1)
    int day = timeinfo.tm_mday;       // Nap

    // Ha a nap nem változott, ne frissítsen feleslegesen
    if (lastCheckedDay == day) {
        delay(10000);  // 10 másodperces várakozás (hogy ne pörögjön)
        return;
    }
    lastCheckedDay = day;

    // URL összeállítása
    String url = "https://nevnap.xsak.hu/json.php?honap=" + String(month) + "&nap=" + String(day);
    Serial.println("Lekérdezett URL: " + url);

    // Névnap lekérése HTTPS-en keresztül
    HTTPClient https;
    https.begin(client, url);
    int httpCode = https.GET();

    if (httpCode == HTTP_CODE_OK) {
        String payload = https.getString();
        Serial.println("Kapott JSON: " + payload);
        
        // JSON feldolgozás
        DynamicJsonDocument doc(1024);
        deserializeJson(doc, payload);

        // Elsődleges névnap (nev1)
        JsonArray nev1 = doc["nev1"];
        //JsonArray nev2 = doc["nev2"];

        // OLED frissítése
        display.clearDisplay();
        display.setCursor(0, 10);
        display.println("Mai nevnapok:");

        // Elsődleges név kiírása
        for (const char* nev : nev1) {
            display.println(nev);
        }

        // További névnapok kiírása (max. 5 név, hogy elférjen a kijelzőn)
        /*int count = 0;
        for (const char* nev : nev2) {
            display.println(nev);
            count++;
            if (count >= 5) break; // Ne írjon ki túl sok nevet
        }*/

        display.display();
    } else {
        Serial.println("HTTP hiba: " + String(httpCode));
    }

    https.end();

    // Naponta egyszer frissít (24 óra várakozás)
    delay(86400000);
}
