#include <WiFi.h>
#include <HTTPClient.h>
#include <Base64.h>
#include "esp_camera.h"
#include <Adafruit_GFX.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <ArduinoJson.h>
#include <vector>

/* ─── Wi-Fi & OpenAI Credentials ───────────────── */
const char* ssid     = "YOUR_SSID";
const char* password = "YOUR_PASSWORD";
const String apiKey  = "sk-...";

/* ─── OLED (SSD1306 128×64) over I²C ───────────── */
#define SCREEN_WIDTH   128
#define SCREEN_HEIGHT    64
#define OLED_RESET      -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

/* ─── Button (GPIO2 → GND) ─────────────────────── */
#define BUTTON_PIN       2

/* ─── Camera pin map ───────────────────────────── */
#define PWDN_GPIO_NUM   32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM   0
#define SIOD_GPIO_NUM  26
#define SIOC_GPIO_NUM  27
#define Y9_GPIO_NUM    35
#define Y8_GPIO_NUM    34
#define Y7_GPIO_NUM    39
#define Y6_GPIO_NUM    36
#define Y5_GPIO_NUM    21
#define Y4_GPIO_NUM    19
#define Y3_GPIO_NUM    18
#define Y2_GPIO_NUM     5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM  23
#define PCLK_GPIO_NUM  22

// ─── display a simple line ──────────────────────
void showText(const String &s) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(s);
  display.display();
}

// ─── wrap a long String into word-wrapped lines ───
void wrapText(const String &text, uint8_t maxChars, std::vector<String> &lines) {
  String word, line;
  for (size_t i = 0; i <= text.length(); i++) {
    char c = (i < text.length()) ? text[i] : ' ';
    if (c == ' ' || c == '\n') {
      if (line.length() + word.length() + 1 > maxChars) {
        lines.push_back(line);
        line = word;
      } else {
        if (line.length()) line += ' ';
        line += word;
      }
      word = "";
      if (c == '\n') {
        lines.push_back(line);
        line = "";
      }
    } else {
      word += c;
    }
  }
  if (line.length()) lines.push_back(line);
}

// ─── display pages of up to 4 lines each ─────────
void displayPages(const std::vector<String> &lines) {
  const uint8_t linesPerPage = 4;
  for (size_t pageStart = 0; pageStart < lines.size(); pageStart += linesPerPage) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    for (uint8_t i = 0; i < linesPerPage; i++) {
      size_t idx = pageStart + i;
      if (idx >= lines.size()) break;
      display.setCursor(0, i * 12);
      display.println(lines[idx]);
    }
    display.display();
    delay(5000);
  }
}

// ─── init camera ────────────────────────────────
bool initCam() {
  camera_config_t config;
  config.ledc_channel   = LEDC_CHANNEL_0;
  config.ledc_timer     = LEDC_TIMER_0;
  config.pin_d0         = Y2_GPIO_NUM;
  config.pin_d1         = Y3_GPIO_NUM;
  config.pin_d2         = Y4_GPIO_NUM;
  config.pin_d3         = Y5_GPIO_NUM;
  config.pin_d4         = Y6_GPIO_NUM;
  config.pin_d5         = Y7_GPIO_NUM;
  config.pin_d6         = Y8_GPIO_NUM;
  config.pin_d7         = Y9_GPIO_NUM;
  config.pin_xclk       = XCLK_GPIO_NUM;
  config.pin_pclk       = PCLK_GPIO_NUM;
  config.pin_vsync      = VSYNC_GPIO_NUM;
  config.pin_href       = HREF_GPIO_NUM;
  config.pin_sscb_sda   = SIOD_GPIO_NUM;
  config.pin_sscb_scl   = SIOC_GPIO_NUM;
  config.pin_pwdn       = PWDN_GPIO_NUM;
  config.pin_reset      = RESET_GPIO_NUM;
  config.xclk_freq_hz   = 20000000;
  config.pixel_format   = PIXFORMAT_JPEG;
  config.frame_size     = FRAMESIZE_QVGA;
  config.jpeg_quality   = 12;
  config.fb_count       = 1;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed: 0x%X (%s)\n",
                  err, esp_err_to_name(err));
    return false;
  }
  return true;
}

// ─── send image + prompt to OpenAI ──────────────
bool postToOpenAI(const String &b64, String &rawJSON,
                  const String &userPrompt, const String &waitingText) {
  showText(waitingText);

  DynamicJsonDocument body(8192);
  body["model"] = "gpt-4o";
  JsonArray msgs = body.createNestedArray("messages");
  JsonObject msg = msgs.createNestedObject();
  msg["role"] = "user";
  JsonArray content = msg.createNestedArray("content");

  // text block
  {
    JsonObject t = content.createNestedObject();
    t["type"] = "text";
    t["text"] = userPrompt;
  }
  // image block
  {
    JsonObject img = content.createNestedObject();
    img["type"] = "image_url";
    JsonObject u = img.createNestedObject("image_url");
    u["url"]    = "data:image/jpeg;base64," + b64;
    u["detail"] = "auto";
  }
  body["max_tokens"] = 100;

  String payload;
  serializeJson(body, payload);

  HTTPClient http;
  http.begin("https://api.openai.com/v1/chat/completions");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + apiKey);

  int code = http.POST(payload);
  if (code < 200 || code >= 300) {
    String err = http.getString();
    showText("Err " + String(code));
    delay(1500);
    DynamicJsonDocument e(1024);
    deserializeJson(e, err);
    showText(e["error"]["message"] | "unknown");
    delay(3000);
    http.end();
    return false;
  }

  rawJSON = http.getString();  
  http.end();
  return true;
}

// ─── capture + AI + display (with paging) ───────
void processImage(const String &prompt, const String &waitingText) {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    showText("Capture ERR");
    return;
  }

  // base64 encode
  String b64 = base64::encode(fb->buf, fb->len);
  esp_camera_fb_return(fb);

  // call API
  String raw;
  if (!postToOpenAI(b64, raw, prompt, waitingText)) return;

  // parse
  DynamicJsonDocument resp(24576);
  DeserializationError err = deserializeJson(resp, raw);
  if (err) {
    showText(err.c_str());
    return;
  }

  String answer = resp["choices"][0]["message"]["content"].as<String>();
  if (answer.isEmpty()) answer = "(empty)";

  // wrap + page through it
  std::vector<String> lines;
  wrapText(answer, 20, lines);
  displayPages(lines);
}

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  Wire.begin(15, 14);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);

  showText("Connecting WiFi...");
  WiFi.begin(ssid, password);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start > 10000) {
      showText("WiFi FAIL");
      return;
    }
    delay(500);
  }
  showText("WiFi OK");

  if (!initCam()) {
    showText("CAM ERR");
    while (true) delay(1000);
  }
  showText("Press btn");
}

void loop() {
  if (digitalRead(BUTTON_PIN) == LOW) {
    while (digitalRead(BUTTON_PIN) == LOW) delay(10);
    processImage("Summarize the image", "Summarizing...");
    delay(5000);
    showText("Press btn");
  }
}
