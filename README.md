# AiThink

# AI-Cam
An AI-powered camera project using the ESP32-CAM, SSD1306 OLED display, and OpenAI's GPT-4 Vision API.
Capture images, send them to OpenAI for summarization or analysis, and scroll the response on a tiny OLED.

# Demo
https://docsend.com/view/s/ysh76zp6jmcn5933

# Features
  - One-button image capture and AI prompt
  - Live status messages on OLED (Wi-Fi, camera, processing)
  - Word-wrapped, paged display of GPT responses
  - Base64 image encoding + secure HTTPS POST
  - Fully standalone and portable

# How It Works
  - Press the button (GPIO2) to trigger the following:
  - Capture a frame from the OV2640 camera
  - Encode the JPEG in Base64
  - Send the prompt + image to OpenAI GPT-4 Vision via HTTPS
  - Parse the JSON response
  - Word wrap and scroll the response on the OLED

# Wiring
| Component       | Arduino Pin  |
|----------------|--------------|
| OLED VCC       | 5V           |
| OLED GND       | GND          |
| OLED SDA       | 15           |
| OLED SCL       | 14           |
| Button         | 2 (to GND)   |
| ESP32-CAM VCC  | 5V           |
| ESP32-CAM GND  | GND          |

*In code, use Wire.begin(15, 14); and pinMode(BUTTON_PIN, INPUT_PULLUP)*

# Getting Started
  - Clone or download this repo
  - Open AI-Cam.ino in the Arduino IDE
  - Install the required libraries:
  - Adafruit SSD1306
  - Adafruit GFX
  - ArduinoJson
  - Base64
  - Add the ESP32 board URL in Preferences
  - Install esp32 core v2.x via the Boards Manager
  - Replace the following in the code:

const char* ssid     = "YOUR_SSID";
const char* password = "YOUR_PASSWORD";
const String apiKey  = "sk-...";
Select the AI Thinker ESP32-CAM board

  - Set partition to Huge APP
  - Upload at 115200 baud
  - Power the ESP32-CAM via 5V and GND from the USB converter
  - Press the button to capture and view AI summaries on the OLED
