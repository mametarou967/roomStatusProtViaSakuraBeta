#include <Arduino.h>
#include "M5Stack.h"
#include <esp_now.h>
#include "WiFi.h"
#include <string.h>
#include "sipf_client.h"

esp_now_peer_info_t slave;

#define UPDATE_DISP_INTERVAL_MSEC  5000
#define UPDATE_DATAUPLOAD_INTERVAL_MSEC  60000 // 1分に1回書く
#define MAX(a,b) ((a)>(b)?(a):(b))
int interval_counter = 1;

//device
#define DEVICE_NUMBER_MIN 1
#define DEVICE_NUMBER_MAX 4

#define SENSOR_TAG_MAX 5
#define SENSOR_TAG_TMP  1
#define SENSOR_TAG_HUMI  2
#define SENSOR_TAG_PRESSURE  3
#define SENSOR_TAG_TVOC  4
#define SENSOR_TAG_ECO2  5

struct deviceInfo
{
  double tmp;
  double humi;
  double pressure;
  int tvoc;
  int eco2;
};
struct deviceInfo devInfo[DEVICE_NUMBER_MAX] = {0};

// esp-now 受信コールバック
void OnDataRecv(const uint8_t *mac_addr, const uint8_t *data, int data_len) {
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  Serial.printf("Last Packet Recv from: %s\n", macStr);

  int deviceNumber = data[0];
  double tempValue = (double)data[1] + ((double)data[2] * 0.01);
  double humiValue = (double)data[3] + ((double)data[4] * 0.01);
  double pressureValue = ((double)data[5] * 100) + (double)data[6] + ((double)data[7] * 0.01);
  int tvocValue = (data[8] * 100) + data[9];
  int eco2Value = (data[10] * 100) + data[11];

  Serial.printf("DvNo:%d ",deviceNumber);
  Serial.printf("Temp:%2.2lfC ", tempValue);
  Serial.printf("Humi:%2.2lf%% ", humiValue);
  Serial.printf("pressure:%4.2lfPa ", pressureValue);
  Serial.printf("TVOC:%4dppb ", tvocValue);
  Serial.printf("eCO2:%4dppm\n", eco2Value);

  // 画面表示用/データ送信用のデータ更新
  if((deviceNumber >=  DEVICE_NUMBER_MIN) && (deviceNumber <=  DEVICE_NUMBER_MAX)){
    devInfo[deviceNumber-1].tmp = tempValue;
    devInfo[deviceNumber-1].humi = humiValue;
    devInfo[deviceNumber-1].pressure = pressureValue;
    devInfo[deviceNumber-1].tvoc = tvocValue;
    devInfo[deviceNumber-1].eco2 = eco2Value;
  }
}


/**
 * SIPF接続情報
 */
static uint8_t buff[256];

static int resetSipfModule()
{
  digitalWrite(5, LOW);
  pinMode(5, OUTPUT);

  // Reset要求
  digitalWrite(5, HIGH);
  delay(10);
  digitalWrite(5, LOW);

  // UART初期化
  Serial2.begin(115200, SERIAL_8N1, 16, 17);

  // 起動完了メッセージ待ち
  Serial.println("### MODULE OUTPUT ###");
  int len, is_echo = 0;
  for (;;) {
    len = SipfUtilReadLine(buff, sizeof(buff), 300000); //タイムアウト300秒
    if (len < 0) {
      return -1;  //Serialのエラーかタイムアウト
    }
    if (len == 1) {
      //空行なら次の行へ
      continue;
    }
    if (len >= 13) {
      if (memcmp(buff, "*** SIPF Client", 15) == 0) {
        is_echo = 1;
      }
      //起動完了メッセージを確認
      if (memcmp(buff, "+++ Ready +++", 13) == 0) {
        Serial.println("#####################");
        break;
      }
      //接続リトライオーバー
      if (memcmp(buff, "ERR:Faild", 9) == 0) {
        Serial.println((char*)buff);
        Serial.println("#####################");
        return -1;
      }
    }
    if (is_echo) {
      Serial.printf("%s\r\n", (char*)buff);
    }
  }
  return 0;
}

static void DispDeviceInfo()
{
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

static void sendDeviceInfoToSipf()
{
  int devIndex = 0;

  for(devIndex = 0;devIndex < DEVICE_NUMBER_MAX;devIndex=devIndex+1){
    int devNumber = devIndex + 1;
    int sensorTagIndex = 0;

    for(sensorTagIndex = 0;sensorTagIndex < SENSOR_TAG_MAX;sensorTagIndex=sensorTagIndex+1){
      int sensorTagNumber = sensorTagIndex + 1;
      int sendTag = (devNumber << 4) | sensorTagNumber;
      SimpObjTypeId sendObjType=OBJ_TYPE_UINT8;
      uint8_t *sendData;
      int sendDataLen = 0;
      bool tagError = false;
      
      switch(sensorTagNumber){
       case  SENSOR_TAG_TMP:
        sendObjType = OBJ_TYPE_FLOAT64;
        sendData = (uint8_t *)&(devInfo[devIndex].tmp);
        sendDataLen = sizeof(double);
        break;
       case  SENSOR_TAG_HUMI:
        sendObjType = OBJ_TYPE_FLOAT64;
        sendData = (uint8_t *)&(devInfo[devIndex].humi);
        sendDataLen = sizeof(double);
        break;
       case  SENSOR_TAG_PRESSURE:
        sendObjType = OBJ_TYPE_FLOAT64;
        sendData = (uint8_t *)&(devInfo[devIndex].pressure);
        sendDataLen = sizeof(double);
        break;
       case SENSOR_TAG_TVOC:
        sendObjType = OBJ_TYPE_UINT32;
        sendData = (uint8_t *)&(devInfo[devIndex].tvoc);
        sendDataLen = sizeof(int);
        break;
       case SENSOR_TAG_ECO2:
        sendObjType = OBJ_TYPE_UINT32;
        sendData = (uint8_t *)&(devInfo[devIndex].eco2);
        sendDataLen = sizeof(int);
        break;
       default:
        tagError = true;
        break;
      }

      if(tagError == false){
        memset(buff, 0, sizeof(buff));
        int ret = SipfCmdTx(sendTag, sendObjType, sendData, sendDataLen, buff);
        if (ret == 0) {
          Serial.printf("OK\nOTID: %s\n", buff);
        } else {
          Serial.printf("NG: %d\n", ret);
        }
      }
    }
  }
}

void setup()
{
  // serial
  Serial.begin(115200);
  
  // M5Stack
  M5.begin();
  M5.Lcd.setTextSize(2);

  // Sipf初期化
  M5.Lcd.printf("SipfModule Booting...");
  if (resetSipfModule() == 0) {
    M5.Lcd.printf(" OK\n");
  } else {
    M5.Lcd.printf(" NG\n");
    return;
  }
  
  M5.Lcd.printf("Setting auth mode...\n");
  if (SipfSetAuthMode(0x01) == 0) {
    M5.Lcd.printf(" OK\n");
  } else {
    M5.Lcd.printf(" NG\n");
    return;
  }

  // ESP-NOW初期化
  M5.Lcd.printf("ESP-NOW Setup...\n");
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

  M5.Lcd.printf("Ready\n");
  Serial.println("Ready");
}

void loop()
{
  M5.update();

  // ディスプレイタイマー満了
  if((interval_counter % UPDATE_DISP_INTERVAL_MSEC) == 0)
  {
    DispDeviceInfo();
  }
  
  // データアップロードタイマー満了
  if((interval_counter % UPDATE_DATAUPLOAD_INTERVAL_MSEC) == 0)
  {
    sendDeviceInfoToSipf();
  }

  // カウンターのリセット
  if(interval_counter > MAX(UPDATE_DISP_INTERVAL_MSEC,UPDATE_DATAUPLOAD_INTERVAL_MSEC))
  {
    Serial.printf("reset!! %d", interval_counter);
    interval_counter = 0;
  }
  
  interval_counter = interval_counter + 1;
  
  delay(1);

  // ----------------------option?
  
  // put your main code here, to run repeatedly:
  int available_len;
  /* PCとモジュールのシリアルポートを中継 */
  available_len = Serial.available();
  for (int i = 0; i < available_len; i++) {
    unsigned char b = Serial.read();
    Serial2.write(b);
  }

  available_len = Serial2.available();
  for (int i = 0; i < available_len; i++) {
    unsigned char b = Serial2.read();
    Serial.write(b);
  }
}
