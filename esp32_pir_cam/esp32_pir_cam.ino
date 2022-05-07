#include <HTTPClient.h>
#include <esp_camera.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <esp_timer.h>

#include "FS.h"                // SD Card ESP32
#include "SD_MMC.h"            // SD Card ESP32

#include "driver/rtc_io.h"
#include <EEPROM.h>            // read and write from flash memory

// define the number of bytes you want to access
#define EEPROM_SIZE 1





/*********************需要修改的地方**********************/
const char* ssid = "RecluseW";           //WIFI名称
const char* password = "lzwjy815";     //WIFI密码
int capture_interval = 20*1000;        // 默认20秒上传一次，可更改（本项目是自动上传，如需条件触发上传，在需要上传的时候，调用take_send_photo()即可）
//const char*  post_url = "http://images.bemfa.com/upload/v1/upimages.php"; // 默认上传地址
//const char*  uid = "XXXXXXXXXXXXXXXXXXXXXXXXXX";    //用户私钥，巴法云控制台获取
//const char*  topic = "XXXXXXXXX";     //图云主题名字，可在控制台新建
bool sentWechat = false;               //是否推送到微信，默认不推送，true 为推送。需要在控制台先绑定微信，不然推送不到
const char*  wechatMsg = "Somebody is here.";     //推送到微信的消息，可随意修改，修改为自己需要发送的消息
/********************************************************/


bool internet_connected = false;
long current_millis;
long last_capture_millis = 0;

int pictureNumber = 0;

struct tm timeinfo;

//Time Server
#define NTP1  "ntp1.aliyun.com"
#define NTP2  "ntp2.aliyun.com"
#define NTP3  "ntp3.aliyun.com"

// CAMERA_MODEL_AI_THINKER
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

void setup(){
    Serial.begin(115200);
    //PIR初始状态
    pinMode(12,INPUT);
    digitalWrite(12,LOW);
    
    if (init_wifi()) { // Connected to WiFi
        internet_connected = true;
        Serial.println("Internet connected");
        configTime(8 * 3600, 0, NTP1, NTP2,NTP3);
        setClock();
    }

    init_SDCard();

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
    config.pin_sscb_sda = SIOD_GPIO_NUM;
    config.pin_sscb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;

    //init with high specs to pre-allocate larger buffers
    if (psramFound()) {
        config.frame_size = FRAMESIZE_UXGA;
        config.jpeg_quality = 10;
        config.fb_count = 2;
    } else {
        config.frame_size = FRAMESIZE_SVGA;
        config.jpeg_quality = 12;
        config.fb_count = 1;
    }

    // camera init
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("Camera init failed with error 0x%x", err);
        return;
    }
}
void setClock() {
    
    if (!getLocalTime(&timeinfo)){
        if(init_wifi()){
            configTime(8 * 3600, 0, NTP1, NTP2,NTP3);
        }
        return;
    }
    Serial.println(&timeinfo, "%F %T %A"); // 格式化输出:2021-10-24 23:00:44 Sunday
    Serial.print(asctime(&timeinfo));  //默认打印格式：Mon Oct 25 11:13:29 2021
}


/********初始化WIFI*********/
bool init_wifi(){
    int connAttempts = 0;
    Serial.println("\r\nConnecting to: " + String(ssid));
    WiFi.disconnect(false);
    WiFi.mode(WIFI_STA);//开启网络  
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED ) {
        delay(500);
        Serial.print(".");
        if (connAttempts > 10) return false;
        connAttempts++;
    }
    return true;
}


/***init SDCARD***/
 void init_SDCard(){

    Serial.println("Starting SD Card");
    if(!SD_MMC.begin()){
        Serial.println("SD Card Mount Failed");
        return;
    }
    uint8_t cardType = SD_MMC.cardType();
    if(cardType == CARD_NONE){
        Serial.println("No SD Card attached");
        return;
    }
}

/********推送图片via HTTP*********/

//void send_via_HTTP( camera_fb_t * fb){
    // HTTPClient http;
    // //设置请求url
    // http.begin(post_url);
    // //设置请求头部信息
    // http.addHeader("Content-Type", "image/jpg");
    // http.addHeader("Authorization", uid);
    // http.addHeader("Authtopic", topic);
    // if(sentWechat){ //判断是否需要推送到微信
    //   http.addHeader("Wechatmsg", wechatMsg);  //设置 http 请求头部信息
    // }
    // //发起请求，并获取状态码
    // int httpResponseCode = http.POST((uint8_t *)fb->buf, fb->len);

    // if(httpResponseCode==200){
    //     //获取post请求后的服务器响应信息，json格式
    //     String response = http.getString();  //Get the response to the request
    //     Serial.print("Response Msg:");
    //     Serial.println(response);           // 打印服务器返回的信息
        
    //     //json数据解析
    //     StaticJsonDocument<200> doc;
    //     DeserializationError error = deserializeJson(doc, response);
    //     if (error) {
    //       Serial.print(F("deserializeJson() failed: "));
    //       Serial.println(error.c_str());
    //     }
    //     const char* url = doc["url"];
    //     Serial.print("Get URL:");
    //     Serial.println(url);//打印获取的URL
    
        
    // }else{
    //     //错误请求
    //     Serial.print("Error on sending POST: ");
    //     Serial.println(httpResponseCode);
    // }
   
    // Serial.print("HTTP Response code: ");
    // Serial.println(httpResponseCode);
    // http.end();
//}

/*******SAVE TO SDCARD****/
void save_to_SDCard(camera_fb_t * fb){


 // initialize EEPROM with predefined size
    EEPROM.begin(EEPROM_SIZE);
    pictureNumber = EEPROM.read(0) + 1;

  // Path where new picture will be saved in SD Card
    //time():get timestamp  ,localtime(time_t):conv timestamp to date

     

    time_t timer=time(NULL);
    struct tm * time_current = localtime(&timer);
    
      String path;
      String YY=String(time_current->tm_year);
      String MM=String(time_current->tm_min);
      String DD=String(time_current->tm_mday);
      String HH=String(time_current->tm_hour);
      String M=String(time_current->tm_min);
      String SS=String(time_current->tm_sec);
    
    path="/IMG_"+YY+MM+DD+HH+M+SS+".jpg";

    //sprintf(path,"IMG_%d%d%d-%d%d%d",time_current->tm_year,time_current->tm_min,time_current->tm_mday,time_current->tm_hour,time_current->tm_min,time_current->tm_sec)

    fs::FS &fs = SD_MMC; 
    Serial.printf("Picture file name: %s\n", path.c_str());
  
    File file = fs.open(path.c_str(), FILE_WRITE);
    if(!file){
        Serial.println("Failed to open file in writing mode");
    } 
    else {
        file.write(fb->buf, fb->len); // payload (image), payload length
        Serial.printf("Saved file to path: %s\n", path.c_str());
        EEPROM.write(0, pictureNumber);
        EEPROM.commit();
    }
    file.close();
}



/********拍摄图片*********/
static esp_err_t takephoto()
{
    //初始化相机并拍照
    Serial.println("Taking picture...");
    camera_fb_t * fb = NULL;
    fb = esp_camera_fb_get();

    if (!fb) {
      Serial.println("Camera capture failed");
      return ESP_FAIL;
    }else{
       // send_via_HTTP(fb);
        save_to_SDCard(fb);
    }

    //清空数据
    esp_camera_fb_return(fb);  
    //回收下次再用
  
}



void loop()
{
//PIR检测电平变化
    if(digitalRead(12)==HIGH)  {
        Serial.println("Somebody is here.");
        takephoto();
    }
    else  {

    time_t timer=time(NULL);
    struct tm * time_curr = localtime(&timer);
    getLocalTime(&timeinfo);
    Serial.println(&timeinfo, "%F %T %A"); // 格式化输出:2021-10-24 23:00:44 Sunday
    Serial.print(asctime(&timeinfo));  //默认打印格式：Mon Oct 25 11:13:29 2021
    
    }
    delay(1000);
}
