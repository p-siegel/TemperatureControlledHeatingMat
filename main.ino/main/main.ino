#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#ifndef STASSID
#define STASSID "<WLANNAME>"
#define STAPSK  "<PASSWORD>"
#endif

const char* ssid = STASSID;
const char* password = STAPSK;
float temperatureC = 0.0;
float globalDelta = 0.5;
int globalCount = 0;

// GPIO where the DS18B20 is connected to
const int oneWireBus = 4;
const int led = 13;

const char* PARAM_ZIELTEMP = "zieltemp";
const char* PARAM_DELTA = "delta";
int zieltemperatur = 28;
enum SocketState {
  unknown,
  on,
  off
};
SocketState currentSocketState = SocketState::unknown;
enum Regulator{
  reg_on,
  reg_off
};
Regulator regulatorState = Regulator::reg_off;

OneWire oneWire(oneWireBus);
DallasTemperature sensors(&oneWire);

ESP8266WebServer server(80);
WiFiClient client;

float getTemperature(void);
void handleRoot(void);
void handleTemperature(void);
void turnOn(void);
void turnOff(void);
void handleNotFound(void);
void checkCurrentTemperature(void);
void setup(void);
void loop(void);

const char index_html[] PROGMEM = R"rawliteral(<!DOCTYPE HTML><html> <head> <meta name="viewport" content="width=device-width, initial-scale=1"> <style> html { font-family: Arial; display: inline-block; margin: 0px auto; text-align: center; } h2 { font-size: 3.0rem; } p { font-size: 3.0rem; } .units { font-size: 1.2rem; } .ds-labels{ font-size: 1.5rem; vertical-align:middle; padding-bottom: 15px; } form { overflow: hidden; } .btn-settings { border: none; color: white; float: right; padding: 15px 32px; text-align: center; text-decoration: none; display: inline-block; font-size: 16px; background-color: #03A1FC; } .btn-on{ border: none; color: white; padding: 15px 32px; text-align: center; text-decoration: none; display: inline-block; font-size: 16px; background-color: #4CAF50; } .grid-container { display: grid; justify-content: center; text-align: center; grid-template-columns: auto auto; } .submit-btn{ border: none; color: white; padding: 15px 32px; text-align: center; text-decoration: none; display: inline-block; font-size: 16px; background-color: #3281A8; } .switch { position: relative; display: inline-block; width: 60px; height: 34px; } /* Hide default HTML checkbox */ .switch input { opacity: 0; width: 0; height: 0; } /* The slider */ .slider { position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0; background-color: #ccc; -webkit-transition: .4s; transition: .4s; } .slider:before { position: absolute; content: ""; height: 26px; width: 26px; left: 4px; bottom: 4px; background-color: white; -webkit-transition: .4s; transition: .4s; } input:checked + .slider { background-color: #2196F3; } input:focus + .slider { box-shadow: 0 0 1px #2196F3; } input:checked + .slider:before { -webkit-transform: translateX(26px); -ms-transform: translateX(26px); transform: translateX(26px); } /* Rounded sliders */ .slider.round { border-radius: 34px; } .slider.round:before { border-radius: 50%; } </style> <script> const Regulator = { reg_on: 0, reg_off: 1 }; const Socket = { socket_unknown: 0, socket_on: 1, socket_off: 2 }; function getTemperature(){ var xhttp = new XMLHttpRequest(); xhttp.onreadystatechange = function() { if (this.readyState == 4 && this.status == 200) { var response = JSON.parse(this.responseText); document.getElementById("temperaturec").innerHTML = response.currentTemp; document.getElementById("temperatureTarget").innerHTML = response.targetTemp; /*document.getElementById("input_temp").value = response.targetTemp;*/ document.getElementById("temperatureDelta").innerHTML = response.delta; /*document.getElementById("input_delta").value = response.delta;*/ if( response.regulator == Regulator.reg_on ){ document.getElementById("regulator-switch").checked = true; }else{ document.getElementById("regulator-switch").checked = false; } if( response.socket == Socket.socket_on ){ document.getElementById("socket-switch").checked = true; }else{ document.getElementById("socket-switch").checked = false; } } }; xhttp.open("GET", "/temperaturec", true); xhttp.send(); }; setInterval(getTemperature, 5000); function turnOn(){ var xhttp = new XMLHttpRequest(); xhttp.open("GET", "/turnOn", true); xhttp.send(); }; function turnOff(){ var xhttp = new XMLHttpRequest(); xhttp.open("GET", "/turnOff", true); xhttp.send(); }; function handleClick(cb) { if(cb.checked){ var xhttp = new XMLHttpRequest(); xhttp.open("GET", "/activateRegulation", true); xhttp.send(); }else{ var xhttp = new XMLHttpRequest(); xhttp.open("GET", "/deactivateRegulation", true); xhttp.send(); } }; function handleMatteClick(cb) { if(cb.checked){ var xhttp = new XMLHttpRequest(); xhttp.open("GET", "/turnOn", true); xhttp.send(); }else{ var xhttp = new XMLHttpRequest(); xhttp.open("GET", "/turnOff", true); xhttp.send(); } }; function getSettings(){ location.href = "/settings"; }; </script> </head> <body onload="getTemperature()"> <h2>Heizmatten Server</h2> <button class="btn-settings" id="settings_btn" onclick="getSettings()">Einstellungen</button> <p> <i class="fas fa-thermometer-half" style="color:#059e8a;"></i> <span class="ds-labels">Aktuelle Temperatur</span> <span id="temperaturec">%TEMPERATUREC%</span> <sup class="units">&deg;C</sup> </p> <p> <span class="ds-labels">Temperaturziel</span> <span id="temperatureTarget">%TEMPERATURETARGET%</span> <sup class="units">&deg;C</sup> </p> <p> <span class="ds-labels">Temperatur Delta</span> <span id="temperatureDelta">%TEMPERATUREDELTA%</span> <sup class="units">&deg;C</sup> </p> <p> </p> <p> <span class="ds-labels">Regelung an/aus:</span> <label class="switch"> <input id="regulator-switch" type="checkbox" onclick="handleClick(this);"> <span class="slider round"></span> </label> </p> <p> <span class="ds-labels">Matte an/aus:</span> <label class="switch"> <input id="socket-switch" type="checkbox" onclick="handleMatteClick(this);"> <span class="slider round"></span> </label> </p> </body> </html>)rawliteral";
const char settings_html[] PROGMEM = R"rawliteral(<!DOCTYPE HTML><html> <head> <meta name="viewport" content="width=device-width, initial-scale=1"> <style> html { font-family: Arial; display: inline-block; margin: 0px auto; text-align: center; } h2 { font-size: 3.0rem; } p { font-size: 3.0rem; } .units { font-size: 1.2rem; } .ds-labels{ font-size: 1.5rem; vertical-align:middle; padding-bottom: 15px; } form { overflow: hidden; } .btn-settings { border: none; color: white; float: right; padding: 15px 32px; text-align: center; text-decoration: none; display: inline-block; font-size: 16px; background-color: #03A1FC; } .btn-on{ border: none; color: white; padding: 15px 32px; text-align: center; text-decoration: none; display: inline-block; font-size: 16px; background-color: #4CAF50; } .grid-container { display: grid; justify-content: center; text-align: center; grid-template-columns: auto auto; } .submit-btn{ border: none; color: white; padding: 15px 32px; text-align: center; text-decoration: none; display: inline-block; font-size: 16px; background-color: #3281A8; } .switch { position: relative; display: inline-block; width: 60px; height: 34px; } /* Hide default HTML checkbox */ .switch input { opacity: 0; width: 0; height: 0; } /* The slider */ .slider { position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0; background-color: #ccc; -webkit-transition: .4s; transition: .4s; } .slider:before { position: absolute; content: ""; height: 26px; width: 26px; left: 4px; bottom: 4px; background-color: white; -webkit-transition: .4s; transition: .4s; } input:checked + .slider { background-color: #2196F3; } input:focus + .slider { box-shadow: 0 0 1px #2196F3; } input:checked + .slider:before { -webkit-transform: translateX(26px); -ms-transform: translateX(26px); transform: translateX(26px); } /* Rounded sliders */ .slider.round { border-radius: 34px; } .slider.round:before { border-radius: 50%; } </style> <script> </script> </head> <body> <h2>Einstellungen</h2> <form action="/" method="POST"> <div class="grid-container"> <div class="grid-item"> <span class="ds-labels">Zieltemperatur:</span> </div> <div class="grid-item"> <input id="input_temp" type="text" name="zieltemp" value="28"> </div> <div class="grid-item"> <span class="ds-labels">TemperaturFenster (Delta):</span> </div> <div class="grid-item"> <input id="input_delta" type="text" name="delta" value="0.5"> </div> <div class="grid-item"></div> <div class="grid-item"> <input class="submit-btn" type="submit" value="Submit"> </div> </div> </form> </body> </html>)rawliteral";
float getTemperature(void){
  sensors.requestTemperatures();
  return sensors.getTempCByIndex(0);
}

void handleRoot(void) {
  server.send(200, "text/html", index_html);
}

void handleTemperature(void) {    
  temperatureC = getTemperature();
  String message1 = "{\"currentTemp\":";
  String message2 = ",\"targetTemp\":";
  String message3 = ",\"delta\":";
  String message4 = ",\"regulator\":";
  String message5 = ",\"socket\":";
  String messageEnd = "}";
  server.send(200, "text/plain", String(message1 + String(temperatureC) + message2 + String(zieltemperatur) + message3 + String(globalDelta) + message4 + String(regulatorState) + message5 + String(currentSocketState) + messageEnd));
}

void turnOn(void) {
  if(currentSocketState == SocketState::on){
    server.send(200, "text/plain", "");
    return;
  }
    
  globalCount += 1;
  const char payload[] = "{\"system\": {\"set_relay_state\": {\"state\": 1}}}";
  int key = 0xAB;

  char encryped_payload[strlen(payload) + 4] = "";
  encryped_payload[0] = 0x00;
  encryped_payload[1] = 0x00;
  encryped_payload[2] = 0x00;
  encryped_payload[3] = 0x2d;

  for (int i = 0; i < sizeof(encryped_payload) - 4; i++) {
    encryped_payload[i + 4] = key ^ payload[i];
    key = encryped_payload[i + 4];
  }
 
  if(client.connect("192.168.178.43", 9999)){
    client.write(encryped_payload, sizeof(encryped_payload));
    Serial.print("Sent on: ");
    Serial.println(globalCount);
    server.send(200, "text/plain", "");
    currentSocketState = SocketState::on;
  }
  else{
    Serial.println("Could not connect!");
  }
}

void turnOff(void) {
  if(currentSocketState == SocketState::off){
    server.send(200, "text/plain", "");
    return;
  }
  globalCount += 1;
  const char payload[] = "{\"system\": {\"set_relay_state\": {\"state\": 0}}}";
  int key = 0xAB;

  char encryped_payload[strlen(payload) + 4] = "";
  encryped_payload[0] = 0x00;
  encryped_payload[1] = 0x00;
  encryped_payload[2] = 0x00;
  encryped_payload[3] = 0x2d;

  for (int i = 0; i < sizeof(encryped_payload) - 4; i++) {
    encryped_payload[i + 4] = key ^ payload[i];
    key = encryped_payload[i + 4];
  }
 
  if(client.connect("192.168.178.43", 9999)){
    client.write(encryped_payload, sizeof(encryped_payload));
    Serial.print("Sent off: ");
    Serial.println(globalCount);
    server.send(200, "text/plain", "");
    currentSocketState = SocketState::off;
  }
  else{
    Serial.println("Could not connect!");
  }
}

void handleNotFound(void) {
  digitalWrite(led, 1);
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
  digitalWrite(led, 0);
}

void checkCurrentTemperature(void){
  if( regulatorState == Regulator::reg_off)
  { return; }
  if( currentSocketState == SocketState::unknown )
  { return; }
  temperatureC = getTemperature();
  if( ( currentSocketState == SocketState::on ) && ( float(temperatureC)  > (zieltemperatur + globalDelta) ) ){
    turnOff();
  }else if( ( currentSocketState == SocketState::off ) && ( float(temperatureC) < (zieltemperatur - globalDelta) ) ){
    turnOn();
  }
}

void setup(void) {
  pinMode(led, OUTPUT);
  digitalWrite(led, 0);
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("");
  sensors.begin();

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  server.on("/", HTTP_GET, handleRoot);
  server.on("/temperaturec", handleTemperature);
  server.on("/turnOn", turnOn);
  server.on("/turnOff", turnOff);
  server.on("/", HTTP_POST, [] () {
    String inputMessage;
    if(server.hasArg(PARAM_ZIELTEMP)) {
      int targetTemp = server.arg(PARAM_ZIELTEMP).toInt();
      Serial.print("Target Temp: ");
      Serial.println(targetTemp);
      if(targetTemp < 35 && targetTemp > 15)
      {
        zieltemperatur = targetTemp;
      }
    }
    if(server.hasArg(PARAM_DELTA)){
      float delta = server.arg(PARAM_DELTA).toFloat();
      Serial.print("Delta: ");
      Serial.println(delta);
      if(delta > 0.0 && delta < 10.0){
        globalDelta = delta;
      }
    }
    server.sendHeader("Location","/");
    server.send(303);
  });
  server.on("/deactivateRegulation", HTTP_GET, [] () {
    regulatorState = Regulator::reg_off; 
    server.send(200, "text/plain", "");
  });
  server.on("/activateRegulation", HTTP_GET, [] () {
    regulatorState = Regulator::reg_on; 
    server.send(200, "text/plain", "");
  });
  server.on("/settings", HTTP_GET, [] () {
    server.send(200, "text/html", settings_html);
  });
  

  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP server started");
}

void loop(void) {
  server.handleClient();
  checkCurrentTemperature();
  delay(100);
}
