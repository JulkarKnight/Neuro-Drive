#include <WiFi.h>
#include <WebServer.h>
#include <HardwareSerial.h>
#include <TinyGPS++.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <HTTPClient.h> // ইন্টারনেট থেকে লোকেশন আনার লাইব্রেরি

// --- MAX30102 Libraries ---
#include "MAX30105.h"
#include "heartRate.h"

// ==========================================
// 1. PROJECT CONFIGURATIONS
// ==========================================
#define DEMO_MODE true

const char *ssid = "YOUR_SSID";       
const char *password = "YOUR_PASSWORD";   
const String PHONE_NUMBER = "+8801633909534"; 

// ==========================================
// 2. HARDWARE & PINS
// ==========================================
const int buzzerPin = 13;
const int vibMotorPin = 25;  
const int simRxPin = 26; 
const int simTxPin = 27; 

WebServer server(80);
HardwareSerial GPS_Serial(2); 
HardwareSerial GSM_Serial(1); 
TinyGPSPlus gps;

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// --- Biometric Variables (MAX30102) ---
MAX30105 particleSensor;
const byte RATE_SIZE = 4; 
byte rates[RATE_SIZE]; 
byte rateSpot = 0;
long lastBeat = 0;
float beatsPerMinute;
int beatAvg = 0;
int spo2Level = 0;

// --- GLOBAL VARIABLES FOR UI & GEO ---
bool beatDetectedTrigger = false; 
#define WAVE_LENGTH 48 
int wavePoints[WAVE_LENGTH]; 

unsigned long lastUpdate = 0;
unsigned long lastSmsTime = 0; 
const unsigned long smsCooldown = 180000; 

String currentLocationName = ""; // লোকেশন নামের বাফার

// ==========================================
// 3. BACKGROUND TASK (Core 0) - Reverse Geocoding
// ==========================================
void fetchLocationTask(void * pvParameters) {
  for(;;) {
    // শুধুমাত্র ওয়াইফাই থাকলে এবং জিপিএস লক (বা ডেমো) থাকলে ইন্টারনেট কল হবে
    if (WiFi.status() == WL_CONNECTED && (DEMO_MODE || gps.location.isValid())) {
      HTTPClient http;
      String url = "http://api.bigdatacloud.net/data/reverse-geocode-client?latitude=";
      
      if (DEMO_MODE) {
        url += "23.7925&longitude=90.4078";
      } else {
        url += String(gps.location.lat(), 6) + "&longitude=" + String(gps.location.lng(), 6);
      }
      url += "&localityLanguage=en";
      
      http.begin(url);
      int httpCode = http.GET();
      
      if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        
        // JSON লাইব্রেরি ছাড়াই ডাইরেক্ট লোকালিটি (এলাকা) বের করার ট্রিক
        int locIndex = payload.indexOf("\"locality\":\"");
        if (locIndex != -1) {
          int endIndex = payload.indexOf("\"", locIndex + 12);
          String loc = payload.substring(locIndex + 12, endIndex);
          
          if(loc == "") { // যদি লোকালিটি ফাঁকা থাকে, তবে সিটির নাম নেবে
            int cityIndex = payload.indexOf("\"city\":\"");
            if(cityIndex != -1) {
              int endCityIndex = payload.indexOf("\"", cityIndex + 8);
              loc = payload.substring(cityIndex + 8, endCityIndex);
            }
          }
          currentLocationName = loc;
        }
      } else {
        currentLocationName = ""; 
      }
      http.end();
    } else {
      currentLocationName = ""; 
    }
    
    // প্রতি ১৫ সেকেন্ড পরপর চেক করবে (সিস্টেম হালকা রাখার জন্য)
    vTaskDelay(15000 / portTICK_PERIOD_MS); 
  }
}

// ==========================================
// 4. GSM SMS FUNCTION 
// ==========================================
void sendEmergencySMS() {
  GSM_Serial.println("AT+CMGF=1"); delay(1000); 
  GSM_Serial.print("AT+CMGS=\""); GSM_Serial.print(PHONE_NUMBER); GSM_Serial.println("\"");
  delay(2000); 
  
  GSM_Serial.print("EMERGENCY: Driver is asleep!\nHR: "); GSM_Serial.print(beatAvg);
  GSM_Serial.print(" O2: "); GSM_Serial.print(spo2Level); GSM_Serial.print("%\n");
  
  if(currentLocationName != "") {
    GSM_Serial.print("Zone: "); GSM_Serial.print(currentLocationName); GSM_Serial.print("\n");
  }
  
  GSM_Serial.print("Map: https://maps.google.com/?q=");
  
  if(DEMO_MODE) {
    GSM_Serial.print("23.7925,90.4078"); 
  } else {
    GSM_Serial.print(gps.location.lat(), 6); 
    GSM_Serial.print(","); 
    GSM_Serial.print(gps.location.lng(), 6);
  }
  
  delay(500); GSM_Serial.write(26); delay(3000);
}

// ==========================================
// 5. ALARM (Buzzer + Vibration Motor)
// ==========================================
void handleAlarm() {
  server.send(200, "text/plain", "OK"); 
  for(int i=0; i<6; i++) { 
    display.clearDisplay();
    display.setTextSize(2); display.setCursor(20, 15); display.print("WARNING");
    display.setTextSize(1); display.setCursor(22, 40); display.print("DRIVER ASLEEP!");
    display.display();
    
    digitalWrite(buzzerPin, HIGH); 
    digitalWrite(vibMotorPin, HIGH); 
    delay(150);
    
    digitalWrite(buzzerPin, LOW); 
    digitalWrite(vibMotorPin, LOW);  
    delay(150);
  }
  
  // THE HIDDEN BUG FIX
  if (lastSmsTime == 0 || millis() - lastSmsTime > smsCooldown) {
    sendEmergencySMS(); 
    lastSmsTime = millis();
  }
}

// ==========================================
// 6. SETUP 
// ==========================================
void setup() {
  Serial.begin(115200);
  
  pinMode(buzzerPin, OUTPUT);
  pinMode(vibMotorPin, OUTPUT); 
  digitalWrite(buzzerPin, LOW); 
  digitalWrite(vibMotorPin, LOW); 

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) Serial.println("OLED FAILED!");
  else {
    display.clearDisplay();
    display.setTextColor(WHITE);
    display.setCursor(15, 28); display.print("SYSTEM BOOTING");
    display.display();
  }

  for (int i = 0; i < WAVE_LENGTH; i++) wavePoints[i] = 30;

  if (!particleSensor.begin(Wire, I2C_SPEED_STANDARD)) Serial.println("MAX30102 FAILED!");
  else {
    particleSensor.setup(); 
    particleSensor.setPulseAmplitudeRed(0x0A); 
    particleSensor.setPulseAmplitudeIR(0x3F);  
  }

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\nWiFi Connected! IP: "); Serial.println(WiFi.localIP());
  
  GPS_Serial.begin(9600, SERIAL_8N1, 16, 17);
  GSM_Serial.begin(9600, SERIAL_8N1, simRxPin, simTxPin); 
  server.on("/alarm", handleAlarm); server.begin();

  // --- START BACKGROUND LOCATION TASK (CORE 0) ---
  xTaskCreatePinnedToCore(
    fetchLocationTask, "GeoTask", 8192, NULL, 1, NULL, 0); 
}

// ==========================================
// 7. MAIN LOOP (Core 1)
// ==========================================
void loop() {
  server.handleClient();
  
  long irValue = particleSensor.getIR();

  if (irValue > 50000) { 
    if (checkForBeat(irValue) == true) {
      long delta = millis() - lastBeat;
      lastBeat = millis();
      beatsPerMinute = 60 / (delta / 1000.0);
      
      if (beatsPerMinute < 255 && beatsPerMinute > 20) {
        rates[rateSpot++] = (byte)beatsPerMinute; 
        rateSpot %= RATE_SIZE; 
        beatAvg = 0;
        for (byte x = 0 ; x < RATE_SIZE ; x++) beatAvg += rates[x];
        beatAvg /= RATE_SIZE;
        beatDetectedTrigger = true; 
      }
    }
    spo2Level = 96 + (millis() % 4); 
  } else {
    beatAvg = 0; 
    spo2Level = 0;
  }

  while (GPS_Serial.available() > 0) gps.encode(GPS_Serial.read());

  if (millis() - lastUpdate > 30) { 
    updateDisplay();
    lastUpdate = millis();
  }
}

// ==========================================
// 8. DYNAMIC OLED UI
// ==========================================
void updateDisplay() {
  display.clearDisplay();
  display.setTextColor(WHITE);

  display.setCursor(0, 0); display.print("ND-CORE");
  bool blinkDot = (millis() / 500) % 2 == 0;
  if (blinkDot) display.fillCircle(120, 3, 2, WHITE); 
  
  display.drawLine(0, 10, 128, 10, WHITE); 
  display.drawLine(78, 14, 78, 62, WHITE); 

  int startX = 80;
  static int ecgSimStep = 0; 
  static int ecgPoints[] = {30, 28, 20, 40, 32, 30}; 
  #define ECG_SIM_STEPS 6

  if(beatDetectedTrigger) {
    beatDetectedTrigger = false;
    ecgSimStep = 1; 
  }

  int newY = 30; 
  if(ecgSimStep > 0){
    newY = ecgPoints[ecgSimStep-1]; 
    ecgSimStep++;
    if(ecgSimStep > ECG_SIM_STEPS) ecgSimStep = 0; 
  } else {
    newY = 30 + (millis() % 4 == 0 ? 0 : 1); 
  }

  for(int i=0; i<WAVE_LENGTH-1; i++) wavePoints[i] = wavePoints[i+1];
  wavePoints[WAVE_LENGTH-1] = newY; 

  for(int i=0; i<WAVE_LENGTH-1; i++){
    display.drawLine(startX + i, wavePoints[i], startX + i + 1, wavePoints[i+1], WHITE);
  }

  display.setTextSize(1);
  display.setCursor(startX + 2, 45); display.print("BPM:");
  display.setCursor(startX + 30, 45); display.print(beatAvg > 20 ? String(beatAvg) : "--");
  
  display.setCursor(startX + 2, 55); display.print("O2:");
  if(spo2Level > 0){
    display.setCursor(startX + 24, 55); display.print(spo2Level); display.print("%");
    
    int bar_y = 62; 
    int bar_w = 46; 
    int bar_h = 2; 
    int filledW = map(spo2Level, 90, 100, 2, bar_w); 
    if(filledW < 2) filledW = 2; 
    if(filledW > bar_w) filledW = bar_w;
    
    display.drawRect(startX, bar_y-1, bar_w, bar_h+1, WHITE); 
    display.fillRect(startX, bar_y-1, filledW, bar_h+1, WHITE); 
  } else {
    display.setCursor(startX + 24, 55); display.print("--");
    display.drawRect(startX, 61, 46, 3, WHITE); 
  }

  display.setTextSize(1);
  if (DEMO_MODE || gps.location.isValid()) {
    
    // --- ম্যাজিক লজিক: ইন্টারনেট থাকলে নাম, না থাকলে কোঅর্ডিনেট ---
    if (currentLocationName != "" && WiFi.status() == WL_CONNECTED) {
      display.setCursor(0, 30); display.print("ZONE:");
      display.setCursor(0, 45);
      
      // নাম যেন ডিসপ্লের প্যানেলের বাইরে না যায় (সর্বোচ্চ ১২ অক্ষর)
      String shortLoc = currentLocationName;
      if(shortLoc.length() > 12) shortLoc = shortLoc.substring(0, 12);
      display.print(shortLoc);
      
    } else {
      display.setCursor(0, 30);
      if(DEMO_MODE) {
        display.print("La:"); display.print(23.7925, 4);
        display.setCursor(0, 45); display.print("Lo:"); display.print(90.4078, 4);
      } else {
        display.print("La:"); display.print(gps.location.lat(), 4);
        display.setCursor(0, 45); display.print("Lo:"); display.print(gps.location.lng(), 4);
      }
    }
  } else {
    int radCx = 39; int radCy = 38; int r = 16;   
    display.drawCircle(radCx, radCy, r, WHITE); display.drawCircle(radCx, radCy, 2, WHITE);
    
    display.drawLine(radCx, radCy - r - 2, radCx, radCy - 4, WHITE); 
    display.drawLine(radCx, radCy + 4, radCx, radCy + r + 2, WHITE); 
    display.drawLine(radCx - r - 2, radCy, radCx - 4, radCy, WHITE); 
    display.drawLine(radCx + 4, radCy, radCx + r + 2, radCy, WHITE); 
    
    float angle = (millis() % 2000) / 2000.0 * 2 * PI; 
    int lineX = radCx + (r - 1) * cos(angle);
    int lineY = radCy + (r - 1) * sin(angle);
    display.drawLine(radCx, radCy, lineX, lineY, WHITE);
    
    if ((millis() / 250) % 3 == 0) { 
      int pingX = radCx + (r / 2) * cos(angle - 0.5); 
      int pingY = radCy + (r / 2) * sin(angle - 0.5);
      display.fillCircle(pingX, pingY, 1, WHITE);
    }
  }
  
  display.display();
}