//
// Listen to Estimote sensors over BLE, provide the data over HTTP and WiFi.
//
#include "iotsa.h"
#include "iotsaEstimote.h"

IotsaApplication application("Iotsa Estimote Gateway");
IotsaEstimoteMod estimoteMod(application);

// Standard setup() method, hands off most work to the application framework
void setup(void){
  application.setup();
  application.lateSetup();
}

// Standard loop() routine, hands off most work to the application framework
void loop(void){
  application.loop();
}
