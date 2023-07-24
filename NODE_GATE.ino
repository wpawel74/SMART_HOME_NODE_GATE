#include "PCF8574.h"
//#include <ESP8266WiFi.h>
#include "EspMQTTClient.h"

#define LOG_ENABLED
#ifdef LOG_ENABLED
#define LOG(...)         Serial.print(__VA_ARGS__)
#else
#define LOG(...)         do {} while(0)
#endif

// ---------------------
// ESP8266 PIN MAP
// ---------------------
#define ACRELAY_PIN           16 // D0
#define SWITCH1_PIN           12 // D6
#define SWITCH2_PIN           13 // D7
#define PCF8574_INT_PIN       14 // D4

// ---------------------
// PCF8574 PIN MAP
// ---------------------
#define BUTTON1_IN_PIN        P0 // HV3 ... RF_1 NICE.SMILO
#define BUTTON2_IN_PIN        P2 // HV4 ... RF_2
#define BUTTON3_IN_PIN        P4 // HV2
#define BUTTON4_IN_PIN        P6 // HV1
#define BUTTON1_OUT_PIN       P1 // HV3 ... RF_1 NICE.SMILO
#define BUTTON2_OUT_PIN       P3 // HV4 ... RF_2
#define BUTTON3_OUT_PIN       P5 // HV2
#define BUTTON4_OUT_PIN       P7 // HV1

#define DOOR_RATE             3000   // 3 sec
#define ACRELAY_RATE          1000   // 1 sec

unsigned long ac_tick = 0, ac_timer = 60;
int switch1_ls = 0, switch2_ls = 0;

// Set i2c address
PCF8574 pcf8574(0x20);
bool pcf8574_irq_enabled = false;

// The client will handle wifi connection (connecting, retrying, etc) and MQTT connection
EspMQTTClient client(
  "HOME",                // WIFI SSID
  "",         // WIFI password
  "smart.home",           // MQTT server ip address
  "ESP8266_GATE",         // MQTT user name, can be omitted if not needed
  "homeland",             // MQTT password, can be omitted if not needed
  "ESP8266_Node_Gate",      // MQTT unique client name
  1883                    // MQTT port
);

bool keyPressed = false;
// Function interrupt
ICACHE_RAM_ATTR void keyPressedOnPCF8574(){
  // Interrupt called (No Serial no read no wire in this function, and DEBUG disabled on PCF library)
  keyPressed = true;
  ac_tick = 1;
}

static void AcRelayHandler(){
  if( pcf8574_irq_enabled == false ){
    attachInterrupt(digitalPinToInterrupt(PCF8574_INT_PIN), keyPressedOnPCF8574, FALLING);
    pcf8574_irq_enabled = true;
  }
  if( ac_tick ){
    if( ac_timer < ++ac_tick ){
      detachInterrupt(digitalPinToInterrupt(PCF8574_INT_PIN));
      pcf8574_irq_enabled = false;
      digitalWrite(ACRELAY_PIN, 0);
      ac_tick = 0;
    }
    if( ac_tick )
      digitalWrite(ACRELAY_PIN, 1);    
  }

  if( client.isConnected() ){
    client.publish("HOME/GATE/ACRELAY/STATUS", digitalRead( ACRELAY_PIN ) ? "1": "0" );
  }
  client.executeDelayed( ACRELAY_RATE, AcRelayHandler );
  //LOG("ACRELAY HANDLER");
  if (keyPressed){
    keyPressed = false;
  }
}

static void gateSensorHandler(){
  if( client.isConnected() &&  ac_tick ){
    if( switch1_ls != digitalRead( SWITCH1_PIN ) )
      client.publish("HOME/GATE/SWITCH1/STATUS", digitalRead( SWITCH1_PIN ) ? "1": "0" );
    if( switch2_ls != digitalRead( SWITCH2_PIN ) )
      client.publish("HOME/GATE/SWITCH2/STATUS", digitalRead( SWITCH2_PIN ) ? "1": "0" );
  }
  switch1_ls = digitalRead( SWITCH1_PIN );
  switch2_ls = digitalRead( SWITCH2_PIN );

  LOG("ESP8266: SWITCH1(");
  LOG(digitalRead( SWITCH1_PIN ));
  LOG(") SWITCH2(");
  LOG(digitalRead( SWITCH2_PIN ));
  LOG(") TICK(");
  LOG(ac_tick);
  LOG(")\n");
  client.executeDelayed( DOOR_RATE, gateSensorHandler );
}

void setButton( int no, bool state ){
  PCF8574::DigitalInput digitalInput;
  pcf8574.digitalWrite(no, !state);
}

// For client
void onConnectionEstablished(){
  client.subscribe("HOME/GATE/BUTTON1/PIN", [](const String & payload) {
    LOG("MQTT: BUTTON1 ");
    LOG(payload);
    LOG("\n");
    if( payload.toInt() ){
      ac_tick = 1;
      digitalWrite(ACRELAY_PIN, 1);
    }
    setButton(BUTTON1_OUT_PIN, payload.toInt());
  });
  client.subscribe("HOME/GATE/BUTTON2/PIN", [](const String & payload) {
    LOG("MQTT: BUTTON2 ");
    LOG(payload);
    LOG("\n");
    if( payload.toInt() ){
      ac_tick = 1;
      digitalWrite(ACRELAY_PIN, 1);
    }
    setButton(BUTTON2_OUT_PIN, payload.toInt());
  });
  client.subscribe("HOME/GATE/BUTTON3/PIN", [](const String & payload) {
    LOG("MQTT: BUTTON3 ");
    LOG(payload);
    LOG("\n");
    if( payload.toInt() ){
      ac_tick = 1;
      digitalWrite(ACRELAY_PIN, 1);
    }
    setButton(BUTTON3_OUT_PIN, payload.toInt());
  });
  client.subscribe("HOME/GATE/BUTTON4/PIN", [](const String & payload) {
    LOG("MQTT: BUTTON4 ");
    LOG(payload);
    LOG("\n");
    if( payload.toInt() ){
      ac_tick = 1;
      digitalWrite(ACRELAY_PIN, 1);
    }
    setButton(BUTTON4_OUT_PIN, payload.toInt());
  });
  client.subscribe("HOME/GATE/ACRELAY/ENABLE", [](const String & payload) {
    LOG("MQTT: ACRELAY ");
    LOG(payload);
    LOG("\n");
    if( payload.toInt() == 1 )
      ac_tick = 1;
    digitalWrite(ACRELAY_PIN, payload.toInt());
  });

  client.publish("HOME/GATE/SWITCH1/STATUS", digitalRead( SWITCH1_PIN ) ? "1": "0" );
  client.publish("HOME/GATE/SWITCH2/STATUS", digitalRead( SWITCH2_PIN ) ? "1": "0" );
  client.publish("HOME/GATE/ACRELAY/STATUS", digitalRead( ACRELAY_PIN ) ? "1": "0" );
  switch1_ls = digitalRead( SWITCH1_PIN );
  switch2_ls = digitalRead( SWITCH2_PIN );

  LOG("MQTT: ... CONNECTED!\n");
}

void setup(){
  Serial.begin(115200);
  //delay(1000);
  LOG("INIT\n");

  pcf8574.pinMode(BUTTON1_IN_PIN, INPUT);
  pcf8574.pinMode(BUTTON2_IN_PIN, INPUT);
  pcf8574.pinMode(BUTTON3_IN_PIN, INPUT);
  pcf8574.pinMode(BUTTON4_IN_PIN, INPUT);

  pcf8574.pinMode(BUTTON1_OUT_PIN, OUTPUT, HIGH);
  pcf8574.pinMode(BUTTON2_OUT_PIN, OUTPUT, HIGH);
  pcf8574.pinMode(BUTTON3_OUT_PIN, OUTPUT, HIGH);
  pcf8574.pinMode(BUTTON4_OUT_PIN, OUTPUT, HIGH);

  LOG("Init pcf8574...");
  if (pcf8574.begin()){
    LOG("OK\n");
  }else{
    LOG("KO\n");
  }
  
  pinMode(ACRELAY_PIN, OUTPUT);
  pinMode(SWITCH1_PIN, INPUT);
  pinMode(SWITCH2_PIN, INPUT);
  pinMode(SWITCH2_PIN, INPUT | FUNCTION_3 );
  pinMode(PCF8574_INT_PIN, INPUT);
  //delay(1000);
  attachInterrupt(digitalPinToInterrupt(PCF8574_INT_PIN), keyPressedOnPCF8574, FALLING);
  pcf8574_irq_enabled = true;

  client.executeDelayed( ACRELAY_RATE, AcRelayHandler );
  client.executeDelayed( DOOR_RATE, gateSensorHandler );
  //ESP
  ESP.wdtDisable();
  ESP.wdtEnable(WDTO_8S);
  LOG("START\n");
}

void loop(){
  client.loop();
}
