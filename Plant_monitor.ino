/*
  LED controller + AS7265x spectral sampling (9600 baud) with "scan" jobs
  - Type `scan` or `scan 6` in Serial Monitor to record N full RGB cycles then stop.
  - Default N=6 if omitted.
  - Use 9600 baud, No line ending.

  Pins: R=D9, G=D10, B=D11 (each via 220–330Ω)
  Common Cathode -> invertPWM=false
  Common Anode   -> invertPWM=true
*/

#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include "SparkFun_AS7265X.h"

// -------- Pin map --------
const uint8_t PIN_R = 9;
const uint8_t PIN_G = 10;
const uint8_t PIN_B = 11;

// -------- Config --------
bool     invertPWM = false;  // true for Common Anode, false for Common Cathode
uint8_t  brightness = 255;   // 0..255
uint32_t cycleMs    = 1500;  // full R,G,B cycle length
uint16_t settleMs   = 60;    // per-color settle time before reading

// -------- Sensor --------
AS7265X triad;
bool sensorOk = false;
uint16_t itMs = 100;         // desired integration time (ms) -> cycles

// -------- State machine --------
enum Mode { M_AUTO, M_MANUAL_OFF, M_MANUAL_R, M_MANUAL_G, M_MANUAL_B };
Mode mode = M_AUTO;

enum Step { S_R, S_G, S_B };
Step step = S_R;

uint32_t nextAt = 0;
uint32_t seq    = 0;

// Per-slot timing state (made global so we can reset on commands)
bool      settled    = false;
uint32_t  slotStart  = 0;

// -------- "Scan job" control --------
// A "scan" is N full RGB cycles (R->G->B). We count a cycle when the B sample is taken.
bool     scanActive       = false;
uint16_t scanTargetCycles = 0;
uint16_t scanDoneCycles   = 0;
const uint16_t DEFAULT_SCAN_CYCLES = 6; // recommended starter

// -------- Helpers --------
inline void pwmWrite(uint8_t pin, uint8_t v){ analogWrite(pin, invertPWM ? (255 - v) : v); }
void allOff(){ pwmWrite(PIN_R,0); pwmWrite(PIN_G,0); pwmWrite(PIN_B,0); }
void setColor(char c){
  switch(c){
    case 'R': pwmWrite(PIN_R, brightness); pwmWrite(PIN_G, 0);          pwmWrite(PIN_B, 0);          break;
    case 'G': pwmWrite(PIN_R, 0);          pwmWrite(PIN_G, brightness); pwmWrite(PIN_B, 0);          break;
    case 'B': pwmWrite(PIN_R, 0);          pwmWrite(PIN_G, 0);          pwmWrite(PIN_B, brightness); break;
    default : allOff(); break;
  }
}

void sendInfo(const char* key, const String &val){
  Serial.print(F("{\"info\":\"")); Serial.print(key); Serial.print(F("\",\"val\":\""));
  Serial.print(val); Serial.println(F("\"}"));
}
void sendState(){
  const char* m = (mode==M_AUTO)?"auto":(mode==M_MANUAL_OFF)?"off":(mode==M_MANUAL_R)?"R":(mode==M_MANUAL_G)?"G":"B";
  Serial.print(F("{\"state\":{\"mode\":\"")); Serial.print(m);
  Serial.print(F("\",\"brightness\":")); Serial.print(brightness);
  Serial.print(F(",\"invertPWM\":")); Serial.print(invertPWM ? "true":"false");
  Serial.print(F(",\"cycleMs\":")); Serial.print(cycleMs);
  Serial.print(F(",\"sensor\":\"")); Serial.print(sensorOk ? "ok":"missing");
  Serial.print(F("\",\"scanActive\":")); Serial.print(scanActive ? "true":"false");
  Serial.print(F(",\"scanDone\":")); Serial.print(scanDoneCycles);
  Serial.print(F(",\"scanTarget\":")); Serial.print(scanTargetCycles);
  Serial.println(F("}}"));
}

// Very small line reader (commands separated by newline)
String inbuf;
bool readCommand(String &line){
  while(Serial.available()){
    char c = Serial.read();
    if(c=='\r') continue;
    if(c=='\n'){ line = inbuf; inbuf = ""; return line.length()>0; }
    inbuf += c;
    if(inbuf.length() > 80) inbuf = ""; // protect
  }
  return false;
}

// ---------- Sensor helpers ----------
void printSampleJSON(char led, const float raw18[18], float tempC){
  Serial.print(F("{\"type\":\"sample\",\"seq\":"));
  Serial.print(++seq);
  Serial.print(F(",\"led\":\"")); Serial.print(led);
  Serial.print(F("\",\"raw\":["));
  for(int i=0;i<18;i++){
    if(i) Serial.print(',');
    if(isnan(raw18[i])) Serial.print(F("null"));
    else Serial.print(raw18[i], 4);
  }
  Serial.print(F("],\"tempC\":"));
  if(isnan(tempC)) Serial.print(F("null")); else Serial.print(tempC, 2);
  Serial.println('}');
}

void takeMeasurement(char led){
  float out18[18]; float tempC = NAN;

  if(sensorOk){
    triad.takeMeasurements();

    // Core 12 bands (A..L)
    out18[0]  = triad.getCalibratedA();
    out18[1]  = triad.getCalibratedB();
    out18[2]  = triad.getCalibratedC();
    out18[3]  = triad.getCalibratedD();
    out18[4]  = triad.getCalibratedE();
    out18[5]  = triad.getCalibratedF();
    out18[6]  = triad.getCalibratedG();
    out18[7]  = triad.getCalibratedH();
    out18[8]  = triad.getCalibratedI();
    out18[9]  = triad.getCalibratedJ();
    out18[10] = triad.getCalibratedK();
    out18[11] = triad.getCalibratedL();

    // Fill remaining 6 as null by default
    for(int i=12;i<18;i++) out18[i] = NAN;

    // OPTIONAL EXTENDED BANDS (uncomment if your library exposes them)
    // out18[12] = triad.getCalibratedR();
    // out18[13] = triad.getCalibratedS();
    // out18[14] = triad.getCalibratedT();
    // out18[15] = triad.getCalibratedU();
    // out18[16] = triad.getCalibratedV();
    // out18[17] = triad.getCalibratedW();

    tempC = triad.getTemperature();
  } else {
    for(int i=0;i<18;i++) out18[i] = NAN;
  }

  printSampleJSON(led, out18, tempC);

  // ---- Scan job accounting: count a cycle at the end of B ----
  if(scanActive && led=='B'){
    scanDoneCycles++;
    Serial.print(F("{\"job\":\"scan_progress\",\"done\":")); Serial.print(scanDoneCycles);
    Serial.print(F(",\"target\":")); Serial.print(scanTargetCycles);
    Serial.println('}');
    if(scanDoneCycles >= scanTargetCycles){
      scanActive = false;
      mode = M_MANUAL_OFF;
      allOff();
      Serial.print(F("{\"job\":\"scan_done\",\"done\":")); Serial.print(scanDoneCycles);
      Serial.print(F(",\"target\":")); Serial.print(scanTargetCycles);
      Serial.println(F("}"));
    }
  }
}

// ---------- Commands ----------
void applyCommand(const String &line){
  String cmd = line; cmd.trim(); cmd.toLowerCase();

  // Start a scan job: "scan" or "scan N"
  if(cmd.startsWith("scan")){
    // Parse N if provided
    uint16_t n = DEFAULT_SCAN_CYCLES;
    if(cmd.length() > 4){
      String arg = cmd.substring(4); arg.trim();
      if(arg.length()>0){ n = (uint16_t)constrain(arg.toInt(), 1, 10000); }
    }
    scanActive = true;
    scanTargetCycles = n;
    scanDoneCycles = 0;

    // Ensure auto mode and reset slot timing from Red
    mode = M_AUTO;
    step = S_R;
    setColor('R');
    settled   = false;
    slotStart = millis();
    nextAt    = millis() + (cycleMs/3);

    Serial.print(F("{\"job\":\"scan_start\",\"target\":")); Serial.print(n); Serial.println(F("}"));
    return;
  }

  // Abort a running scan
  if(cmd=="stopscan" || cmd=="abort"){
    scanActive = false;
    sendInfo("scan","aborted");
    return;
  }

  if(cmd=="start"){ // continuous (no job)
    scanActive = false;
    mode = M_AUTO;
    settled   = false;
    step = S_R; setColor('R');
    nextAt = millis();
    sendInfo("mode","auto");
    return;
  }

  if(cmd=="stop"){ // stop cycling
    scanActive = false;
    mode = M_MANUAL_OFF; allOff();
    sendInfo("mode","off");
    return;
  }

  if(cmd.startsWith("set ")){
    scanActive = false;
    String v = cmd.substring(4);
    if(v=="r"){ mode = M_MANUAL_R; setColor('R'); sendInfo("mode","R"); }
    else if(v=="g"){ mode = M_MANUAL_G; setColor('G'); sendInfo("mode","G"); }
    else if(v=="b"){ mode = M_MANUAL_B; setColor('B'); sendInfo("mode","B"); }
    else { mode = M_MANUAL_OFF; allOff(); sendInfo("mode","off"); }
    return;
  }

  if(cmd.startsWith("bright ")){
    int val = cmd.substring(7).toInt();
    val = constrain(val, 0, 255);
    brightness = (uint8_t)val;
    if(mode==M_MANUAL_R) setColor('R');
    else if(mode==M_MANUAL_G) setColor('G');
    else if(mode==M_MANUAL_B) setColor('B');
    sendInfo("brightness", String(brightness));
    return;
  }

  if(cmd.startsWith("interval ")){
    uint32_t ms = (uint32_t)cmd.substring(9).toInt();
    ms = constrain(ms, 300u, 5000u);
    cycleMs = ms;
    sendInfo("cycleMs", String(cycleMs));
    return;
  }

  if(cmd.startsWith("invert ")){
    String v = cmd.substring(7);
    if(v=="on") invertPWM = true;
    else if(v=="off") invertPWM = false;
    else invertPWM = !invertPWM;
    if(mode==M_MANUAL_R) setColor('R');
    else if(mode==M_MANUAL_G) setColor('G');
    else if(mode==M_MANUAL_B) setColor('B');
    sendInfo("invertPWM", invertPWM ? "on":"off");
    return;
  }

  if(cmd=="status"){ sendState(); return; }

  sendInfo("unknown", cmd);
}

// -------- Arduino --------
void setup(){
  pinMode(PIN_R, OUTPUT);
  pinMode(PIN_G, OUTPUT);
  pinMode(PIN_B, OUTPUT);
  allOff();

  Serial.begin(9600);
  delay(300);        // allow port to sync
  sendState();

  Wire.begin();      // Uno: SDA=A4, SCL=A5
  sensorOk = triad.begin(); // do not halt if missing
  if(sensorOk){
    triad.setGain(2);  // numeric: 0..3 (≈1x, ~3.7x, 16x, 64x)
    uint8_t itCycles = (uint8_t)constrain((int)(itMs / 2.8f), 1, 255);
    triad.setIntegrationCycles(itCycles);
    sendInfo("sensor","ok");
  } else {
    sendInfo("sensor","missing");
  }

  // Start in continuous auto mode at Red
  nextAt = millis();
  setColor('R');
  step = S_R;
  settled = false;
}

void loop(){
  // Handle commands
  String line;
  if(readCommand(line)) applyCommand(line);

  // Auto cycle (drives both continuous and scan jobs)
  if(mode == M_AUTO){
    uint32_t now = millis();
    uint32_t slot = cycleMs / 3; if(slot < settleMs) slot = settleMs;

    if(!settled){
      slotStart = now;
      settled = true;
    }

    // Take exactly one measurement once the settle time passes
    if((int32_t)(now - slotStart) >= (int32_t)settleMs){
      char led = (step==S_R)?'R':(step==S_G)?'G':'B';
      takeMeasurement(led);

      // Immediately move to next color and schedule remainder of slot
      settled = false;
      if(step==S_R){ step = S_G; setColor('G'); }
      else if(step==S_G){ step = S_B; setColor('B'); }
      else { step = S_R; setColor('R'); }
      nextAt = now + (slot - settleMs);
    }

    // Safety boundary to keep time stable
    if((int32_t)(now - nextAt) >= 0){
      settled = false;
      nextAt = now + slot;
    }
  }
  // Manual modes: hold last color, no reads
}
