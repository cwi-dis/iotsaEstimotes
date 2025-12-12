#include "iotsa.h"
#include "iotsaEstimote.h"
#include "iotsaConfigFile.h"

//
// Definition of an Estimote Nearable advertisement packet
#define ID_ESTIMOTE 0x015d

// These two defines basically determine how the radio is split between BLE and WiFi usage

#define BLESCAN_MAX_DURATION 1  // Seconds
#define BLESCAN_DURATION_AFTER_SUCCESS 100 // Milliseconds to continue scanning after a detection
#define BLESCAN_DURATION_NOSCAN 180 // Milliseconds to not use the radio for BLE to allow WiFi

#pragma pack(push, 1)
typedef struct NearableAdvertisement {
  uint16_t companyID;
  uint8_t frameType;
  uint8_t nearableID[8];
  uint8_t hardwareVersion;
  uint8_t firmwareVersion;
  uint8_t tempLo;
  uint8_t tempHi;
  uint8_t voltageAndMoving;
  int8_t xAccelleration;
  int8_t yAccelleration;
  int8_t zAccelleration;
  uint8_t curMovementDuration;
  uint8_t prevMovementDuration;
} NearableAdvertisement;
#pragma pack(pop)

#if 0
static uint32_t dontScanBefore;
static bool continueScanning = false;
#endif
static bool isScanning;
static uint32_t sendDMXat;
static uint32_t startScanAt;

static void scanCompleteCB(BLEScanResults results) {
  startScanAt = millis() + BLESCAN_DURATION_NOSCAN;
  isScanning = false;
}

static void _hex2id(const String& hex, uint8_t *id) {
  const char *p = hex.c_str();
  memset(id, 0, 8);
  for(int i=0; i<8; i++) {
    char c = *p++;
    if (c == 0) break;
    char c2 = *p++;
    if (c2 == 0) break;
    char v = 0;
    if (isdigit(c)) {
      v = (c-'0') << 4;
    } else if (c >= 'A' && c <= 'F') {
      v = (c-'A' + 10) << 4;
    } else if (c >= 'a' && c <= 'f') {
      v = (c-'a' + 10) << 4;
    }
    if (isdigit(c2)) {
      v |= (c2-'0');
    } else if (c2 >= 'A' && c2 <= 'F') {
      v |= (c2-'A' + 10);
    } else if (c2 >= 'a' && c2 <= 'f') {
      v |= (c2-'a' + 10);
    }
    *id++ = v;
  }
}

static void _id2hex(const uint8_t *id, String& hex) {
  for (int i=0; i<8; i++) {
    String c = String(id[i], HEX);
    // Grrr... Need to cater for leading zero
    if (c.length() == 1) c = "0" + c;
    hex += c;
  }
}

#ifdef IOTSA_WITH_WEB
void
IotsaEstimoteMod::handler() {
  bool anyChanged = false;
  if( server->hasArg("Clear")) {
    if (needsAuthentication()) return;
    nKnownEstimote = nNewEstimote = 0;
    if (estimotes) free(estimotes);
    estimotes = NULL;
    anyChanged = true;
  }
  if( server->hasArg("new")) {
    String id = server->arg("new");
    _hex2id(id, estimotes[nKnownEstimote].id);
    nKnownEstimote++;
    nNewEstimote = 0;
    anyChanged = true;
  }
  if (anyChanged) configSave();

  String message = "<html><head><title>Estimote module</title></head><body><h1>Estimote module</h1>";
  message += "<form method='get'><input type='submit' value='Reload'></form>";
  message += "<h2>Known Estimotes</h2><ol>";
  for (int i=0; i<nKnownEstimote; i++) {
    String id;
    _id2hex(estimotes[i].id, id);
    message += "<li>" + id + ", x=" + String(estimotes[i].x) + " y=" + String(estimotes[i].y) + " z=" + String(estimotes[i].z) + "</li>";
  }
  message += "</ol><form method='get'><input type='submit' name='Clear' value='Clear'></form>";

  message += "<h2>Unknown Estimotes</h2><ul>";
  for (int i=nKnownEstimote; i<nKnownEstimote+nNewEstimote; i++) {
    String id;
    _id2hex(estimotes[i].id, id);
    message += "<li>" + id + "<form method='get'><input type='hidden' name='new' value='" + id + "'><input type='submit' value='Add' name='add'></form></li>";
  }
  server->send(200, "text/html", message);
}

String IotsaEstimoteMod::info() {
  String message = "<p>Built with estimote module. See <a href=\"/estimote\">/estimote</a> to change devices and settings or <a href=\"/api/estimote\">/api/estimote</a> for REST interface.</p>";
  return message;
}
#endif // IOTSA_WITH_WEB

void IotsaEstimoteMod::setup() {
  configLoad();

  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan(); //create new scan
  pBLEScan->setAdvertisedDeviceCallbacks(this);
  isScanning = false;
  startScanAt = millis() + BLESCAN_DURATION_NOSCAN;
  pBLEScan->setActiveScan(true); //active scan uses more power, but get results faster
  pBLEScan->setInterval(155);
  pBLEScan->setWindow(151);  // less or equal setInterval value
}

#ifdef IOTSA_WITH_API
bool IotsaEstimoteMod::getHandler(const char *path, JsonObject& reply) {
  JsonArray ids = reply.createNestedArray("estimotes");
  JsonArray newIds = reply.createNestedArray("newEstimotes");
  for (int i=0; i<nKnownEstimote+nNewEstimote; i++) {
    String id;
    _id2hex(estimotes[i].id, id);
    if (i < nKnownEstimote) {
      ids.add(id);
    } else {
      newIds.add(id);
    }
  }
  return true;
}

bool IotsaEstimoteMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply) {
  bool anyChanged = false;
  JsonObject reqObj = request.as<JsonObject>();
  if (reqObj.containsKey("estimotes")) {
    JsonArray ids = reqObj["estimotes"];
    if (estimotes) free(estimotes);
    estimotes = NULL;
    nKnownEstimote = nNewEstimote = 0;
    nKnownEstimote = ids.size();
    if (nKnownEstimote > 0) {
      estimotes = (struct Estimote *)malloc(nKnownEstimote*sizeof(struct Estimote));
      for(int i=0; i<nKnownEstimote; i++) {
        const String& id = ids[i];
        _hex2id(id, estimotes[i].id);
        estimotes[i].x = estimotes[i].y = estimotes[i].z = 0;
        estimotes[i].seen = false;
      }
    }
    anyChanged = true;
  }
  if (anyChanged) configSave();
  return anyChanged;
}
#endif // IOTSA_WITH_API

void IotsaEstimoteMod::serverSetup() {
#ifdef IOTSA_WITH_WEB
  server->on("/estimote", std::bind(&IotsaEstimoteMod::handler, this));
#endif
#ifdef IOTSA_WITH_API
  api.setup("/api/estimote", true, true);
  name = "estimote";
#endif
}

void IotsaEstimoteMod::configLoad() {
  IotsaConfigFileLoad cf("/config/estimote.cfg");
  if (estimotes) {
    free((void *)estimotes);
    estimotes = NULL;
  }
  nKnownEstimote = 0;
  nNewEstimote = 0;
  // xxxjack should use object interface
  cf.get("nEstimote", nKnownEstimote, 0);
  if (nKnownEstimote > 0) {
    estimotes = (struct Estimote *)malloc(nKnownEstimote*sizeof(struct Estimote));
    if (estimotes == NULL) {
      IotsaSerial.println("out of memory");
      return;
    }
    for(int i=0; i<nKnownEstimote; i++) {
      String name = "id_" + String(i);
      String byteString;
      cf.get(name, byteString, "0000000000000000");
  //    IFDEBUG IotsaSerial.print("load ");
  //    IFDEBUG IotsaSerial.print(name);
  //    IFDEBUG IotsaSerial.print(", value ");
  //    IFDEBUG IotsaSerial.println(byteString);
      _hex2id(byteString, estimotes[i].id);
      estimotes[i].x = estimotes[i].y = estimotes[i].z = 0;
      estimotes[i].seen = false;
    }
  }
}

void IotsaEstimoteMod::configSave() {
  IotsaConfigFileSave cf("/config/estimote.cfg");
  cf.put("nEstimote", nKnownEstimote);
  struct Estimote *ep = estimotes;
  for (int i=0; i<nKnownEstimote; i++) {
    String name = "id_" + String(i);
    String byteString;
    _id2hex(ep->id, byteString);
//    IFDEBUG IotsaSerial.print("save ");
//    IFDEBUG IotsaSerial.print(name);
//    IFDEBUG IotsaSerial.print(", value ");
//    IFDEBUG IotsaSerial.println(byteString);
    cf.put(name, byteString);
    ep++;
  }
}

void IotsaEstimoteMod::setDMX(IotsaDMXMod *_dmx, int portIndex) {
  dmx = _dmx;
  dmx->setDMXInputHandler(portIndex, sliderBuffer, 512);
}

bool IotsaEstimoteMod::_allSensorsSeen() {
  struct Estimote *ep = estimotes;
  int n = nKnownEstimote;
  while(n-- > 0) {
    if (!ep->seen) return false;
    ep++;
  }
  return true;
}

void IotsaEstimoteMod::_resetSensorsSeen() {
  struct Estimote *ep = estimotes;
  int n = nKnownEstimote;
  while(n-- > 0) {
    ep->seen = false;
    ep++;
  }
}

void IotsaEstimoteMod::_sensorData(uint8_t *id, int8_t x, int8_t y, int8_t z) {
  struct Estimote *ep = estimotes;
  int n = nKnownEstimote + nNewEstimote;
  while(n-- > 0) {
    if (memcmp(id, ep->id, 8) == 0) {
      ep->seen = true;
      ep->x = x;
      ep->y = y;
      ep->z = z;
      IFDEBUG IotsaSerial.printf("Estimote num=%d x=%d y=%d z=%d\n", (ep-estimotes), ep->x, ep->y, ep->z);
      int idx = ep-estimotes;
      sliderBuffer[6*idx+0] = x > 0 ? x*2 : 0;
      sliderBuffer[6*idx+1] = x < 0 ? -x*2 : 0;
      sliderBuffer[6*idx+2] = y > 0 ? y*2 : 0;
      sliderBuffer[6*idx+3] = y < 0 ? -y*2 : 0;
      sliderBuffer[6*idx+4] = z > 0 ? z*2 : 0;
      sliderBuffer[6*idx+5] = z < 0 ? -z*2 : 0;
      if (n < nKnownEstimote && dmx) {
        sendDMXat = millis() + BLESCAN_DURATION_AFTER_SUCCESS;
      }
      return;
    }
    ep++;
  }
  // A new Estimote we have not seen before.
  nNewEstimote++;
  n = nKnownEstimote + nNewEstimote;
  if (n == 1) {
    ep = (struct Estimote *)malloc(sizeof(struct Estimote));
  } else {
    ep = (struct Estimote *)realloc((void *)estimotes, n * sizeof(struct Estimote));
  }
  if (ep == NULL) {
    IotsaSerial.println("out of memory");
    return;
  }
  estimotes = ep;
  ep = ep + (n-1);
  memcpy(ep->id, id, 8);
  ep->x = x;
  ep->y = y;
  ep->z = z;
  ep->seen = true;
  IFDEBUG IotsaSerial.printf("New Estimote %2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x x=%d y=%d z=%d\n", ep->id[0], ep->id[1], ep->id[2], ep->id[3], ep->id[4], ep->id[5], ep->id[6], ep->id[7], ep->x, ep->y, ep->z);
}

void IotsaEstimoteMod::loop() {
#if 0
  if (!isScanning && wantToSendDMX) {
    wantToSendDMX = false;
    dmx->dmxInputChanged();
    return;
  }
#endif
  //
  // If we have a pending transmission we stop scanning (if we are scanning) to 
  // free the radio, we prepare for sending the DMX packet and we schedule
  // restart of the scan
  //
  if (sendDMXat != 0 && millis() > sendDMXat) {
    if (isScanning) {
      pBLEScan->stop();
      isScanning = false;
      startScanAt = millis() + BLESCAN_DURATION_NOSCAN;
    }
    if (dmx) dmx->dmxInputChanged();
    sendDMXat = 0;
  }
  if (!isScanning && millis() > startScanAt) {
    pBLEScan->clearResults();
    _resetSensorsSeen();
    IFDEBUG IotsaSerial.print("SCAN ");
    isScanning = pBLEScan->start(BLESCAN_MAX_DURATION, scanCompleteCB);
    IFDEBUG IotsaSerial.println("started");
  }
}

void IotsaEstimoteMod::onResult(BLEAdvertisedDevice advertisedDevice) {
  //IFDEBUG IotsaSerial.printf("Advertised Device: %s \n", advertisedDevice.toString().c_str());
  std::string manufacturerDataString(advertisedDevice.getManufacturerData());
  uint8_t *manufacturerData = (uint8_t *)manufacturerDataString.data();
  uint8_t manufacturerDataLength = (uint8_t)manufacturerDataString.length();
  if (manufacturerDataLength < sizeof(NearableAdvertisement)) {
    //IFDEBUG IotsaSerial.println("Too short");
    return;
  }
  NearableAdvertisement *adv = (NearableAdvertisement *)manufacturerData;
  if (adv->companyID != ID_ESTIMOTE) {
    //IFDEBUG IotsaSerial.println("Not estimote");
    return;
  }
#if 0
  IFDEBUG IotsaSerial.printf("Estimote %2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x x=%d y=%d z=%d\n", adv->nearableID[0], adv->nearableID[1], adv->nearableID[2], adv->nearableID[3], adv->nearableID[4], adv->nearableID[5], adv->nearableID[6], adv->nearableID[7], adv->xAccelleration, adv->yAccelleration, adv->zAccelleration);
#endif
  _sensorData(adv->nearableID, adv->xAccelleration, adv->yAccelleration, adv->zAccelleration);
}