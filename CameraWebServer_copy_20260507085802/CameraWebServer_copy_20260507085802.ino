#include <Arduino.h>
#include "esp_camera.h"
#include <WiFi.h>

// ================================================================
// ক্য়ামেরা পিন কনফিগারেশন (AI-Thinker Model-এর জন্য)
// ================================================================
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// ===========================
// তোমার WiFi ডিটেইলস এখানে দাও
// ===========================
const char *ssid = "YOUR_SSID";
const char *password = "YOUR_PASSWORD";
void startCameraServer();

void setup() {
  // ১. CPU স্পিড সর্বোচ্চ (240MHz) করা হলো প্রসেসিং ফাস্ট করার জন্য
  setCpuFrequencyMhz(240);
  
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  // ২. রেজোলিউশন অপ্টিমাইজেশন: CIF (400x296) স্ট্রিমিংয়ের জন্য সবথেকে ফাস্ট
  if(psramFound()){
    config.frame_size = FRAMESIZE_CIF; 
    config.jpeg_quality = 20; // ২০ দিলে ফাইল সাইজ ছোট হয় এবং স্পিড বাড়ে
    config.fb_count = 2;
    config.grab_mode = CAMERA_GRAB_LATEST; // লেটেস্ট ফ্রেম নেওয়ার সেটিংস
  } else {
    config.frame_size = FRAMESIZE_QVGA;
    config.fb_location = CAMERA_FB_IN_DRAM;
    config.jpeg_quality = 20;
    config.fb_count = 1;
  }

  // ক্যামেরা ইনিশিয়ালাইজেশন
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  // ৩. সেন্সর সেটিংস ফাইন-টিউন করা
  sensor_t * s = esp_camera_sensor_get();
  s->set_vflip(s, 1);       // ক্যামেরা উল্টো থাকলে এটি ১ বা ০ করে ঠিক করো
  s->set_hmirror(s, 0);     // মিরর ইমেজ ঠিক করার জন্য
  s->set_brightness(s, 1);  // আলো একটু বাড়ানোর জন্য
  s->set_contrast(s, 1);    // কন্ট্রাস্ট বাড়ালে চোখ ডিটেক্ট করা সহজ হয়

  // WiFi কানেকশন
  WiFi.begin(ssid, password);
  WiFi.setSleep(false); // ওয়াইফাইকে ঘুমাতে দেওয়া যাবে না (ল্যাগ কমানোর জন্য)

  Serial.print("WiFi connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");

  // ক্যামেরা সার্ভার শুরু
  startCameraServer();

  Serial.print("Camera Ready! Use 'http://");
  Serial.print(WiFi.localIP());
  Serial.println(":81/stream' in your Python code");
}

void loop() {
  // স্ট্রিমিং ব্যাকগ্রাউন্ড টাস্কে চলে, তাই লুপ খালি থাকবে
  delay(10000);
}