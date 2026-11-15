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

// Flip a motor's value to -1 if that wheel spins backward during a forward test.
const int8_t LF_POLARITY = 1;
const int8_t LR_POLARITY = 1;
const int8_t RF_POLARITY = -1;
const int8_t RR_POLARITY = -1;

const uint32_t COMMAND_TIMEOUT_MS = 700;
WebServer server(80);
int currentSpeed = 190; // 0..255
uint32_t lastCommandAt = 0;
bool moving = false;

const char PAGE[] PROGMEM = R"HTML(
<!doctype html><html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no">
<meta name="theme-color" content="#101820"><title>Tamiya Car</title>
<style>
:root{color-scheme:dark;--bg:#101820;--panel:#192832;--line:#30434d;--ink:#f3f6f4;--muted:#a8b7b9;--lime:#b7ef55;--orange:#ff9c46}
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent;touch-action:manipulation}
body{margin:0;background:radial-gradient(ellipse at 50% -40%,#29404a,var(--bg) 65%);color:var(--ink);font:600 16px system-ui,-apple-system,sans-serif;min-height:100svh;display:grid;place-items:center;padding:clamp(10px,2.5vw,28px);user-select:none}
.app{width:min(100%,1050px);display:grid;grid-template-columns:1fr minmax(160px,.65fr) 1fr;gap:clamp(12px,3vw,36px);align-items:center}
header{grid-column:1/-1;display:flex;align-items:center;justify-content:space-between;border-bottom:1px solid var(--line);padding-bottom:12px}
.brand{display:flex;align-items:center;gap:11px}.badge{width:38px;height:38px;border-radius:12px;background:var(--lime);color:#15200e;display:grid;place-items:center;font-size:20px}.title{font-size:18px;letter-spacing:.08em;text-transform:uppercase}.sub{font-size:12px;color:var(--muted);font-weight:500;margin-top:2px}
.status{font-size:12px;color:var(--muted);display:flex;align-items:center;gap:7px}.dot{width:8px;height:8px;border-radius:50%;background:var(--orange);box-shadow:0 0 10px #ff9c4670}.dot.ok{background:var(--lime);box-shadow:0 0 10px #b7ef5570}
.pad{background:#ffffff08;border:1px solid var(--line);border-radius:25px;padding:clamp(10px,2vw,20px);display:grid;grid-template-columns:repeat(3,1fr);grid-template-rows:repeat(3,minmax(64px,1fr));gap:clamp(7px,1.5vw,14px);aspect-ratio:1.08}
button{font:inherit;color:var(--ink);border:1px solid #465b64;background:linear-gradient(145deg,#263943,#1b2a32);border-radius:19px;box-shadow:0 5px 0 #0a1116,0 10px 18px #0003;display:grid;place-items:center;cursor:pointer;min-width:0;touch-action:none;transition:transform .08s,border-color .08s,background .08s}
button .arrow{font-size:clamp(24px,5vw,40px);line-height:1}.label{font-size:10px;letter-spacing:.12em;color:var(--muted);margin-top:2px}.drive:active,.drive.held{transform:translateY(4px);box-shadow:0 1px 0 #0a1116;background:#384d4f;border-color:var(--lime)}
.up{grid-column:2;grid-row:1}.left{grid-column:1;grid-row:2}.stop{grid-column:2;grid-row:2;background:#462e26;border-color:#765039;color:#ffd3ae;box-shadow:0 5px 0 #241913}.stop:active{transform:translateY(4px);box-shadow:0 1px 0 #241913}.right{grid-column:3;grid-row:2}.down{grid-column:2;grid-row:3}
.center{display:flex;flex-direction:column;align-items:center;gap:12px}.car{width:min(100%,170px);aspect-ratio:1;display:grid;place-items:center;border:1px solid var(--line);border-radius:50%;background:radial-gradient(circle,#263942,#142129 70%);position:relative}.car:before,.car:after{content:"";position:absolute;left:8%;right:8%;height:10px;border-radius:8px;background:var(--lime);opacity:.8}.car:before{top:25%}.car:after{bottom:25%}.car svg{width:56%;z-index:1}.speed{width:100%;background:var(--panel);border:1px solid var(--line);padding:12px 14px;border-radius:16px}.speedrow{display:flex;justify-content:space-between;color:var(--muted);font-size:12px;margin-bottom:8px}input{width:100%;accent-color:var(--lime)}.note{font-size:11px;color:var(--muted);text-align:center;font-weight:500}
@media(orientation:portrait){body{place-items:start center}.app{grid-template-columns:1fr 1fr;max-width:620px}.center{grid-column:1/-1;grid-row:2;flex-direction:row}.car{width:100px;flex:none}.speed{max-width:320px}.pad{grid-row:3;grid-column:1/-1;max-height:50vh}}
@media(max-height:430px) and (orientation:landscape){body{padding:8px}.app{gap:12px}header{padding-bottom:6px}.pad{padding:9px;gap:7px;grid-template-rows:repeat(3,minmax(44px,1fr))}.car{max-width:125px}.speed{padding:8px 12px}}
</style></head><body><main class="app">
<header><div class="brand"><div class="badge">✦</div><div><div class="title">Tamiya Buggy</div><div class="sub">Four-wheel drive · live control</div></div></div><div class="status"><i class="dot" id="dot"></i><span id="status">Connecting…</span></div></header>
<section class="pad" aria-label="Driving controls">
<button class="drive up" data-cmd="forward" aria-label="Forward"><span><span class="arrow">↑</span><span class="label">FORWARD</span></span></button>
<button class="drive left" data-cmd="left" aria-label="Turn left"><span><span class="arrow">←</span><span class="label">LEFT</span></span></button>
<button class="stop" id="stop" aria-label="Stop"><span><span class="arrow">■</span><span class="label">STOP</span></span></button>
<button class="drive right" data-cmd="right" aria-label="Turn right"><span><span class="arrow">→</span><span class="label">RIGHT</span></span></button>
<button class="drive down" data-cmd="reverse" aria-label="Reverse"><span><span class="arrow">↓</span><span class="label">REVERSE</span></span></button></section>
<section class="center"><div class="car" aria-hidden="true"><svg viewBox="0 0 100 130"><path d="M28 8h44l8 21 11 19v51l-11 19H20L9 99V48l11-19z" fill="#b7ef55"/><path d="M30 25h40l8 21H22zm-8 51h56v22H22z" fill="#18262d"/><circle cx="27" cy="57" r="5" fill="#ff9c46"/><circle cx="73" cy="57" r="5" fill="#ff9c46"/></svg></div><div class="speed"><div class="speedrow"><span>SPEED</span><strong id="speedval">75%</strong></div><input id="speed" type="range" min="25" max="100" value="75" aria-label="Speed"></div></section>
<div class="note">Press and hold to drive · release to stop</div>
</main><script>
const statusEl=document.querySelector('#status'),dot=document.querySelector('#dot'),speed=document.querySelector('#speed');let active=null,timer=null;
async function send(cmd){try{const r=await fetch(`/cmd?move=${cmd}&speed=${speed.value}`,{cache:'no-store'});if(!r.ok)throw 0;statusEl.textContent='Connected';dot.classList.add('ok')}catch(e){statusEl.textContent='Reconnecting';dot.classList.remove('ok')}}
function stop(){if(timer){clearInterval(timer);timer=null}document.querySelectorAll('.held').forEach(b=>b.classList.remove('held'));if(active){active=null;send('stop')}}
document.querySelectorAll('.drive').forEach(b=>{b.addEventListener('pointerdown',e=>{e.preventDefault();stop();active=b.dataset.cmd;b.classList.add('held');send(active);timer=setInterval(()=>active&&send(active),180)});['pointerup','pointercancel','pointerleave','lostpointercapture'].forEach(ev=>b.addEventListener(ev,stop))});
document.querySelector('#stop').addEventListener('pointerdown',e=>{e.preventDefault();stop();send('stop')});window.addEventListener('blur',stop);document.addEventListener('visibilitychange',()=>document.hidden&&stop());
speed.addEventListener('input',()=>{document.querySelector('#speedval').textContent=speed.value+'%';if(active)send(active)});send('stop');
</script></body></html>
)HTML";

struct Motor { uint8_t in1, in2, pwm; int8_t polarity; };
Motor motors[] = {
  {LF_IN1, LF_IN2, LF_PWM, LF_POLARITY},
  {LR_IN1, LR_IN2, LR_PWM, LR_POLARITY},
  {RF_IN1, RF_IN2, RF_PWM, RF_POLARITY},
  {RR_IN1, RR_IN2, RR_PWM, RR_POLARITY}
};

void setMotor(const Motor &m, int direction, int speed) {
  direction *= m.polarity;
  if (direction == 0 || speed == 0) {
    digitalWrite(m.in1, LOW);
    digitalWrite(m.in2, LOW);
    analogWrite(m.pwm, 0);
    return;
  }
  digitalWrite(m.in1, direction > 0 ? HIGH : LOW);
  digitalWrite(m.in2, direction > 0 ? LOW : HIGH);
  analogWrite(m.pwm, speed);
}

void drive(int leftDirection, int rightDirection) {
  digitalWrite(STBY_LEFT, HIGH);
  digitalWrite(STBY_RIGHT, HIGH);
  setMotor(motors[0], leftDirection, currentSpeed);
  setMotor(motors[1], leftDirection, currentSpeed);
  setMotor(motors[2], rightDirection, currentSpeed);
  setMotor(motors[3], rightDirection, currentSpeed);
  moving = leftDirection != 0 || rightDirection != 0;
}

void stopCar() {
  for (const Motor &m : motors) setMotor(m, 0, 0);
  digitalWrite(STBY_LEFT, LOW);
  digitalWrite(STBY_RIGHT, LOW);
  moving = false;
}

void handleCommand() {
  if (server.hasArg("speed")) currentSpeed = constrain(server.arg("speed").toInt(), 0, 255);
  String move = server.arg("move");
  if (move == "forward") drive(1, 1);
  else if (move == "reverse") drive(-1, -1);
  else if (move == "left") drive(-1, 1);
  else if (move == "right") drive(1, -1);
  else stopCar();
  lastCommandAt = millis();
  server.send(200, "text/plain", "OK");
}

void setup() {
  Serial.begin(115200);
  uint8_t pins[] = {LF_IN1, LF_IN2, LF_PWM, LR_IN1, LR_IN2, LR_PWM,
                    RF_IN1, RF_IN2, RF_PWM, RR_IN1, RR_IN2, RR_PWM,
                    STBY_LEFT, STBY_RIGHT};
  for (uint8_t pin : pins) pinMode(pin, OUTPUT);
  stopCar();

  WiFi.mode(WIFI_AP);
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
}
