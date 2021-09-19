#include "M5Stack.h"
#include <esp_now.h>
#include "WiFi.h"

esp_now_peer_info_t slave;

#define N 1024

//device
#define DEVICE_NUMBER_MIN 1
#define DEVICE_NUMBER_MAX 4
struct deviceInfo
{
  double tmp;
  double humi;
  int tvoc;
  int eco2;
};
struct deviceInfo devInfo[DEVICE_NUMBER_MAX] = {0};

// 受信コールバック
void OnDataRecv(const uint8_t *mac_addr, const uint8_t *data, int data_len) {
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  Serial.printf("Last Packet Recv from: %s\n", macStr);
  Serial.printf("Last Packet Recv Data(%d): ", data_len);
  for ( int i = 0 ; i < data_len ; i++ ) {
    Serial.print(data[i]);
    Serial.print(" ");
  }
  Serial.println("");

  int deviceNumber = data[0];
  double tempValue = (double)data[1] + ((double)data[2] * 0.01);
  double humiValue = (double)data[3] + ((double)data[4] * 0.01);
  int tvocValue = (data[5] << 8) + data[6];
  int eco2Value = (data[7] << 8) + data[8];
  
  if((deviceNumber >=  DEVICE_NUMBER_MIN) && (deviceNumber <=  DEVICE_NUMBER_MAX)){
    devInfo[deviceNumber-1].tmp = tempValue;
    devInfo[deviceNumber-1].humi = humiValue;
    devInfo[deviceNumber-1].tvoc = tvocValue;
    devInfo[deviceNumber-1].eco2 = eco2Value;
  }
}

void setup()
{
  // serial
  Serial.begin(115200);
  
  // M5Stack
  M5.begin();
  M5.Lcd.setTextSize(2);
  
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
  esp_now_register_recv_cb(OnDataRecv);
}

void loop()
{
  delay(5000);
  M5.update();
  
  M5.Lcd.setCursor(0, 0);
  {
    int i = 0;
    for(i = 0;i < DEVICE_NUMBER_MAX;i=i+2)
    {
      M5.Lcd.setTextColor(WHITE, BLACK);
      M5.Lcd.printf("DvNo|    %d    |     %d\n",i+1,i+2);
      M5.Lcd.printf("----+---------+-----------\n");
      M5.Lcd.printf("Temp| %02.2fC  | %02.2fC\n",devInfo[i].tmp,devInfo[i+1].tmp);
      M5.Lcd.printf("Humi| %02.2f%%  | %02.2f%%\n",devInfo[i].humi,devInfo[i+1].humi);
      M5.Lcd.printf("TVOC|  %4dppb|  %4dppb\n", devInfo[i].tvoc,devInfo[i+1].tvoc);
      M5.Lcd.printf("eCO2|  %4dppm|  %4dppb\n", devInfo[i].eco2,devInfo[i+1].eco2);
      M5.Lcd.printf("\n");
    }
  }
}
