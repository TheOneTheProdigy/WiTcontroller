/**
 * This app turns the ESP32 into a Bluetooth LE keyboard that is intended to act as a dedicated
 * gamepad for the JMRI or any wiThrottle server.

  Instructions:
  - Update WiFi SSID and password as necessary.
  - Flash the sketch 
 */

#include <WiFi.h>                 // https://github.com/espressif/arduino-esp32/tree/master/libraries/WiFi
#include <ESPmDNS.h>              // https://github.com/espressif/arduino-esp32/blob/master/libraries/ESPmDNS
#include <WiThrottleProtocol.h>   // https://github.com/flash62au/WiThrottleProtocol
#include <AiEsp32RotaryEncoder.h> // https://github.com/igorantolic/ai-esp32-rotary-encoder
#include <Keypad.h>               // https://www.arduinolibraries.info/libraries/keypad
#include "actions.h"

const char* ssid     = "pra";
const char* password = "...";
const char* deviceName = "WiT Controller";

// 4x4 keypad
int buttonActions[14] = { SPEED_STOP,   // 0
                         FUNCTION_0,    // 1 - lights
                         FUNCTION_1,    // 2 - bell
                         FUNCTION_2,    // 3 - horn
                         FUNCTION_NULL, // 4
                         SPEED_UP,      // 5
                         FUNCTION_NULL, // 6
                         DIRECTION_REVERSE, // 7
                         SPEED_DOWN,    // 8
                         DIRECTION_FORWARD  // 9
                         FUNCTION_NULL, // A
                         FUNCTION_NULL, // B
                         FUNCTION_NULL, // C
                         FUNCTION_NULL // D
};
// 4x3 keypad
// int buttonActions[10] = { SPEED_STOP,   // 0
//                          FUNCTION_0,    // 1 - lights
//                          FUNCTION_1,    // 2 - bell
//                          FUNCTION_2,    // 3 - horn
//                          FUNCTION_NULL, // 4
//                          SPEED_UP,      // 5
//                          FUNCTION_NULL, // 6
//                          DIRECTION_REVERSE, // 7
//                          SPEED_DOWN,    // 8
//                          DIRECTION_FORWARD  // 9
// };

int currentSpeed = 0;
Direction currentDirection;
const int speedStep = 4;
const int speedStepMultiplier = 2;  // for 'fast' speed steps
TrackPower trackPower = PowerUnknown;

#define MAX_LOCOS     10  // maximum number of locos that can be added to the consist

#define ROTARY_ENCODER_A_PIN 33
#define ROTARY_ENCODER_B_PIN 32
#define ROTARY_ENCODER_BUTTON_PIN 27
#define ROTARY_ENCODER_VCC_PIN -1 /* 27 put -1 of Rotary encoder Vcc is connected directly to 3,3V; else you can use declared output pin for powering rotary encoder */
#define ROTARY_ENCODER_STEPS 4 //depending on your encoder - try 1,2 or 4 to get expected behaviour

bool circleValues = true;
int encoderValue = 0;
int lastEncoderValue = 0;

#define ROW_NUM     4 // four rows
#define COLUMN_NUM  4 // four columns
char keys[ROW_NUM][COLUMN_NUM] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};
byte pin_rows[ROW_NUM]      = {19, 18, 5, 17}; // connect to the row pins
byte pin_column[COLUMN_NUM] = {16, 4, 2, 15};   // connect to the column pins
//#define ROW_NUM     4 // four rows
//#define COLUMN_NUM  3 // four columns
//char keys[ROW_NUM][COLUMN_NUM] = {
//  {'1', '2', '3'},
//  {'4', '5', '6'},
//  {'7', '8', '9'},
//  {'*', '0', '#'}
//};
//byte pin_rows[ROW_NUM]      = {19, 18, 17, 16}; // GIOP19, GIOP18, GIOP5, GIOP17 connect to the row pins
//byte pin_column[COLUMN_NUM] = { 4, 0, 2};   // GIOP16, GIOP4, GIOP0 connect to the column pins
Keypad keypad = Keypad( makeKeymap(keys), pin_rows, pin_column, ROW_NUM, COLUMN_NUM );
boolean menuCommandStarted = false;
String menuCommand = "";

// WiThrottleProtocol Delegate class
class MyDelegate : public WiThrottleProtocolDelegate {
  
  public:
    void receivedVersion(String version) {     
      Serial.print("Received version: "); Serial.println(version);  
    }
    void receivedServerDescription(String description) {
      Serial.print("Received description: "); Serial.println(description); 
    }
    void receivedSpeed(int speed) {             // Vnnn
      currentSpeed = speed;
      Serial.print("Speed: "); Serial.println(speed); 
    }
    void receivedDirection(Direction dir) {     // R{0,1}
      currentDirection = dir;
    }
    // void receivedFunctionState(uint8_t func, bool state) { 
    //   Serial.print("Received Fn: "); Serial.print(func); Serial.print(" State: "); Serial.println( (state) ? "True" : "False" );
    // }
    void receivedTrackPower(TrackPower state) { 
      trackPower = state;
    }
};

IPAddress firstWitServerIP;
int firstWitServerPort = 0;
WiFiClient client;
WiThrottleProtocol wiThrottleProtocol;
MyDelegate myDelegate;
boolean witConnected = false;
int deviceId = random(1000,9999);

AiEsp32RotaryEncoder rotaryEncoder = AiEsp32RotaryEncoder(ROTARY_ENCODER_A_PIN, ROTARY_ENCODER_B_PIN, ROTARY_ENCODER_BUTTON_PIN, ROTARY_ENCODER_VCC_PIN, ROTARY_ENCODER_STEPS);
void IRAM_ATTR readEncoderISR() {
  rotaryEncoder.readEncoder_ISR();
}

void rotary_onButtonClick() {
  static unsigned long lastTimePressed = 0;
  if (millis() - lastTimePressed < 500) {   //ignore multiple press in that time milliseconds
    return;
  }
  lastTimePressed = millis();
  if (wiThrottleProtocol.getNumberOfLocomotives()>0) {
    wiThrottleProtocol.setSpeed(0);
    currentSpeed = 0;
  }
  Serial.println("encoder button pressed");
}

void rotary_loop() {
  if (rotaryEncoder.encoderChanged()) {   //don't print anything unless value changed
    encoderValue = rotaryEncoder.readEncoder();
     Serial.print("Encoder From: "); Serial.print(lastEncoderValue);
     Serial.print(" to: "); Serial.println(encoderValue);
    if (abs(encoderValue-lastEncoderValue) > 800) { // must have passed through zero
      if (encoderValue > 800) {
        lastEncoderValue = 1001; 
      } else {
        lastEncoderValue = 0; 
      }
      Serial.print("Corrected Encoder From: "); Serial.print(lastEncoderValue);
      Serial.print(" to: "); Serial.println(encoderValue);
    }
    if (wiThrottleProtocol.getNumberOfLocomotives()>0) {
      if (encoderValue > lastEncoderValue) {
        if (abs(encoderValue-lastEncoderValue)<50) {
          speedDown(speedStep);
        } else {
          speedDown(speedStep*speedStepMultiplier);
        }
      } else {
        if (abs(encoderValue-lastEncoderValue)<50) {
          speedUp(speedStep);
        } else {
          speedUp(speedStep*speedStepMultiplier);
        }
      } 
    }
    lastEncoderValue = encoderValue;
  }
  
  if (rotaryEncoder.isEncoderButtonClicked()) {
    rotary_onButtonClick();
  }
}

void keypadEvent(KeypadEvent key){
  switch (keypad.getState()){
  case PRESSED:
    Serial.print("Button "); Serial.print(String(key - '0')); Serial.println(" pushed.");
    doKeyPress(key, true);
    break;
  case RELEASED:
    doKeyPress(key, false);
    Serial.print("Button "); Serial.print(String(key - '0')); Serial.println(" released.");
    break;
//    case HOLD:
//        break;
  }
}

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    Serial.print(".");
  }
  Serial.println(""); Serial.print("Connected to "); Serial.println(ssid);
  Serial.print("IP address: "); Serial.println(WiFi.localIP());

  if (!MDNS.begin("ESP32_Browser")) {
    Serial.println("Error setting up MDNS responder!");
    while(1){
      delay(1000);
    }
  }

  rotaryEncoder.begin();  //initialize rotary encoder
  rotaryEncoder.setup(readEncoderISR);
  //set boundaries and if values should cycle or not 
  rotaryEncoder.setBoundaries(0, 1000, circleValues); //minValue, maxValue, circleValues true|false (when max go to min and vice versa)
  //rotaryEncoder.disableAcceleration(); //acceleration is now enabled by default - disable if you dont need it
  rotaryEncoder.setAcceleration(100); //or set the value - larger number = more accelearation; 0 or 1 means disabled acceleration

  keypad.addEventListener(keypadEvent); // Add an event listener for this keypad

  connectFirstWitServer();
}

void loop() {
  // parse incoming messages
  wiThrottleProtocol.check();
  
  char key = keypad.getKey();
  rotary_loop();
  
  delay(100);
}

void browseService(const char * service, const char * proto){
  Serial.printf("Browsing for service _%s._%s.local. ... ", service, proto);
  int n = MDNS.queryService(service, proto);
  if (n == 0) {
    Serial.println("no services found");
  } else {
    Serial.print(n);
    Serial.println(" service(s) found");
    for (int i = 0; i < n; ++i) {
      // Print details for each service found
      Serial.print("  "); Serial.print(i + 1); Serial.print(": "); Serial.print(MDNS.hostname(i));
      Serial.print(" ("); Serial.print(MDNS.IP(i)); Serial.print(":"); Serial.print(MDNS.port(i)); Serial.println(")");
    }
    firstWitServerIP = MDNS.IP(0);
    firstWitServerPort = MDNS.port(0);
  }
  Serial.println();
}

void doKeyPress(char key, boolean pressed) {
  if (pressed)  { //pressed
    switch (key){
      case '*':  // menu command
        menuCommand = "";
        if (menuCommandStarted) { // then cancel the menu
          menuCommandStarted = false;
        } else {
          menuCommandStarted = true;
          Serial.println("Command started");
        }
        break;
      case '#': // end of command
        if ((menuCommandStarted) && (menuCommand.length()>=1)) {
          doMenu();
        }  // otherwise nothing to do
        resetMenu();
        break;
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
        if (menuCommandStarted) { // appeand to the string
          menuCommand += key;
        } else{
          doDirectCommand(key, true);
        }
        break;
      default: { //A, B, C, D
        doDirectCommand(key, true);
        break;
      }
    }
  } else {  // released
    if ( (!menuCommandStarted) && (key>='0') && (key<='D')) { // only process releases for the numeric keys + A,B,C,D and only if a menu command has not be started
      Serial.println("Process key release");
      doDirectCommand(key, false);
    }
  }
}

void doDirectCommand (char key, boolean pressed) {
  Serial.print("Direct command: "); Serial.println(key);
  int buttonAction = buttonActions[(key - '0')];
  if (buttonAction!=FUNCTION_NULL) {
    if ( (buttonAction>=FUNCTION_0) && (buttonAction<=FUNCTION_28) ) {
      doFunction(buttonAction, pressed);

    } else {
      if (pressed) { // only process these on the key press, not the release
        switch (buttonAction) {
            case DIRECTION_FORWARD: {
              changeDirection(Forward);
              break; 
            }
            case DIRECTION_REVERSE: {
              changeDirection(Reverse);
              break; 
            }
            case DIRECTION_TOGGLE: {
              toggleDirection();
              break; 
            }
            case SPEED_UP: {
              speedUp(speedStep);
              break; 
            }
            case SPEED_DOWN: {
              speedDown(speedStep);
              break; 
            }
            case SPEED_UP_FAST: {
              speedUp(speedStep*speedStepMultiplier);
              break; 
            }
            case SPEED_DOWN_FAST: {
              speedUp(speedStep*speedStepMultiplier);
              break; 
            }
            case E_STOP: {
              wiThrottleProtocol.emergencyStop();
              break; 
            }
            case POWER_ON: {
              powerOnOff(PowerOn);
              break; 
            }
            case POWER_OFF: {
              powerOnOff(PowerOff);
              break; 
            }
            case POWER_TOGGLE: {
              powerToggle();
              break; 
            }
        }
      }
    }
  }
}

void doMenu() {
  String loco = "";
  String function = "";
  Serial.print("Menu: "); Serial.println(menuCommand);

  switch (menuCommand[0]) {
    case '1': { // select loco
        if (menuCommand.length()>1) {
          loco = menuCommand.substring(1, menuCommand.length());
          loco = getLocoWithLength(loco);
          Serial.print("add Loco: "); Serial.println(loco);
          wiThrottleProtocol.addLocomotive(loco);
        }
        break;
      }
    case '2': { // de-select loco
        loco = menuCommand.substring(1, menuCommand.length());
        if (loco!="") { // a loco is specified
          loco = getLocoWithLength(loco);
          Serial.print("release Loco: "); Serial.println(loco);
          wiThrottleProtocol.releaseLocomotive(loco);
          // }
        } else { //not loco speciffied so release all
          releaseAllLocos();
        }
        break;
      }
    case '3': { // change direction
        toggleDirection();
        break;
      }
    case '0': { // function button
        if (menuCommand.length()>1) {
          function = menuCommand.substring(1, menuCommand.length());
          int functionNumber = function.toInt();
          if (function != "") { // a function is specified
            doFunction(functionNumber, true);  // always act like latching i.e. pressed
          }
        }
        break;
      }
    case '9': { // disconnect/reconnect
        if (witConnected) {
          disconnectWitServer();
        } else {
          connectFirstWitServer();
        }
        break;
      }
    case '8': {
      powerToggle();
      break;
    }
  }
}

void resetMenu() {
  menuCommandStarted = false;
  menuCommand = "";
}

String getLocoWithLength(String loco) {
  int locoNo = menuCommand.toInt() + 0;
  if (locoNo <= 127) {
    loco = "S" + loco;
  } else {
    loco = "L" + loco;
  }
  return loco;
}

void connectFirstWitServer() {
  // find the first WiT server
  while (firstWitServerPort==0) {
    browseService("withrottle", "tcp");
    delay(1000);
  }

  // connect
  Serial.println("Connecting to the server...");
  if (!client.connect(firstWitServerIP, firstWitServerPort)) {
    Serial.println("connection failed");
    while(1) delay(1000);
  }
  Serial.println("Connected to the server");
  Serial.println(firstWitServerIP);
  Serial.println(firstWitServerPort);
    
  // Uncomment for logging on Serial
//    wiThrottleProtocol.setLogStream(&Serial);
  
  // Pass the delegate instance to wiThrottleProtocol
  wiThrottleProtocol.setDelegate(&myDelegate);

  // Pass the communication to WiThrottle
  wiThrottleProtocol.connect(&client);
  Serial.println("WiThrottle connected");
  wiThrottleProtocol.setDeviceName(deviceName);  
  wiThrottleProtocol.setDeviceID(""+deviceId);  

  witConnected = true;
}

void disconnectWitServer() {
  releaseAllLocos();
  wiThrottleProtocol.disconnect();
  Serial.println("Disconnected from wiThrottle server\n");
  witConnected = false;
}

void speedDown(int amt) {
  if (wiThrottleProtocol.getNumberOfLocomotives()>0) {
    int newSpeed = currentSpeed - amt;
    if (newSpeed <0) { newSpeed = 0; }
    wiThrottleProtocol.setSpeed(newSpeed);
    currentSpeed = newSpeed;
    Serial.print("Speed Down: "); Serial.println(amt);
  }
}

void speedUp(int amt) {
  if (wiThrottleProtocol.getNumberOfLocomotives()>0) {
    int newSpeed = currentSpeed + amt;
    if (newSpeed >128) { newSpeed = 126; }
    wiThrottleProtocol.setSpeed(newSpeed);
    currentSpeed = newSpeed;
    Serial.print("Speed Up: "); Serial.println(amt);
  }
}

void releaseAllLocos() {
  if (wiThrottleProtocol.getNumberOfLocomotives()>0) {
    for(int index=0;index<wiThrottleProtocol.getNumberOfLocomotives();index++) {
      wiThrottleProtocol.releaseLocomotive(wiThrottleProtocol.getLocomotiveAtPosition(index));
    } 
  }
}

void toggleDirection() {
  if (wiThrottleProtocol.getNumberOfLocomotives()>0) {
    Direction direction = Reverse;
    if (currentDirection == Reverse) {
      direction = Forward;
    }
    changeDirection(direction);
  }
}

void changeDirection(Direction direction) {
  if (wiThrottleProtocol.getNumberOfLocomotives()>0) {
    wiThrottleProtocol.setDirection(direction);
    currentDirection = direction;
    Serial.print("Change direction: "); Serial.println( (direction==Forward) ? "Forward" : "Reverse");
  }
}

void doFunction(int functionNumber, boolean pressed) {
  if (wiThrottleProtocol.getNumberOfLocomotives()>0) {
    wiThrottleProtocol.setFunction(functionNumber, pressed );
    Serial.print("fn: "); Serial.print(functionNumber); Serial.println( (pressed) ? " Pressed" : " Released");
  }
}

void powerOnOff(TrackPower powerState) {
  wiThrottleProtocol.setTrackPower(powerState);
  trackPower = powerState;
}

void powerToggle() {
  if (trackPower==PowerOn) {
    powerOnOff(PowerOff);
  } else {
    powerOnOff(PowerOn);
  }
}
