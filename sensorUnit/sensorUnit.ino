#include "EEPROM.h"
#include <esp_now.h>
#include "WiFi.h"
#include "M5StickC.h"
#include "Adafruit_BMP280.h"
#include "Adafruit_SHT31.h"
#include "Adafruit_SGP30.h"

esp_now_peer_info_t slave;

// EEPROM
#define EEPROM_SIZE 64
#define DEVICE_NUMBER_ADDRESS 1

// device
int deviceNumber = 1;
#define DEVICE_NUMBER_MIN 1
#define DEVICE_NUMBER_MAX 4

// ENV 2
Adafruit_SHT31 sht3x = Adafruit_SHT31(&Wire);
Adafruit_BMP280 bme = Adafruit_BMP280(&Wire);

// SGP30
Adafruit_SGP30 sgp;

void readDeviceNumber()
{
  deviceNumber = EEPROM.readInt(DEVICE_NUMBER_ADDRESS);
  if((deviceNumber  < DEVICE_NUMBER_MIN) || (deviceNumber  >  DEVICE_NUMBER_MAX))
  {
    deviceNumber = DEVICE_NUMBER_MIN;
  }
}

void countUpDeviceNumber()
{
  deviceNumber = deviceNumber + 1;
  if(deviceNumber  >  DEVICE_NUMBER_MAX)
  {
     deviceNumber = DEVICE_NUMBER_MIN;
  }
  EEPROM.writeInt(DEVICE_NUMBER_ADDRESS, deviceNumber);
  EEPROM.commit();
}

// esp-now 送信コールバック
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  Serial.print("Last Packet Sent to: ");
  Serial.println(macStr);
  Serial.print("Last Packet Send Status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

void setup()
{
  // シリアルの初期設定
  Serial.begin(115200);

  // EEPROMの初期化
  if (!EEPROM.begin(EEPROM_SIZE))
  {
    Serial.println("Failed to initialise EEPROM");
    Serial.println("Restarting...");
    delay(1000);
    ESP.restart();
  }
  
  // m5stickcの初期設定
  M5.begin();
  M5.Lcd.setRotation(3);
  M5.Lcd.setTextSize(2);
  Wire.begin(32, 33);

  // deviceNumberの読み出し
  readDeviceNumber();

  // ENV2 の初期設定
  while (!bme.begin(0x76)) {
    Serial.println("Could not find a valid BMP280 sensor, check wiring!");
    M5.Lcd.println("Could not find a valid BMP280 sensor.");
  }
  
  while (!sht3x.begin(0x44)) {
    Serial.println("Could not find a valid SHT3X sensor, check wiring!");
    M5.Lcd.println("Could not find a valid SHT3X sensor.");
  }

  // SGP30の初期設定  
  while(! sgp.begin()){
    Serial.println("Could not find a valid SGP30 sensor, check wiring!");
    M5.Lcd.println("Could not find a valid SGP30 sensor.");
  }

  // ESP-NOW初期化
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  if (esp_now_init() == ESP_OK) {
    Serial.println("ESPNow Init Success");
    M5.Lcd.print("ESPNow Init Success\n");
  } else {
    Serial.println("ESPNow Init Failed");
    M5.Lcd.print("ESPNow Init Failed\n");
    ESP.restart();
  }

  // マルチキャスト用Slave登録
  memset(&slave, 0, sizeof(slave));
  for (int i = 0; i < 6; ++i) {
    slave.peer_addr[i] = (uint8_t)0xff;
  }
  esp_err_t addStatus = esp_now_add_peer(&slave);
  if (addStatus == ESP_OK) {
    // Pair success
    Serial.println("Pair success");
  }

  // ESP-NOWコールバック登録
  esp_now_register_send_cb(OnDataSent);

  M5.Lcd.fillScreen(BLACK);
}

void loop()
{
  M5.update();

  // 各値の取得
  float tmp = sht3x.readTemperature();
  float hum = sht3x.readHumidity();
  float pressure = ( bme.readPressure() / 100);
  sgp.IAQmeasure();

  // 表示の更新
  Serial.printf("Temperatura: %2.2f*C  Humedad: %0.2f%%  Pressure: %0.2fhPa Tvoc:%dppb eCo2:%dppm \r\n", tmp, hum, pressure,sgp.TVOC,sgp.eCO2);
  M5.Lcd.setCursor(0, 0);
  M5.Lcd.setTextColor(WHITE, BLACK);
  M5.Lcd.printf("DvNo:%d\n",deviceNumber);
  M5.Lcd.printf("Temp:%2.2fC\n", tmp);
  M5.Lcd.printf("Humi:%2.2f%%\n", hum);
  M5.Lcd.printf("TVOC:%4dppb\n", sgp.TVOC);
  M5.Lcd.printf("eCO2:%4dppm\n", sgp.eCO2);

  uint8_t tmpUpper = (uint8_t)tmp;
  uint8_t tmpLower = (uint8_t)((tmp - (float)tmpUpper) * 100);
  uint8_t humUpper = (uint8_t)hum;
  uint8_t humLower = (uint8_t)((hum - (float)humUpper) * 100);
  uint8_t pressureUpper1 = (uint8_t)(((int)pressure) / 100);
  uint8_t pressureUpper2 = (uint8_t)(((int)pressure) % 100);
  uint8_t pressureLower = (uint8_t)((pressure - (int)pressure) * 100);
  uint8_t tvocUpper = (uint8_t)(sgp.TVOC / 100);
  uint8_t tvocLower = (uint8_t)(sgp.TVOC % 100);
  uint8_t eco2Upper = (uint8_t)(sgp.eCO2 / 100);
  uint8_t eco2Lower = (uint8_t)(sgp.eCO2 % 100);
  
  uint8_t data[12] = {
    deviceNumber,
    tmpUpper,
    tmpLower,
    humUpper,
    humLower,
    pressureUpper1,
    pressureUpper2,
    pressureLower,
    tvocUpper,
    tvocLower,
    eco2Upper,
    eco2Lower,
    };
    esp_err_t result = esp_now_send(slave.peer_addr, data, sizeof(data));

  // ボタンを押された場合はDeviceNumberを変更
  if(M5.BtnB.wasPressed()){
    Serial.println("ButtonB pressed");
    countUpDeviceNumber();
  }
  
  delay(1000);
}
