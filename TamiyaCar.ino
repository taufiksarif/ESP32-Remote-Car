/*
  Tamiya Car - ESP32 AP mode, four-wheel-drive craft-stick buggy

  Hardware:
    - ESP32 DevKit
    - 4 BO gear motors with wheels
    - 2 x TB6612FNG dual H-bridge modules (one motor per channel)
    - Separate motor battery matched to the motors and driver current limits
    - Common ground between ESP32, both drivers, and motor battery

  The pin assignment below suits a classic ESP32-WROOM DevKit. Change pins if
  your board differs. Check each motor's stall current against the driver.
  Do not power motors from the ESP32 3V3 pin.
*/

#include <WiFi.h>
#include <WebServer.h>

const char *AP_SSID = "Tamiya Car";
const char *AP_PASSWORD = "12345678";

// TB6612 #1: left front and left rear. TB6612 #2: right front and right rear.
// AIN1/AIN2 and BIN1/BIN2 are direction pins; PWMA/PWMB are speed pins.
const uint8_t LF_IN1 = 16, LF_IN2 = 17, LF_PWM = 18;
const uint8_t LR_IN1 = 19, LR_IN2 = 21, LR_PWM = 22;
const uint8_t RF_IN1 = 25, RF_IN2 = 26, RF_PWM = 27;
const uint8_t RR_IN1 = 32, RR_IN2 = 33, RR_PWM = 23;
const uint8_t STBY_LEFT = 4;
const uint8_t STBY_RIGHT = 13;
const uint8_t WIFI_LED_PIN = 2; // DOIT DevKit V1 onboard LED, typically active HIGH
const uint32_t WIFI_LED_BLINK_MS = 500;

// Flip a motor's value to -1 if that wheel spins backward during a forward test.
const int8_t LF_POLARITY = 1;
const int8_t LR_POLARITY = 1;
const int8_t RF_POLARITY = -1;
const int8_t RR_POLARITY = -1;

const uint32_t COMMAND_TIMEOUT_MS = 700;
WebServer server(80);
int currentSpeed = 191; // PWM equivalent of the default 75% UI setting
int turnStrength = 100; // 0 = straight / no differential, 100 = pivot turn
int leftOutputSpeed = 0;
int rightOutputSpeed = 0;
int leftTargetSpeed = 0;
int rightTargetSpeed = 0;
uint32_t lastRampAt = 0;
uint32_t lastCommandAt = 0;
bool moving = false;

const char PAGE[] PROGMEM = R"HTML(
<!doctype html><html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no">
<meta name="theme-color" content="#101820"><title>Tamiya Car</title>
<style>
:root{color-scheme:dark;--bg:#101820;--panel:#1a2a33;--line:#344953;--ink:#f5f7f2;--muted:#aab9bb;--lime:#c2f36b;--orange:#ffad62}
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
html,body{width:100%;min-height:100%;margin:0}
body{background:radial-gradient(ellipse at 50% -35%,#304954,var(--bg) 68%);color:var(--ink);font:600 16px/1.25 system-ui,-apple-system,sans-serif;min-height:100svh;display:grid;place-items:center;padding:clamp(10px,2.4vw,28px);user-select:none;overflow-x:hidden}
.app{width:min(100%,1180px);display:grid;grid-template-columns:minmax(0,1.2fr) minmax(220px,.8fr);grid-template-rows:auto minmax(0,1fr) auto;gap:clamp(12px,2.2vw,26px);align-items:center}
header{grid-column:1/-1;display:flex;align-items:center;justify-content:space-between;gap:16px;border-bottom:1px solid var(--line);padding:0 2px clamp(9px,1.5vh,15px)}
.brand{display:flex;align-items:center;gap:11px;min-width:0}.badge{width:clamp(34px,5vh,42px);height:clamp(34px,5vh,42px);flex:none;border-radius:12px;background:var(--lime);color:#15200e;display:grid;place-items:center;font-size:20px}.title{font-size:clamp(15px,2.2vh,19px);letter-spacing:.08em;text-transform:uppercase}.sub{font-size:clamp(10px,1.5vh,12px);color:var(--muted);font-weight:500;margin-top:3px}
.status{font-size:clamp(11px,1.6vh,13px);color:var(--muted);display:flex;align-items:center;gap:8px;white-space:nowrap}.dot{width:9px;height:9px;flex:none;border-radius:50%;background:var(--orange);box-shadow:0 0 10px #ff9c4670}.dot.ok{background:var(--lime);box-shadow:0 0 10px #b7ef5570}
.pad{width:100%;height:min(67svh,510px);min-height:250px;background:linear-gradient(145deg,#ffffff0b,#ffffff04);border:1px solid var(--line);border-radius:clamp(20px,3vw,30px);padding:clamp(10px,2vw,20px);display:grid;grid-template-columns:repeat(3,minmax(0,1fr));grid-template-rows:repeat(3,minmax(0,1fr));gap:clamp(8px,1.5vw,15px)}
button{font:inherit;color:var(--ink);border:1px solid #4a606a;background:linear-gradient(145deg,#2a404a,#1c2c35);border-radius:clamp(14px,2vw,21px);box-shadow:0 4px 0 #0a1116,0 9px 18px #0003;display:grid;place-items:center;cursor:pointer;min-width:0;min-height:0;padding:5px;touch-action:none;transition:transform .08s,border-color .08s,background .08s,box-shadow .08s}
button>span{display:flex;flex-direction:column;align-items:center;justify-content:center;gap:clamp(2px,.7vh,7px);line-height:1.1}.arrow{font-size:clamp(28px,7vh,48px);line-height:1}.label{font-size:clamp(9px,1.5vh,12px);letter-spacing:.1em;color:#c0cece;white-space:nowrap}.drive:active,.drive.held{transform:translateY(3px);box-shadow:0 1px 0 #0a1116;background:#354e53;border-color:var(--lime)}
.up{grid-column:2;grid-row:1}.left{grid-column:1;grid-row:2}.stop{grid-column:2;grid-row:2;background:linear-gradient(145deg,#553b2d,#39271f);border-color:#936443;color:#ffe0c2;box-shadow:0 4px 0 #241913}.stop .label{color:#ffd3ae}.stop:active{transform:translateY(3px);box-shadow:0 1px 0 #241913}.right{grid-column:3;grid-row:2}.down{grid-column:2;grid-row:3}
.center{height:min(67svh,510px);display:flex;flex-direction:column;align-items:center;justify-content:center;gap:clamp(16px,4vh,32px);padding:clamp(12px,2vw,24px);border:1px solid var(--line);border-radius:clamp(20px,3vw,30px);background:linear-gradient(155deg,#ffffff09,#ffffff03)}
.car{width:clamp(112px,25vh,190px);aspect-ratio:1;flex:none;display:grid;place-items:center;border:1px solid #405761;border-radius:50%;background:radial-gradient(circle,#314852,#142129 72%);position:relative;box-shadow:inset 0 0 30px #0004,0 12px 28px #0003}.car:before,.car:after{content:"";position:absolute;left:8%;right:8%;height:9px;border-radius:8px;background:var(--lime);opacity:.8}.car:before{top:25%}.car:after{bottom:25%}.car svg{width:56%;z-index:1}
.speed{width:min(100%,340px);background:#0d171c80;border:1px solid var(--line);padding:clamp(13px,2.2vh,20px) clamp(15px,2.2vw,24px);border-radius:18px}.speedrow{display:flex;align-items:center;justify-content:space-between;color:var(--muted);font-size:clamp(12px,1.7vh,14px);letter-spacing:.08em;margin-bottom:4px}.speedrow strong{color:var(--lime);font-size:1.1em;font-variant-numeric:tabular-nums}.sliderGroup+.sliderGroup{margin-top:9px;padding-top:8px;border-top:1px solid #344953}.sliderGroup input{display:block;width:100%;height:25px;margin:0;accent-color:var(--lime);cursor:pointer;touch-action:pan-x}.rangeEnds{display:flex;justify-content:space-between;color:var(--muted);font-size:10px;font-weight:500;margin-top:-2px}.note{grid-column:1/-1;font-size:clamp(10px,1.5vh,12px);color:var(--muted);text-align:center;font-weight:500;letter-spacing:.02em}
@media(orientation:portrait){body{place-items:start center;padding:14px}.app{max-width:620px;grid-template-columns:minmax(0,1fr) minmax(0,1fr);grid-template-rows:auto auto auto auto;gap:14px}.pad{grid-column:1/-1;grid-row:3;height:clamp(270px,43svh,430px);min-height:0}.center{grid-column:1/-1;grid-row:2;height:auto;min-height:128px;flex-direction:row;justify-content:space-evenly;gap:14px;padding:12px 16px}.car{width:clamp(82px,18vw,112px)}.speed{width:min(65%,360px)}.note{grid-row:4}}
@media(max-height:480px) and (orientation:landscape){body{padding:8px 16px}.app{gap:9px 18px}.pad{height:min(68svh,340px);min-height:0;padding:9px;gap:8px}.center{height:min(68svh,340px);gap:12px;padding:10px}.car{width:clamp(88px,20vh,125px)}.speed{padding:9px 14px}.speedrow{margin-bottom:4px}input{height:24px}.note{font-size:10px}}
</style></head><body><main class="app">
<header><div class="brand"><div class="badge">✦</div><div><div class="title">Tamiya Buggy</div><div class="sub">Four-wheel drive · live control</div></div></div><div class="status"><i class="dot" id="dot"></i><span id="status">Connecting…</span></div></header>
<section class="pad" aria-label="Driving controls">
<button class="drive up" data-cmd="forward" aria-label="Forward"><span><span class="arrow">↑</span><span class="label">FORWARD</span></span></button>
<button class="drive left" data-cmd="left" aria-label="Turn left"><span><span class="arrow">←</span><span class="label">LEFT</span></span></button>
<button class="stop" id="stop" aria-label="Stop"><span><span class="arrow">■</span><span class="label">STOP</span></span></button>
<button class="drive right" data-cmd="right" aria-label="Turn right"><span><span class="arrow">→</span><span class="label">RIGHT</span></span></button>
<button class="drive down" data-cmd="reverse" aria-label="Reverse"><span><span class="arrow">↓</span><span class="label">REVERSE</span></span></button></section>
<section class="center"><div class="car" aria-hidden="true"><svg viewBox="0 0 100 130"><path d="M28 8h44l8 21 11 19v51l-11 19H20L9 99V48l11-19z" fill="#b7ef55"/><path d="M30 25h40l8 21H22zm-8 51h56v22H22z" fill="#18262d"/><circle cx="27" cy="57" r="5" fill="#ff9c46"/><circle cx="73" cy="57" r="5" fill="#ff9c46"/></svg></div><div class="speed"><div class="sliderGroup"><div class="speedrow"><span>SPEED</span><strong id="speedval">75%</strong></div><input id="speed" type="range" min="25" max="100" value="75" aria-label="Speed"></div><div class="sliderGroup"><div class="speedrow"><span>TURN STRENGTH</span><strong id="turnval">100%</strong></div><input id="turn" type="range" min="0" max="100" value="100" aria-label="Turn strength"><div class="rangeEnds"><span>STRAIGHT</span><span>PIVOT</span></div></div></div></section>
<div class="note">Press and hold to drive · release for a smooth stop</div>
</main><script>
const statusEl=document.querySelector('#status'),dot=document.querySelector('#dot'),speed=document.querySelector('#speed'),turn=document.querySelector('#turn');let active=null,timer=null;
async function send(cmd){try{const r=await fetch(`/cmd?move=${cmd}&speed=${speed.value}&turn=${turn.value}`,{cache:'no-store'});if(!r.ok)throw 0;statusEl.textContent='Connected';dot.classList.add('ok')}catch(e){statusEl.textContent='Reconnecting';dot.classList.remove('ok')}}
function stop(){if(timer){clearInterval(timer);timer=null}document.querySelectorAll('.held').forEach(b=>b.classList.remove('held'));if(active){active=null;send('stop')}}
document.querySelectorAll('.drive').forEach(b=>{b.addEventListener('pointerdown',e=>{e.preventDefault();stop();active=b.dataset.cmd;b.classList.add('held');send(active);timer=setInterval(()=>active&&send(active),180)});['pointerup','pointercancel','pointerleave','lostpointercapture'].forEach(ev=>b.addEventListener(ev,stop))});
document.querySelector('#stop').addEventListener('pointerdown',e=>{e.preventDefault();stop();send('stop')});window.addEventListener('blur',stop);document.addEventListener('visibilitychange',()=>document.hidden&&stop());
speed.addEventListener('input',()=>{document.querySelector('#speedval').textContent=speed.value+'%';if(active)send(active)});turn.addEventListener('input',()=>{document.querySelector('#turnval').textContent=turn.value+'%';if(active)send(active)});send('stop');
</script></body></html>
)HTML";

struct Motor { uint8_t in1, in2, pwm; int8_t polarity; };
Motor motors[] = {
  {LF_IN1, LF_IN2, LF_PWM, LF_POLARITY},
  {LR_IN1, LR_IN2, LR_PWM, LR_POLARITY},
  {RF_IN1, RF_IN2, RF_PWM, RF_POLARITY},
  {RR_IN1, RR_IN2, RR_PWM, RR_POLARITY}
};

void setMotor(const Motor &m, int signedSpeed) {
  signedSpeed *= m.polarity;
  if (signedSpeed == 0) {
    digitalWrite(m.in1, LOW);
    digitalWrite(m.in2, LOW);
    analogWrite(m.pwm, 0);
    return;
  }
  digitalWrite(m.in1, signedSpeed > 0 ? HIGH : LOW);
  digitalWrite(m.in2, signedSpeed > 0 ? LOW : HIGH);
  analogWrite(m.pwm, abs(signedSpeed));
}

void drive(int leftSpeed, int rightSpeed) {
  leftTargetSpeed = constrain(leftSpeed, -255, 255);
  rightTargetSpeed = constrain(rightSpeed, -255, 255);
  if (leftTargetSpeed != 0 || rightTargetSpeed != 0 || leftOutputSpeed != 0 || rightOutputSpeed != 0) {
    digitalWrite(STBY_LEFT, HIGH);
    digitalWrite(STBY_RIGHT, HIGH);
  }
  moving = leftTargetSpeed != 0 || rightTargetSpeed != 0 || leftOutputSpeed != 0 || rightOutputSpeed != 0;
}

void stopCar() {
  leftTargetSpeed = rightTargetSpeed = 0;
  leftOutputSpeed = rightOutputSpeed = 0;
  for (const Motor &m : motors) setMotor(m, 0);
  digitalWrite(STBY_LEFT, LOW);
  digitalWrite(STBY_RIGHT, LOW);
  moving = false;
}

int rampToward(int current, int target) {
  const int RAMP_STEP = 12;
  if (current < target) return min(current + RAMP_STEP, target);
  if (current > target) return max(current - RAMP_STEP, target);
  return current;
}

void updateMotorRamp() {
  if (!moving) return;
  if (millis() - lastRampAt < 15) return;
  lastRampAt = millis();
  leftOutputSpeed = rampToward(leftOutputSpeed, leftTargetSpeed);
  rightOutputSpeed = rampToward(rightOutputSpeed, rightTargetSpeed);
  setMotor(motors[0], leftOutputSpeed);
  setMotor(motors[1], leftOutputSpeed);
  setMotor(motors[2], rightOutputSpeed);
  setMotor(motors[3], rightOutputSpeed);
  moving = leftTargetSpeed != 0 || rightTargetSpeed != 0 || leftOutputSpeed != 0 || rightOutputSpeed != 0;
  if (!moving) stopCar();
}

void handleCommand() {
  if (server.hasArg("speed")) currentSpeed = constrain(server.arg("speed").toInt(), 25, 100) * 255 / 100;
  if (server.hasArg("turn")) turnStrength = constrain(server.arg("turn").toInt(), 0, 100);
  String move = server.arg("move");
  int innerSpeed = currentSpeed * (100 - 2 * turnStrength) / 100;
  if (move == "forward") drive(currentSpeed, currentSpeed);
  else if (move == "reverse") drive(-currentSpeed, -currentSpeed);
  else if (move == "left") drive(innerSpeed, currentSpeed);
  else if (move == "right") drive(currentSpeed, innerSpeed);
  else drive(0, 0);
  lastCommandAt = millis();
  server.send(200, "text/plain", "OK");
}

void setup() {
  Serial.begin(115200);
  pinMode(WIFI_LED_PIN, OUTPUT);
  digitalWrite(WIFI_LED_PIN, LOW);
  uint8_t pins[] = {LF_IN1, LF_IN2, LF_PWM, LR_IN1, LR_IN2, LR_PWM,
                    RF_IN1, RF_IN2, RF_PWM, RR_IN1, RR_IN2, RR_PWM,
                    STBY_LEFT, STBY_RIGHT};
  for (uint8_t pin : pins) pinMode(pin, OUTPUT);
  stopCar();

  WiFi.mode(WIFI_AP);
  IPAddress apIP(10, 10, 10, 10);
  IPAddress gateway(10, 10, 10, 10);
  IPAddress subnet(255, 255, 255, 0);
  if (!WiFi.softAPConfig(apIP, gateway, subnet)) {
    Serial.println("Warning: failed to configure AP IP address");
  }
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  Serial.print("Join Wi-Fi: "); Serial.println(AP_SSID);
  Serial.print("Open: http://"); Serial.println(WiFi.softAPIP());

  server.on("/", HTTP_GET, []() { server.send_P(200, "text/html", PAGE); });
  server.on("/cmd", HTTP_GET, handleCommand);
  server.onNotFound([]() { server.send(404, "text/plain", "Not found"); });
  server.begin();
  lastCommandAt = millis();
}

void loop() {
  server.handleClient();
  if (moving && millis() - lastCommandAt > COMMAND_TIMEOUT_MS) stopCar();
  updateMotorRamp();

  static uint32_t lastLedToggleAt = 0;
  static bool ledOn = false;
  if (WiFi.softAPgetStationNum() > 0) {
    ledOn = true;
    digitalWrite(WIFI_LED_PIN, HIGH);
  } else if (millis() - lastLedToggleAt >= WIFI_LED_BLINK_MS) {
    lastLedToggleAt = millis();
    ledOn = !ledOn;
    digitalWrite(WIFI_LED_PIN, ledOn ? HIGH : LOW);
  }
}
