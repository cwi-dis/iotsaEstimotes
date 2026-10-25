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

const char *periodNames[] = {
  "seconds",
  "minutes",
  "hours",
  "days",
  "weeks"
};

// Structure of the packet comes from https://github.com/Estimote/estimote-specs/blob/master/estimote-nearable.js
#pragma pack(push, 1)
struct NearableAdvertisement {
  uint16_t companyID;
  uint8_t frameType;
  uint8_t nearableID[8];
  uint8_t hardwareVersion;
  uint8_t firmwareVersion;
  uint8_t tempLo;
  uint8_t tempHiAndVoltage;
  uint8_t voltageAndMoving;
  int8_t xAccelleration;
  int8_t yAccelleration;
  int8_t zAccelleration;
  uint8_t curMovementDuration;
  uint8_t prevMovementDuration;
};
#pragma pack(pop)

#if 0
static uint32_t dontScanBefore;
static bool continueScanning = false;
#endif
static bool isScanning;
static uint32_t startScanAt;

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

void IotsaEstimoteMod::_clear() {
  nKnownEstimote = nNewEstimote = 0;
  if (estimotes) free(estimotes);
  estimotes = NULL;
}

// The index of the estimote with this id among [from, to), or -1.
int IotsaEstimoteMod::_find(const uint8_t *id, int from, int to) {
  for (int i=from; i<to; i++) {
    if (memcmp(id, estimotes[i].id, 8) == 0) return i;
  }
  return -1;
}

#ifdef IOTSA_WITH_WEB
// Add (a newly seen estimote to the tracked ones) and Clear (all of them):
// setting up for a session, which needs "estimotes" (lateSetup()).
void IotsaEstimoteMod::formHandler_args(IotsaWebContext& ctx) {
  if (ctx.argAction("add", IotsaContext::Saved)) {
    String id;
    ctx.argPeek("add", id);
    uint8_t idbuf[8];
    _hex2id(id, idbuf);
    int idx = _find(idbuf, nKnownEstimote, nKnownEstimote+nNewEstimote);
    if (idx < 0) {
      ctx.error(404, "Estimote " + id + " has not been seen.");
      return;
    }
    // The new one moves to the end of the tracked ones.
    struct Estimote tmp = estimotes[nKnownEstimote];
    estimotes[nKnownEstimote] = estimotes[idx];
    estimotes[idx] = tmp;
    nKnownEstimote++;
    nNewEstimote--;
  }
  if (ctx.argConfirmedAction("clear")) {
    _clear();
    // A confirmed action counts as Transient: save it here.
    configSave();
    ctx.feedback("All estimotes removed.");
  }
}

void IotsaEstimoteMod::formHandler_fields(IotsaWebContext& ctx) {
  if (ctx.includesEveryday()) {
    ctx << "<p><a href='/estimote'>Reload</a></p>";
    if (nKnownEstimote == 0) {
      ctx << "<p>No estimotes tracked.</p>";
      return;
    }
    String message = "<ol>";
    for (int i=0; i<nKnownEstimote; i++) {
      String id;
      _id2hex(estimotes[i].id, id);
      message += "<li>" + id + ", x=" + String(estimotes[i].x) + " y=" + String(estimotes[i].y) + " z=" + String(estimotes[i].z) + ", moving=" + String(estimotes[i].moving) + ", curDur=" + String(estimotes[i].curMoveDuration) + " " + periodNames[estimotes[i].curMovePeriod] + ", prevDur=" + String(estimotes[i].prevMoveDuration) + " " + periodNames[estimotes[i].prevMovePeriod] + ", temp=" + String(estimotes[i].temp) + ", voltage=" + String(estimotes[i].voltage) + (estimotes[i].voltageStress ? " (radio on)" : " (idle)") + "</li>";
    }
    message += "</ol>";
    ctx << message;
    return;
  }
  ctx << "<h3>Newly seen estimotes</h3>";
  if (nNewEstimote == 0) ctx << "<p>None.</p>";
  for (int i=nKnownEstimote; i<nKnownEstimote+nNewEstimote; i++) {
    String id;
    _id2hex(estimotes[i].id, id);
    ctx.showButton("add", "Add " + id, IotsaContext::Saved, id.c_str());
  }
  if (nKnownEstimote) ctx.showConfirmedButton("clear", "Remove all tracked estimotes", IotsaContext::Saved);
}
#endif // IOTSA_WITH_WEB

String IotsaEstimoteMod::info() {
  String message = "<p>Built with estimote module. See <a href=\"/estimote\">/estimote</a> to change devices and settings, <a href=\"/api/estimote\">/api/estimote</a> for the tracked/observed estimote configuration, or <a href=\"/api/readings\">/api/readings</a> for live sensor readings.</p>";
  return message;
}

void IotsaEstimoteMod::setup() {
  configLoad();

  NimBLEDevice::init("");
  pBLEScan = NimBLEDevice::getScan(); //create new scan
  pBLEScan->setScanCallbacks(this);
  isScanning = false;
  startScanAt = millis() + BLESCAN_DURATION_NOSCAN;
  pBLEScan->setActiveScan(true); //active scan uses more power, but get results faster
  pBLEScan->setInterval(155);
  pBLEScan->setWindow(151);  // less or equal setInterval value
}

void _fillValues(JsonObject repl, Estimote *ep) {
  repl["x"] = ep->x;
  repl["y"] = ep->y;
  repl["z"] = ep->z;
  repl["moving"] = ep->moving;
  repl["curMoveDuration"] = ep->curMoveDuration;
  repl["curMoveScale"] = periodNames[ep->curMovePeriod];
  repl["prevMoveDuration"] = ep->prevMoveDuration;
  repl["prevMoveScale"] = periodNames[ep->prevMovePeriod];
  repl["temp"] = ep->temp;
  repl["voltage"] = ep->voltage;
  repl["voltageUnderStress"] = ep->voltageStress;
}

void _fillReply(JsonObject container, Estimote *ep) {
  String id;
  _id2hex(ep->id, id);
  JsonObject repl = container[id].to<JsonObject>();
  _fillValues(repl, ep);
}

#ifdef IOTSA_WITH_WEB
// All tracked estimotes, keyed by id, or with ?id= only that one. A raw web
// handler, because the API context has no query arguments; it is open to
// everyone on the network.
void IotsaEstimoteMod::readingsHandler() {
  IotsaWebServer *server = app.server;
  JsonDocument doc;
  JsonObject root = doc.to<JsonObject>();
  if (server->hasArg("id")) {
    String idWanted = server->arg("id");
    uint8_t idbuf[8];
    _hex2id(idWanted, idbuf);
    int idx = _find(idbuf, 0, nKnownEstimote);
    if (idx < 0) {
      server->send(404, "text/plain", "Unknown estimote " + idWanted);
      return;
    }
    _fillValues(root, estimotes+idx);
  } else {
    for (int i=0; i<nKnownEstimote; i++) {
      _fillReply(root, estimotes+i);
    }
  }
  String body;
  serializeJson(doc, body);
  server->send(200, "application/json", body);
}
#endif // IOTSA_WITH_WEB

// The configuration: the ids of the tracked estimotes, and of the newly seen
// ones that can be added. The sensor values are on /api/readings.
void IotsaEstimoteMod::getHandler(IotsaApiContext& ctx) {
  JsonArray idList = ctx.reply()["estimotes"].to<JsonArray>();
  for (int i=0; i<nKnownEstimote; i++) {
    String id;
    _id2hex(estimotes[i].id, id);
    idList.add(id);
  }
  JsonArray unknownList = ctx.reply()["newEstimotes"].to<JsonArray>();
  for (int i=nKnownEstimote; i<nKnownEstimote+nNewEstimote; i++) {
    String id;
    _id2hex(estimotes[i].id, id);
    unknownList.add(id);
  }
}

// estimotes: the ids to track, replacing the list (the newly seen ones are
// forgotten too).
void IotsaEstimoteMod::putHandler(IotsaApiContext& ctx) {
  if (!ctx.argAction("estimotes", IotsaContext::Saved)) return;
  JsonVariant v = ctx.request()["estimotes"];
  if (!v.is<JsonArray>()) {
    ctx.error(400, "estimotes: a list of ids");
    return;
  }
  JsonArray ids = v.as<JsonArray>();
  for (JsonVariant id : ids) {
    if (!id.is<const char *>() || strlen(id.as<const char *>()) != 16) {
      ctx.error(400, "estimotes: ids of 16 hex digits");
      return;
    }
  }
  _clear();
  if (ids.size() == 0) return;
  estimotes = (struct Estimote *)calloc(ids.size(), sizeof(struct Estimote));
  if (estimotes == NULL) {
    ctx.error(500, "out of memory");
    return;
  }
  nKnownEstimote = ids.size();
  for (int i=0; i<nKnownEstimote; i++) {
    _hex2id(ids[i].as<String>(), estimotes[i].id);
  }
}

void IotsaEstimoteMod::lateSetup() {
  name = "estimote";
  // /estimote (the module page) is registered by this get=true api.setup().
  api.setup("estimote", true, true);
  changesNeedRight("estimotes");
#ifdef IOTSA_WITH_WEB
  addWebHandler("/api/readings", [this]() { readingsHandler(); }, IOTSA_RIGHT_NONE, true);
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
      IOTSA_LOG("estimote", "out of memory");
      return;
    }
    for(int i=0; i<nKnownEstimote; i++) {
      String name = "id_" + String(i);
      String byteString;
      cf.get(name, byteString, "0000000000000000");
      _hex2id(byteString, estimotes[i].id);
      estimotes[i].x = estimotes[i].y = estimotes[i].z = 0;
      estimotes[i].moving = false;
      estimotes[i].curMoveDuration = estimotes[i].prevMoveDuration = 0;
      estimotes[i].temp = 0;
      estimotes[i].voltage = 0;
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
    cf.put(name, byteString);
    ep++;
  }
}

void _parseDuration(int& period, int& duration, uint8_t raw) {
      int number = raw & 0b00111111;
      int code = (raw & 0b11000000) >> 6;
      duration = number;
      // 3 is special: number < 32 means days, >= 32 means weeks+32
      if (code == 3 && number >= 32) {
        period = 4;
        number = number - 32;
      } else {
        period = code;
      }
}

void _parsePacket(struct Estimote *ep, struct NearableAdvertisement *pkt) {
  int tempRaw = (pkt->tempHiAndVoltage & 0xf) << 8 | pkt->tempLo;
  if (tempRaw > 2047) {
    tempRaw -= 4096;
  }
  ep->temp = tempRaw / 16.0;
  ep->moving = pkt->voltageAndMoving & 0b01000000;
  ep->voltageStress = pkt->voltageAndMoving & 0b10000000;
  int voltRaw = ((pkt->voltageAndMoving & 0b00111111) << 4) | ((pkt->tempHiAndVoltage & 0b11110000) >> 4);
  ep->voltage = (3 * 1.2 * voltRaw) / 1023;
  ep->x = 0.015621 * pkt->xAccelleration;
  ep->y = 0.015621 * pkt->yAccelleration;
  ep->z = 0.015621 * pkt->zAccelleration;
  _parseDuration(ep->curMovePeriod, ep->curMoveDuration, pkt->curMovementDuration);
  _parseDuration(ep->prevMovePeriod, ep->prevMoveDuration, pkt->prevMovementDuration);

}

void IotsaEstimoteMod::_sensorData(struct NearableAdvertisement *pkt) {
  struct Estimote *ep = estimotes;
  int n = nKnownEstimote + nNewEstimote;
  while(n-- > 0) {
    if (memcmp(pkt->nearableID, ep->id, 8) == 0) {
      ep->seen = true;
      _parsePacket(ep, pkt);

      IOTSA_LOG_DEBUG("estimote", "num=%d x=%f y=%f z=%f", (int)(ep-estimotes), ep->x, ep->y, ep->z);
      int idx = ep-estimotes;
      return;
    }
    ep++;
  }
  // A new Estimote we have not seen before.
  n = nKnownEstimote + nNewEstimote + 1;
  ep = (struct Estimote *)realloc((void *)estimotes, n * sizeof(struct Estimote));
  if (ep == NULL) {
    IOTSA_LOG("estimote", "out of memory");
    return;
  }
  nNewEstimote++;
  estimotes = ep;
  ep = ep + (n-1);
  memcpy(ep->id, pkt->nearableID, 8);
  _parsePacket(ep, pkt);
  ep->seen = true;
  IOTSA_LOG("estimote", "new %2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x x=%f y=%f z=%f", ep->id[0], ep->id[1], ep->id[2], ep->id[3], ep->id[4], ep->id[5], ep->id[6], ep->id[7], ep->x, ep->y, ep->z);
}

void IotsaEstimoteMod::loop() {
  //
  // If we have a pending transmission we stop scanning (if we are scanning) to 
  // free the radio, we prepare for sending the DMX packet and we schedule
  // restart of the scan
  //

  if (!isScanning && millis() > startScanAt) {
    pBLEScan->clearResults();
    isScanning = pBLEScan->start(BLESCAN_MAX_DURATION);
    IOTSA_LOG_DEBUG("estimote", "scan started");
  }
}

void IotsaEstimoteMod::onScanEnd(const NimBLEScanResults& scanResults, int reason) {
  startScanAt = millis() + BLESCAN_DURATION_NOSCAN;
  isScanning = false;
}

void IotsaEstimoteMod::onResult(const NimBLEAdvertisedDevice *advertisedDevice) {
  std::string manufacturerDataString(advertisedDevice->getManufacturerData());
  uint8_t *manufacturerData = (uint8_t *)manufacturerDataString.data();
  uint8_t manufacturerDataLength = (uint8_t)manufacturerDataString.length();
  if (manufacturerDataLength < sizeof(NearableAdvertisement)) {
    return;
  }
  // This runs in the BLE task; the estimotes array belongs to the loop task
  // (the web and REST handlers read it, _sensorData() reallocates it). So a
  // copy of the advertisement goes to the loop (cwi-dis/iotsa#236).
  NearableAdvertisement adv;
  memcpy(&adv, manufacturerData, sizeof(adv));
  if (adv.companyID != ID_ESTIMOTE) {
    return;
  }
  IotsaApplication::postToLoop([this, adv]() mutable { _sensorData(&adv); });
}