// Implementation of Button press function. Menu is displayed at first.
// If power button is pressed then Time and date values will displayed.
// If Button A (Home Button) is pressed then BME280 value is displayed.
// If Button B (Right button is pressed then Gyroscope is displayed.

// For Over the Air(OTA) Program
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>

// Date and Time libraries
#include "WiFi.h"
#include "time.h"

// M5 Stick Libraries
#include <M5StickC.h>
#include <Wire.h>

//For BME280
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
Adafruit_BME280 bme;

// WiFi connection credentials
const char* host = "M5Stick-C";
const char* ssid = "tkwtic24";
const char* password = "20191002";

WebServer server(80);

// NTP Server credentials
const char* ntpServer = "ntp.jst.mfeed.ad.jp";
const long gmtOffset_sec = 9 * 3600;            // Japan time is GMT +9
const int daylightOffset_sec = 0;
struct tm timeinfo;

//For LED
int PIN = 32;

//For Gyroscope
float accX = 0.0F;
float accY = 0.0F;
float accZ = 0.0F;

float gyroX = 0.0F;
float gyroY = 0.0F;
float gyroZ = 0.0F;


//OTA browser code upload page
/* Style */
String style =
"<style>#file-input,input{width:100%;height:44px;border-radius:4px;margin:10px auto;font-size:15px}"
"input{background:#f1f1f1;border:0;padding:0 15px}body{background:#3498db;font-family:sans-serif;font-size:14px;color:#777}"
"#file-input{padding:0;border:1px solid #ddd;line-height:44px;text-align:left;display:block;cursor:pointer}"
"#bar,#prgbar{background-color:#f1f1f1;border-radius:10px}#bar{background-color:#3498db;width:0%;height:10px}"
"form{background:#fff;max-width:258px;margin:75px auto;padding:30px;border-radius:5px;text-align:center}"
".btn{background:#3498db;color:#fff;cursor:pointer}</style>";

/* Login page */
String loginIndex = 
"<form name=loginForm>"
"<h1>ESP32 Login</h1>"
"<input name=userid placeholder='User ID'> "
"<input name=pwd placeholder=Password type=Password> "
"<input type=submit onclick=check(this.form) class=btn value=Login></form>"
"<script>"
"function check(form) {"
"if(form.userid.value=='admin' && form.pwd.value=='admin')"
"{window.open('/serverIndex')}"
"else"
"{alert('Error Password or Username')}"
"}"
"</script>" + style;
 
/* Server Index Page */
String serverIndex = 
"<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></script>"
"<form method='POST' action='#' enctype='multipart/form-data' id='upload_form'>"
"<input type='file' name='update' id='file' onchange='sub(this)' style=display:none>"
"<label id='file-input' for='file'>   Choose file...</label>"
"<input type='submit' class=btn value='Update'>"
"<br><br>"
"<div id='prg'></div>"
"<br><div id='prgbar'><div id='bar'></div></div><br></form>"
"<script>"
"function sub(obj){"
"var fileName = obj.value.split('\\\\');"
"document.getElementById('file-input').innerHTML = '   '+ fileName[fileName.length-1];"
"};"
"$('form').submit(function(e){"
"e.preventDefault();"
"var form = $('#upload_form')[0];"
"var data = new FormData(form);"
"$.ajax({"
"url: '/update',"
"type: 'POST',"
"data: data,"
"contentType: false,"
"processData:false,"
"xhr: function() {"
"var xhr = new window.XMLHttpRequest();"
"xhr.upload.addEventListener('progress', function(evt) {"
"if (evt.lengthComputable) {"
"var per = evt.loaded / evt.total;"
"$('#prg').html('progress: ' + Math.round(per*100) + '%');"
"$('#bar').css('width',Math.round(per*100) + '%');"
"}"
"}, false);"
"return xhr;"
"},"
"success:function(d, s) {"
"console.log('success!') "
"},"
"error: function (a, b, c) {"
"}"
"});"
"});"
"</script>" + style;


void setup() {
  // put your setup code here, to run once:
  M5.begin();
  Wire.begin(0, 26);
  Serial.begin(115200);
  M5.IMU.Init();

  // WiFi connection setup
  Serial.print("Connecting to: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
  }
  Serial.println(" CONNECTED");

  //init and get the time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.println(&timeinfo, "%Y/%m/%d %a %H:%M:%S");

  // Disconnect WiFi as it's no longer needed
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);

  // BME Connection
  while (!bme.begin()) {
    M5.Lcd.println("Error! Check wiring!");
  }

  /*use mdns for host name resolution*/
  if (!MDNS.begin(host)) { //http://esp32.local
    Serial.println("Error setting up MDNS responder!");
    while (1) {
      delay(1000);
    }
  }
  
  Serial.println("mDNS responder started");
  /*return index page which is stored in serverIndex */
  server.on("/", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", loginIndex);
  });
  server.on("/serverIndex", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", serverIndex);
  });
  /*handling uploading firmware file */
  server.on("/update", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    ESP.restart();
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.printf("Update: %s\n", upload.filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { //start with max available size
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      /* flashing firmware to ESP*/
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) { //true to set the size to the current progress
        Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
    }
  });
  server.begin();

  // Displaying Menu on M5 LCD.
  M5.Lcd.setRotation(3);
  M5.Lcd.setCursor(0, 0);
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.println("Press \n Pwr Btn: Time \n Btn A: BME280 \n Btn B: Gyroscope");
}

void loop() {
  // put your main code here, to run repeatedly
  M5.update();
  server.handleClient();
  delay(1);

  if (M5.BtnA.isPressed()) {      // If the home button (Button A), big button on the middle is pressed then tmeperature, pressure and humidity value is shown but you have
    while (1) {

      pinMode(GPIO_NUM_10, OUTPUT);
      digitalWrite(GPIO_NUM_10, LOW);

      M5.Lcd.fillScreen(BLACK);    
      
      Bme280();

      pinMode(GPIO_NUM_10, OUTPUT);
      digitalWrite(GPIO_NUM_10, HIGH);

      loop();
    }
  }

  if (M5.BtnB.isPressed()) {    // If the Button B (On the right side middle) is pressed then gyroscope value is shown but you have to
    while (1) {
      M5.Lcd.fillScreen(BLACK);   
      M5.Lcd.setCursor(0, 0);

      pinMode(GPIO_NUM_10, OUTPUT);
      digitalWrite(GPIO_NUM_10, LOW);

      Gyroscope();
      loop();
    }
  }

  if (M5.Axp.GetBtnPress()) {     // Pressing the power button on the left down side shows you date and time
    while (1) {
      pinMode(GPIO_NUM_10, OUTPUT);
      digitalWrite(GPIO_NUM_10, HIGH);
      
      M5.Lcd.fillScreen(BLACK);
      M5.Lcd.setRotation(3);
      M5.Lcd.setCursor(15, 10);
      getTime();
      loop();
    }
  }
  delay(1000);
}

void Bme280() {
  M5.Lcd.fillScreen(BLACK);
  getTempC();
  getPressureP();
  getHumidityR();

}

void Gyroscope() {

  M5.Lcd.setTextColor(WHITE, BLACK);
  M5.Lcd.setTextSize(1);
  M5.Lcd.setCursor(40, 0);
  M5.Lcd.println("SH200I TEST");
  M5.Lcd.setCursor(0, 15);
  M5.Lcd.println("  X       Y       Z");

  float temp = 0;
  M5.IMU.getGyroData(&gyroX, &gyroY, &gyroZ);
  M5.IMU.getAccelData(&accX, &accY, &accZ);
  M5.IMU.getTempData(&temp);

  M5.Lcd.setCursor(0, 30);
  M5.Lcd.printf("%6.2f  %6.2f  %6.2f      ", gyroX, gyroY, gyroZ);
  M5.Lcd.setCursor(140, 30);
  M5.Lcd.print("o/s");
  M5.Lcd.setCursor(0, 45);
  M5.Lcd.printf(" %5.2f  %5.2f  %5.2f   ", accX, accY, accZ);
  M5.Lcd.setCursor(140, 45);
  M5.Lcd.print("G");
  M5.Lcd.setCursor(0, 60);
  M5.Lcd.printf("Temperature : %.2f C", temp);

}

void getTime() {
  getLocalTime(&timeinfo);        // Updating the date and time values in every loop run
  M5.Lcd.setTextColor(RED, BLACK);
  M5.Lcd.setTextSize(2);
  M5.Lcd.println(&timeinfo, "%Y/%m/%d \n");
  
  M5.Lcd.setTextColor(GREEN, BLACK);
  M5.Lcd.setTextSize(3);
  M5.Lcd.println(&timeinfo, "%H:%M:%S");

}

void getTempC() {
  M5.Lcd.setCursor(4, 0);
  M5.Lcd.setTextColor(RED);
  M5.Lcd.setTextSize(2);
  M5.Lcd.println(" ");
  M5.Lcd.print("T:");
  M5.Lcd.print(bme.readTemperature());
  M5.Lcd.println(" *C");
}

void getPressureP() {
  M5.Lcd.setCursor(4, 38);
  M5.Lcd.setTextColor(GREEN);
  M5.Lcd.setTextSize(2);
  M5.Lcd.print("P:");
  M5.Lcd.print(bme.readPressure() / 100.0F);
  M5.Lcd.println(" hPa");
}

void getHumidityR() {
  M5.Lcd.setCursor(4, 56);
  M5.Lcd.setTextColor(BLUE);
  M5.Lcd.setTextSize(2);
  M5.Lcd.print("H:");
  M5.Lcd.print(bme.readHumidity());
  M5.Lcd.println(" %");
  M5.Lcd.println(" ");
}
