/*
  2017.10.14 Ease
  功能说明:
      1.8个继电器开关控制
      2.后置灯带控制
      3.温度信息获取
  其它说明：
      1. 因为电源位不足，也便于控制，使用了1个引脚作为电源用 P4
      2. 因为数字端口不足，使用了模拟端口作为数字端口用 SpeakerPin A5
      3. 计划增加状态查询功能
      4. 将用于控制继电器的6引脚改为2引脚
      5. 重启功能: MT7688将RST引脚置低电平
      6. 使用7号控制导航模块电源
*/
#include <SoftwareSerial.h>                  // 软件串口，用于获取GPS模块数据
#include "FastLED.h"                          // 灯带控制
#include "DHT.h"                              // 温湿度传感器
#define WS2812DataPin 6                       // 灯带控制引脚
#define DHTDataPin  5                         // 温湿度传感器数据引脚
#define SpeakerPin A5                         // 蜂鸣器端口
//CRGB leds[30];

DHT dht;
SoftwareSerial mySerial(2,3);               // 软件串口 RX=2,TX=3 
CRGBArray<30> leds;                          // 先定义30粒LED的数据空间
CRGBPalette16 currentPalette;
TBlendType  currentBlending;
extern CRGBPalette16 myRedWhiteBluePalette;
extern const TProgmemPalette16 myRedWhiteBluePalette_p PROGMEM;

unsigned char RPin[] = { 13, 12, 11, 10, 17, 16, 15, 14, 7, 4 };  // 继电器与引脚对应: 14=A0, 15=A1, 16=A2, 17=A3, 4 DHT温度传感器代替电源的引脚 7 代替GPS模块电源的引脚
unsigned char WS2812Num = 30;                           // 灯带---灯数
unsigned int WS2812Dealy = 40;                          // 灯带---显示延迟
bool WS2812Replay = false;                              // 灯带---灯带循环显示开关
String WS2812Cmd = "";                                  // 灯带---灯带循环显示指令
unsigned char WS2812Light = 200;                        // 灯带---灯带亮度
unsigned char SerialChar = 0;                           // 串口字符
String SerialCmd = "";                                  // 串口指令
unsigned char RSwitch = "";                             // 继电器开关状态
unsigned char RLoc = "";                                // 继电器位
unsigned int tmpVar = 0;                                // 临时使用的全局变量
void(* resetFunc) (void) = 0;                           // 制造重启命令(有误)

void setup() {
  Serial.begin(19200);           // 串口与MT7688通讯
  mySerial.begin(9600);          // 软件串口与GPS模块通讯
  randomSeed(analogRead(0));     // 置随机数
  FastLED.addLeds<WS2812, WS2812DataPin, RGB>(leds, WS2812Num).setCorrection( TypicalLEDStrip ); // 初始化灯带
  FastLED.setBrightness(WS2812Light);                                                            // 初始化灯带亮度
  currentPalette = RainbowColors_p;
  currentBlending = LINEARBLEND;
  for (int i = 0; i < sizeof(RPin)/sizeof(RPin[0]); i++) {                                        // 继电器控制引脚初始状态,高电平断开
      pinMode(RPin[i], OUTPUT);
      digitalWrite(RPin[i], HIGH);
  }
  dht.setup(DHTDataPin);  
  pinMode(WS2812DataPin, OUTPUT);
  pinMode(SpeakerPin, OUTPUT);
  while ( !Serial ) {  }
}
void loop() {
  if(mySerial.available())  {         // 当GPS有数据时直接发送到串口（MT7688），可以通过控制电源引脚7关闭GPS
    Serial.write(mySerial.read());   // 从软件串口读出一字节，写入硬件串口
  }
    
  if (Serial.available() > 0)
  {
    SerialChar = Serial.read();
    if (SerialChar == '\n') {
      if ( SerialCmd == "" ) {
          FastLED.show();            // 将灯带指令表现出来，这里大部份用于将灯置黑
          return;
      }
      SerialCmd.trim();
      Serial.println( SerialCmd );
      switch (SerialCmd.charAt(0)) {
        case 'I':
          ShowHelp();
        case 'J':                                              // 继电器控制
          RSwitch = SerialCmd.charAt(2);
          if ( RSwitch == 'Z' ) {
             Serial.println("?");                   
          } else {
            RLoc = SerialCmd.charAt(1);
            if ( RSwitch == '0' ) {
               digitalWrite(RPin[RLoc - 48], HIGH);
            } else {
               digitalWrite(RPin[RLoc - 48], LOW);
            }
            Serial.println(RPin[RLoc - 48],RSwitch);
          }
          break;
        case 'B':                                           // 控制板
          switch (SerialCmd.charAt(1)) {
            case 'R':                                       // 重启程序开始
                resetFunc();                              
                break;
            case 'T':                                      // 获取温度
                delay(dht.getMinimumSamplingPeriod());
                Serial.println("DHT," + String(dht.getHumidity()) + "," + String(dht.getTemperature()));            
                break;
            case 'S':                                     // 蜂鸣器报警
                AlertSpeak();
                Serial.println("Beep");
                break;
          }          
          break;
        case 'L':                                         // 灯带控制
          switch(SerialCmd.charAt(1)) {
            case 'T':                                     // 显示样式定义
                LedShow(SerialCmd);
                Serial.println("LEDSHOW");
                break;
            case 'I':                                      // 初始化显示
                LedI();
                Serial.println("LEDINIT");
                break;
            case 'R':                                     // 灯带循环显示开关
                WS2812Replay = not WS2812Replay;
                if ( WS2812Replay==false ) {
                    WS2812Cmd = "";
                    Serial.println("LEDUNRE");
                } else {
                    Serial.println("LEDRE");
                }
                break;
            case 'L':                                     // 灯带亮度
                WS2812Light = HexToDec(SerialCmd.substring(2));
                FastLED.setBrightness(WS2812Light);
                Serial.println("LEDLIGHT");
                break;
            case 'N':                                     // 灯数量
                WS2812Num = HexToDec(SerialCmd.substring(2));
                memset(leds,0,WS2812Num);                // 重定义灯数组
                Serial.println("LEDNUMBER");
                break;
            case 'S':                                     // 灯带延时
                WS2812Dealy = HexToDec(SerialCmd.substring(2));
                Serial.println("LEDDEALY");
                break;
            case 'G':                                     // 灯组控制
                LedGroupControl(SerialCmd.substring(2));
                Serial.println("LEDGROUP");
                break;
          }
          break;
        default:
          Serial.println("ERROR");
          break;      
      }
      SerialCmd = "";
    } else {
      SerialCmd = SerialCmd + (char)SerialChar;
    }
  } else { 
    if ( WS2812Replay and WS2812Cmd !="" ) {                          // 如果设置了灯带效果重复显示
        LedShow(WS2812Cmd);
        Serial.println(WS2812Cmd);
    } else {
        FastLED.show();
    }
  }
}
// 十六进制转换为十进制
unsigned int HexToDec(String hexString) {
    unsigned int ret = 0;
    for(int i=0;i<hexString.length(); i++) {        
        char ch = hexString[i];
        int val = 0;
        if('0'<=ch && ch<='9')      val = ch - '0';
        else if('a'<=ch && ch<='f') val = ch - 'a' + 10;
        else if('A'<=ch && ch<='F') val = ch - 'A' + 10;
        else continue; // skip non-hex characters
        ret = ret*16 + val;
    }    
    // Serial.println(ret);
    return ret;
}


// 灯带控制
void LedShow(String cmd) {
      if ( cmd == "" ) { return; }
      char type;
      type = cmd.charAt(2);
      if ( type == '0') {
          LedI();
      } else if ( type == '1' ) {
          Led0();
      } else if ( type == '2' ) {
          Led1();
      } else if ( type == '3' ) {
          Led0();
          Led1();
      } else if ( type == '4' ) {
          Led2();
      } else if ( type == '5' ) {
          Led3();
      } else if ( type == '6' ) {
          Led4();
      } else if ( type == '7' ) {
          Led5();
      } else if ( type == '8' ) {
          Led6();
      } else if ( type == '9' ) {
          Led5();
          Led6();
      } else if ( type == 'A' ) {
          Led7_L();
          Led7_R();
      } else if ( type == 'B' ) {
          Led8();
      } else if ( type == 'C' ) {
          Led9();
      } else if ( type == 'D' ) {
          Led10();
      } else if ( type == 'E' ) {
          Led11();
      } else if ( type == 'F' ) {
          Led12();
      } else if ( type == 'G' ) {
          Led13();
      } else if ( type == 'H' ) {
          Led14();
      } else if ( type == 'I' ) {
          Led15();
      } else if ( type == 'J' ) {
          Led16();
      } else if ( type == 'K' ) {
          Led17();
      } else if ( type == 'L' ) {
          Led18();
      } else if ( type == 'M' ) {
          Led19(); 
      } else if ( type == 'N' ) {
          Led20();
      } else if ( type == 'O' ) {
          Led21();
      } else if ( type == 'P' ) {
          Led22(); 
      }
      if ( WS2812Replay ) {
        WS2812Cmd = cmd;
      } else {
        WS2812Cmd = "";
      }  
}
// 初始化显示,全黑
void LedI() {
   for(int i = 0; i < WS2812Num; i++) {
      leds[i] = CRGB::Black;
   }  
   FastLED.show();
}
// 全亮
void LedL() {
   for(int i = 0; i < WS2812Num; i++) {
      leds[i] = CRGB::White;
   }  
   FastLED.show();
}
// 显示样式：从A到B滚动
void Led0() {
   for(int i = 0; i < WS2812Num; i++) {
      leds[i] = CRGB::White;
      FastLED.show();
      delay(WS2812Dealy);
      leds[i] = CRGB::Black;
   }  
}
// 显示样式：从B到A滚动
void Led1() {
   for(int i = WS2812Num-1; i > -1; i--) {
      leds[i] = CRGB::White;
      FastLED.show();
      delay(WS2812Dealy);
      leds[i] = CRGB::Black;
   }  
}
// 显示样式：从A到B滚动,双灯
void Led2() {
   for(int i = 0; i < WS2812Num-2; i++) {
      leds[i] = CRGB::White;
      leds[i+2] = CRGB::White;
      FastLED.show();
      delay(WS2812Dealy);
      leds[i] = CRGB::Black;
      leds[i+2] = CRGB::Black;
   }  
}
// 显示样式：全带循环变色
void Led3() {
  static uint8_t hue;
  for(int i = 0; i < WS2812Num/2; i++) {   
      leds[i] = CHSV(hue++,200,WS2812Light);
      leds(WS2812Num/2,WS2812Num-1) = leds(WS2812Num/2 - 1 ,0);
      FastLED.delay(WS2812Dealy/10);
  }
}
// 显示样式：随机闪烁,颜色随机
void Led4() {
  int n = random(WS2812Num);
  leds[n].r = random8();
  leds[n].g = random8();
  leds[n].b = random8();
  FastLED.delay(WS2812Dealy);
  FastLED.show();
  leds[n] = CRGB::Black;
}
// 显示样式：从A到B滚动,颜色渐变
void Led5() {
   for(int i = 0; i < WS2812Num; i++) {
      if (tmpVar=255) { 
          leds[i] = CHSV(tmpVar--,200,WS2812Light);
      }
      if (tmpVar==0) {
          leds[i] = CHSV(tmpVar++,200,WS2812Light); 
      }
      FastLED.show();
      delay(WS2812Dealy);
      leds[i] = CRGB::Black;
   }    
}
// 显示样式：从B到A滚动,颜色渐变
void Led6() {
   for(int i = WS2812Num-1; i > -1; i--) {
      if (tmpVar=255) { 
          leds[i] = CHSV(tmpVar--,200,WS2812Light);
      }
      if (tmpVar==0) {
          leds[i] = CHSV(tmpVar++,200,WS2812Light); 
      }
      FastLED.show();
      delay(WS2812Dealy);
      leds[i] = CRGB::Black;
   }   
}
// 显示样式：左部份闪烁
void Led7_L() {
  for(int i = 0; i < WS2812Num/2; i++) {   
      leds[i] = CRGB::White;
  }
  FastLED.show();
  delay(WS2812Dealy*10);  
  for(int i = 0; i < WS2812Num/2; i++) {   
      leds[i] = CRGB::Black;
  }  
  FastLED.show();
  delay(WS2812Dealy*5);  
}
// 显示样式：右部份闪烁
void Led7_R() {
  for(int i = WS2812Num/2; i < WS2812Num; i++) {   
      leds[i] = CRGB::White;
  }
  FastLED.show();
  delay(WS2812Dealy*10);  
  for(int i = WS2812Num/2; i < WS2812Num; i++) {   
      leds[i] = CRGB::Black;
  }  
  FastLED.show();
  delay(WS2812Dealy*5);  
}
// 显示样式：从中间向两边，随机长度颜色渐变
void Led8() {
  int n = random(WS2812Num/2);
  int m = random(n)+1;
  int o = random(WS2812Dealy);
  for(int i=0; i<n; i++) {
      leds[WS2812Num/2+i] =  CHSV(100+i,200,WS2812Light);
      leds[WS2812Num/2-i] =  CHSV(100+i,200,WS2812Light);
      FastLED.show();
      delay(o);
  }
  for(int i=n+1; i>=m; i--) {
      leds[WS2812Num/2+i] = CRGB::Black;
      leds[WS2812Num/2-i] = CRGB::Black;
      FastLED.show();
      delay(o);   
  }
  delay(random(WS2812Dealy));
}
// 显示样式: 3灯左右滚动
void Led9() {
   for(int i = 0; i < WS2812Num-2; i++) {
      leds[i] = CHSV( 224, 187, 220);
      leds[i+1] =  CHSV( 224, 187, 150);
      leds[i+2] =  CHSV( 224, 187, 80);   
      FastLED.show();
      delay(WS2812Dealy);
      leds[i] = CRGB::Black;
   }    
   for(int i = WS2812Num-1; i > 1; i--) {
      leds[i] = CHSV( 224, 187, 80);
      leds[i-1] = CHSV( 224, 187, 150);    
      leds[i-2] = CHSV( 224, 187, 220);      
      FastLED.show();
      delay(WS2812Dealy);
      leds[i] = CRGB::Black;
   }     
}   
// 显示样式：随机呼吸,颜色随机
void Led10() {
  int n = random(WS2812Num);
  int h = random8();
  int s = random8();
  int d = random(WS2812Dealy);
  for(int i=0;i<250;i=i+10){
      leds[n] = CHSV(h,s,i);
      FastLED.show();
      FastLED.delay(d);
      leds[n] = CRGB::Black;
  }
  for(int i=250;i>0;i=i-10){
      leds[n] = CHSV(h,s,i);
      FastLED.show();
      FastLED.delay(d);
      leds[n] = CRGB::Black;
  }
}
// 显示样式：间隔交叉显示
void Led11() {
  for (int i=0; i<WS2812Num; i=i+2) {
      leds[i] = CRGB::White;
  }
  FastLED.show();
  FastLED.delay(WS2812Dealy);
  for (int i=0; i<WS2812Num; i=i+2) {
      leds[i] = CRGB::Black;
  }
  FastLED.show();
  FastLED.delay(WS2812Dealy);
  for (int i=1; i<WS2812Num-1; i=i+2) {
      leds[i] = CRGB::White;
  }
  FastLED.show();
  FastLED.delay(WS2812Dealy);
  for (int i=1; i<WS2812Num-1; i=i+2) {
      leds[i] = CRGB::Black;
  }
  FastLED.show();
  FastLED.delay(WS2812Dealy);
}
void Led12() {
  static uint8_t hue;
  for(int i = 0; i < WS2812Num/2; i++) {   
    // fade everything out
    leds.fadeToBlackBy(40);
    // let's set an led value
    leds[i] = CHSV(hue++,255,255);
    // now, let's first 20 leds to the top 20 leds, 
    leds(WS2812Num/2,WS2812Num-1) = leds(WS2812Num/2 - 1 ,0);
    FastLED.delay(WS2812Dealy);
  }  
}
void Led13() {
    currentPalette = RainbowStripeColors_p;   
    currentBlending = NOBLEND;
    LedGroup();
}
void Led14() {
    currentPalette = RainbowStripeColors_p;   
    currentBlending = LINEARBLEND;
    LedGroup();
}
void Led15() {
    SetupPurpleAndGreenPalette();             
    currentBlending = LINEARBLEND;
    LedGroup();
}
void Led16() {
    SetupTotallyRandomPalette();
    currentBlending = LINEARBLEND;
    LedGroup();
}
void Led17() {
    SetupBlackAndWhiteStripedPalette();
    currentBlending = NOBLEND;
    LedGroup();
}
void Led18() {
    SetupBlackAndWhiteStripedPalette();
    currentBlending = LINEARBLEND;
    LedGroup();
}
void Led19() {
    currentPalette = CloudColors_p;
    currentBlending = LINEARBLEND;
    LedGroup();
}
void Led20() {
    currentPalette = PartyColors_p;
    currentBlending = LINEARBLEND;
    LedGroup();
}
void Led21() {
    currentPalette = myRedWhiteBluePalette_p; 
    currentBlending = NOBLEND;
    LedGroup();
}
void Led22() {
    currentPalette = myRedWhiteBluePalette_p; 
    currentBlending = LINEARBLEND;  
    LedGroup();
}

void LedGroup() {
    static uint8_t startIndex = 0;
    startIndex = startIndex + 1; /* motion speed */
    FillLEDsFromPaletteColors( startIndex);
    FastLED.show();
    FastLED.delay(WS2812Dealy);  
}

void FillLEDsFromPaletteColors( uint8_t colorIndex)
{
    uint8_t brightness = 255;
    for( int i = 0; i < WS2812Num; i++) {
        leds[i] = ColorFromPalette( currentPalette, colorIndex, brightness, currentBlending);
        colorIndex += 3;
    }
}
// This function fills the palette with totally random colors.
void SetupTotallyRandomPalette()
{
    for( int i = 0; i < 16; i++) {
        currentPalette[i] = CHSV( random8(), 255, random8());
    }
}
// This function sets up a palette of black and white stripes,
// using code.  Since the palette is effectively an array of
// sixteen CRGB colors, the various fill_* functions can be used
// to set them up.
void SetupBlackAndWhiteStripedPalette()
{
    // 'black out' all 16 palette entries...
    fill_solid( currentPalette, 16, CRGB::Black);
    // and set every fourth one to white.
    currentPalette[0] = CRGB::White;
    currentPalette[4] = CRGB::White;
    currentPalette[8] = CRGB::White;
    currentPalette[12] = CRGB::White;
}
// This function sets up a palette of purple and green stripes.
void SetupPurpleAndGreenPalette()
{
    CRGB purple = CHSV( HUE_PURPLE, 255, 255);
    CRGB green  = CHSV( HUE_GREEN, 255, 255);
    CRGB black  = CRGB::Black;
    currentPalette = CRGBPalette16(
                                   green,  green,  black,  black,
                                   purple, purple, black,  black,
                                   green,  green,  black,  black,
                                   purple, purple, black,  black );
}
// This example shows how to set up a static color palette
// which is stored in PROGMEM (flash), which is almost always more
// plentiful than RAM.  A static PROGMEM palette like this
// takes up 64 bytes of flash.
const TProgmemPalette16 myRedWhiteBluePalette_p PROGMEM =
{
    CRGB::Red,
    CRGB::Gray, // 'white' is too bright compared to red and blue
    CRGB::Blue,
    CRGB::Black,
    CRGB::Red,
    CRGB::Gray,
    CRGB::Blue,
    CRGB::Black,
    CRGB::Red,
    CRGB::Red,
    CRGB::Gray,
    CRGB::Gray,
    CRGB::Blue,
    CRGB::Blue,
    CRGB::Black,
    CRGB::Black
};
// 灯组控制
void LedGroupControl(String cmd) {
  String vCmd = "";
  int lednum = 0;
  if (cmd.length() % 8 != 0) { return; }
  for(int i=0; i<cmd.length(); i=i+8) {
      vCmd = cmd.substring(i,i+8);
      lednum = HexToDec(vCmd.substring(0,2));
      if (lednum == 0) {                     
        String gCmd = vCmd.substring(2,4);        
        if (gCmd=="00") {                          // 灯带全黑
          delay(HexToDec(vCmd.substring(4,8)));    // 延时
          LedI();
        } else if (gCmd=="01") {                   // 延时设置
          delay(HexToDec(vCmd.substring(4,8))); 
        } else if (gCmd=="02") {                   // 灯带全亮
          delay(HexToDec(vCmd.substring(4,8)));    // 延时
          LedL();
        }
      } else {
        if (lednum<=WS2812Num){
          leds[lednum].r = HexToDec(vCmd.substring(2,4));
          leds[lednum].g = HexToDec(vCmd.substring(4,6));
          leds[lednum].b = HexToDec(vCmd.substring(6,8));
          FastLED.show();
        }
      }
  }
}
// 警报声
void AlertSpeak() {
  for (int i = 200; i <= 800; i=i+2) //用循环的方式将频率从200HZ 增加到800HZ
  {
    tone(SpeakerPin, i); //在四号端口输出频率
    delay(5); //该频率维持5毫秒
  }
  delay(1000);
  for (int i = 800; i >= 200; i=i-2)
  {
    tone(SpeakerPin, i);
    delay(10);
  }  
  noTone(SpeakerPin);
}

void ShowHelp() {
  Serial.println("I: 帮助信息");
  Serial.println("Jx0: 继电器开关(J10 继电器1低电平,JA1 继电器A高电平)");
  Serial.println("JZ: 查询继电器控制");
  Serial.println("BR: 控制板重启");
  Serial.println("BT: 获取温度");
  Serial.println("BS: 蜂鸣器报警");
  Serial.println("LT0: 灯带显示样式 0-P");
  Serial.println("LDFFRRGGBB: 灯带单灯控制(LD20A03282 灯带中第32灯颜色为A03282)");
  Serial.println("LLFF: 灯带亮度(LL30 灯带亮度为48)");
  Serial.println("LNFF: 灯数(LN10 LED灯16粒");
  Serial.println("LS0000: 灯带延迟(毫秒)(LS9000 灯带延迟9000毫秒)");
  Serial.println("LI: 灯带初始化");
  Serial.println("LR: 灯带循环显示");
  Serial.println("LGS - LGE: 灯带单灯组控制(两个标记之间为单个灯控制，但在同时显示)");
  Serial.println("           LG ... FFRRGGBB FFRRGGBB FFRRGGBB ...");
  Serial.println("           0001xxxx 表示延时(xxxx)");
  Serial.println("           0000xxxx 表示灯带全黑");
  Serial.println("           0002xxxx 表示灯带全亮");  
 }
