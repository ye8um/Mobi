/*
  MOBI v1 — Wi-Fi phone-controlled two-wheel robot
  Target: Seeed XIAO ESP32-C3, bare TB6612FNG, SSD1306 128x64 I2C OLED.
  Uses the REVISED pin swap discussed in the build:
  D4/GPIO6 = BIN2, D6/GPIO21 = OLED SDA, D5/GPIO7 = OLED SCL.
  TB6612 PWMA, PWMB, STBY must be tied to 3.3 V in hardware.
  5V motor supply and common ground required. Never power motors from 3V3.
  Requires Adafruit GFX + Adafruit SSD1306 libraries and esp32 board package.
  Not tested with the physical Mobi PCB; verify wiring and directions first.
*/
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

constexpr uint8_t AIN1_PIN = 3;   // XIAO D1
constexpr uint8_t AIN2_PIN = 4;   // XIAO D2
constexpr uint8_t BIN1_PIN = 5;   // XIAO D3
constexpr uint8_t BIN2_PIN = 6;   // XIAO D4 (swapped)
constexpr uint8_t OLED_SDA_PIN = 21; // XIAO D6 (swapped)
constexpr uint8_t OLED_SCL_PIN = 7;  // XIAO D5
constexpr uint8_t OLED_ADDR = 0x3C;

// Reverse either motor in software if its wheel runs backward.
constexpr bool REVERSE_LEFT = false;
constexpr bool REVERSE_RIGHT = false;
// Stop when no command has been renewed for this many milliseconds.
constexpr uint32_t DEADMAN_MS = 450;
constexpr uint32_t FRAME_MS = 70;
constexpr uint8_t DEFAULT_SPEED = 55;
constexpr uint8_t MAX_DUTY = 220; // cap speed for initial testing

const char* AP_NAME = "Mobi-Robot";
const char* AP_PASSWORD = "mobi2026drive"; // change before using in public

WebServer server(80);
Adafruit_SSD1306 oled(128, 64, &Wire, -1);
bool oledOK = false;
uint8_t speedPercent = DEFAULT_SPEED;
uint32_t lastCommandMs = 0;
uint32_t lastFrameMs = 0;
uint32_t lastBlinkMs = 0;
uint32_t blinkAtMs = 0;
char movement = 'S';
bool blinking = false;

const char PAGE[] PROGMEM = R"HTML(
<!DOCTYPE html><html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no">
<title>Mobi Controller</title><style>
:root{font-family:system-ui,sans-serif;color:#17263b;background:#f4f8fc}
body{max-width:430px;margin:0 auto;padding:24px 18px;text-align:center}
h1{margin:6px 0 0;font-size:2rem}p{color:#617083}
.panel{background:white;border:1px solid #dae3ed;border-radius:22px;padding:22px;box-shadow:0 10px 35px #142c4814}
#status{padding:12px;border-radius:12px;background:#eaf3ff;font-weight:700;letter-spacing:.08em}
.pad{display:grid;grid-template-columns:repeat(3,1fr);gap:12px;margin-top:20px}
button{border:0;border-radius:18px;background:#1663b4;color:white;font-size:30px;height:75px;touch-action:none;user-select:none;-webkit-user-select:none}
button:active,.held{background:#0d396f;transform:scale(.97)}
.stop{background:#d14a4a}.blank{visibility:hidden}
label{display:flex;justify-content:space-between;margin:20px 0 10px}input{width:100%}
.small{font-size:13px;margin-top:20px}
</style></head><body><h1>◉ MOBI</h1><p>Wi-Fi robot controller</p>
<div class="panel"><div id="status">STOPPED</div>
<div class="pad"><div class="blank"></div><button data-dir="F">↑</button><div class="blank"></div>
<button data-dir="L">←</button><button class="stop" id="stop">■</button><button data-dir="R">→</button>
<div class="blank"></div><button data-dir="B">↓</button><div class="blank"></div></div>
<label for="speed">Speed <strong><span id="speedText">55</span>%</strong></label>
<input type="range" id="speed" min="0" max="100" value="55" step="5">
<p class="small">Hold a direction to move. Release to stop.<br>Use only on a clear, flat floor; there is no edge sensor.</p></div>
<script>
let held='S', timer=null;
const label=document.getElementById('status'),speed=document.getElementById('speed');
const names={F:'FORWARD',B:'REVERSE',L:'LEFT',R:'RIGHT',S:'STOPPED'};
function transmit(dir){fetch('/cmd?dir='+dir+'&speed='+speed.value,{cache:'no-store'}).catch(()=>{});}
function stop(){held='S';clearInterval(timer);timer=null;label.textContent='STOPPED';transmit('S');document.querySelectorAll('button').forEach(b=>b.classList.remove('held'));}
function start(dir,button){stop();held=dir;label.textContent=names[dir];button.classList.add('held');transmit(dir);timer=setInterval(()=>transmit(held),160);}
document.querySelectorAll('button[data-dir]').forEach(button=>{
 button.addEventListener('pointerdown',e=>{e.preventDefault();button.setPointerCapture(e.pointerId);start(button.dataset.dir,button);});
 button.addEventListener('pointerup',e=>{e.preventDefault();stop();});
 button.addEventListener('pointercancel',stop);
 button.addEventListener('lostpointercapture',stop);
});
document.getElementById('stop').addEventListener('click',stop);
speed.addEventListener('input',()=>{document.getElementById('speedText').textContent=speed.value;if(held!=='S')transmit(held);});
window.addEventListener('blur',stop);document.addEventListener('visibilitychange',()=>{if(document.hidden)stop();});
window.addEventListener('pagehide',stop);
</script></body></html>
)HTML";

void setOneMotor(uint8_t pin1, uint8_t pin2, int requested, bool invert) {
  if (invert) requested = -requested;
  requested = constrain(requested, -255, 255);
  if (requested > 0) {
    analogWrite(pin1, requested); analogWrite(pin2, 0);
  } else if (requested < 0) {
    analogWrite(pin1, 0); analogWrite(pin2, -requested);
  } else {
    analogWrite(pin1, 0); analogWrite(pin2, 0);
  }
}

void drive(char dir, uint8_t percent) {
  const int duty = map(constrain(percent, 0, 100), 0, 100, 0, MAX_DUTY);
  int l = 0, r = 0;
  switch (dir) {
    case 'F': l = duty; r = duty; break;
    case 'B': l = -duty; r = -duty; break;
    case 'L': l = -duty; r = duty; break;
    case 'R': l = duty; r = -duty; break;
    default: dir = 'S'; break;
  }
  setOneMotor(AIN1_PIN, AIN2_PIN, l, REVERSE_LEFT);
  setOneMotor(BIN1_PIN, BIN2_PIN, r, REVERSE_RIGHT);
  movement = dir;
}

void handleCmd() {
  if (!server.hasArg("dir")) { server.send(400,"text/plain","Missing dir"); return; }
  String arg = server.arg("dir");
  if (arg.length()!=1 || String("FBLRS").indexOf(arg.charAt(0))<0) {
    server.send(400,"text/plain","Bad direction"); return;
  }
  const char dir = arg.charAt(0);
  if (server.hasArg("speed")) speedPercent = constrain(server.arg("speed").toInt(),0,100);
  lastCommandMs = millis();
  drive(dir,speedPercent);
  server.sendHeader("Cache-Control","no-store");
  server.send(200,"text/plain","OK");
}

void drawEyes() {
  if (!oledOK) return;
  oled.clearDisplay();
  const bool moving = movement != 'S';
  if (blinking) {
    oled.drawFastHLine(24, 32, 28, SSD1306_WHITE);
    oled.drawFastHLine(76, 32, 28, SSD1306_WHITE);
  } else {
    oled.fillRoundRect(23, 15, 28, 36, 11, SSD1306_WHITE);
    oled.fillRoundRect(77, 15, 28, 36, 11, SSD1306_WHITE);
    int offset = movement == 'L' ? -5 : movement == 'R' ? 5 : 0;
    oled.fillCircle(36+offset,33,5,SSD1306_BLACK);
    oled.fillCircle(90+offset,33,5,SSD1306_BLACK);
  }
  if (moving) oled.drawPixel(63, 56, SSD1306_WHITE);
  oled.display();
}

void setup() {
  Serial.begin(115200);
  pinMode(AIN1_PIN,OUTPUT); pinMode(AIN2_PIN,OUTPUT);
  pinMode(BIN1_PIN,OUTPUT); pinMode(BIN2_PIN,OUTPUT);
  drive('S',0);
  Wire.begin(OLED_SDA_PIN,OLED_SCL_PIN);
  oledOK = oled.begin(SSD1306_SWITCHCAPVCC,OLED_ADDR);
  if (!oledOK) Serial.println("OLED not found; verify 0x3C and swapped SDA/SCL wiring");
  else {oled.clearDisplay(); oled.setTextSize(1); oled.setTextColor(SSD1306_WHITE); oled.setCursor(25,26); oled.print("MOBI STARTING"); oled.display();}
  WiFi.mode(WIFI_AP);
  bool apOK = WiFi.softAP(AP_NAME,AP_PASSWORD);
  Serial.printf("AP started: %s | IP: %s\n",apOK?"YES":"NO",WiFi.softAPIP().toString().c_str());
  server.on("/",HTTP_GET,[]{server.send_P(200,"text/html",PAGE);});
  server.on("/cmd",HTTP_GET,handleCmd);
  server.onNotFound([]{server.send(404,"text/plain","Not found");});
  server.begin();
  lastCommandMs = millis(); blinkAtMs = millis()+2900;
}

void loop() {
  server.handleClient();
  const uint32_t now = millis();
  if (movement != 'S' && now-lastCommandMs > DEADMAN_MS) drive('S',0);
  if (!blinking && (int32_t)(now-blinkAtMs)>=0) {blinking=true; lastBlinkMs=now;}
  if (blinking && now-lastBlinkMs>=140) {blinking=false; blinkAtMs=now+3000+(now%2500);}
  if (now-lastFrameMs>=FRAME_MS) {lastFrameMs=now; drawEyes();}
}
