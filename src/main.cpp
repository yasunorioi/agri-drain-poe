// agri-drain-poe — drainage (排液) gauge node on M5Stack ATOM PoE.
// A DFRobot SEN0575 tipping-bucket rain gauge repurposed to measure
// irrigation drainage: tips × ml_per_tip → cumulative volume, published
// to MQTT + UECS-CCM. Fork of agri-rain-poe; most of the network / UI /
// publisher plumbing lives in agri-node-poe-core.

#include <Arduino.h>
#include <AgriNode.h>

#include "config.h"
#include "sensors.h"
#include "mqtt_pub.h"
#include "ccm_pub.h"

const char *FW_NAME    = "agri-drain-poe";
const char *FW_VERSION = "0.1.0";

// globals declared extern in headers
AppConfig g_cfg;
HardwareSerial SensorSerial(1);
bool     g_sensor_detected = false;
uint32_t g_cum_rain_raw    = 0;
uint32_t g_raw_tips        = 0;
uint16_t g_work_minutes    = 0;

// ---- Dashboard / Config hooks --------------------------------------------
static String renderDashboardSensors() {
  String s;
  s.reserve(360);
  char dml[16]; dtostrf(drainageMl(), 1, 1, dml);
  char mlt[12]; dtostrf(g_cfg.ml_per_tip, 1, 2, mlt);
  s  = F("<h3>Drainage</h3><table>");
  s += "<tr><th>Cumulative</th><td>"; s += dml;            s += " mL</td></tr>";
  s += "<tr><th>Raw tips</th><td>";   s += g_raw_tips;     s += "</td></tr>";
  s += "<tr><th>mL / tip</th><td>";   s += mlt;            s += " (configurable)</td></tr>";
  s += "<tr><th>Work time</th><td>";  s += g_work_minutes; s += " min</td></tr>";
  s += "<tr><th>Sensor</th><td>";     s += g_sensor_detected ? "detected" : "NOT detected"; s += "</td></tr>";
  s += F("</table>");
  return s;
}

static String renderConfigSensorRows() {
  String s;
  char mlt[12]; dtostrf(g_cfg.ml_per_tip, 1, 2, mlt);
  s += "<tr><th>mL per tip</th><td><input type=number step=0.01 name=ml_tip value='";
  s += mlt;
  s += "'></td></tr>";
  s += "<tr><th>Order</th><td><input type=number name=ccm_ord value='";
  s += g_cfg.ccm_order;
  s += "'></td></tr>";
  return s;
}

static void applyConfigSensorForm(const String &body) {
  char buf[16] = "";
  agri::parseFormStr(body, "ml_tip", buf, sizeof(buf));
  if (buf[0]) {
    float v = atof(buf);
    if (v > 0.0f && v < 1000.0f) g_cfg.ml_per_tip = v;
  }
  g_cfg.ccm_order = (int16_t)agri::parseFormInt(body, "ccm_ord", g_cfg.ccm_order);
}

static void addStatusFields(JsonObject doc) {
  doc["sensor_ok"]   = g_sensor_detected;
  doc["drainage_ml"] = drainageMl();
  doc["raw_tips"]    = g_raw_tips;
  doc["ml_per_tip"]  = g_cfg.ml_per_tip;
  doc["work_min"]    = g_work_minutes;
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.printf("\n=== %s v%s ===\n", FW_NAME, FW_VERSION);

  agri::Led::begin();
  loadConfig();
  Serial.printf("[CFG] node=%s mqtt_host=%s ccm=%s ml_per_tip=%.2f\n",
                g_cfg.common.node_id,
                g_cfg.common.mqtt_host[0] ? g_cfg.common.mqtt_host : "(unset)",
                g_cfg.common.ccm_enabled ? "on" : "off",
                g_cfg.ml_per_tip);

  sensorsBegin();
  detectSensor();

  agri::Network::begin(g_cfg.common.hostname);
  agri::Network::waitForLease();

  agri::ccmBegin();
  agri::MQTT::begin();

  agri::WebHooks hooks;
  hooks.nodeTitle             = [](){ return FW_NAME; };
  hooks.renderDashboardSensors= renderDashboardSensors;
  hooks.renderConfigSensorRows= renderConfigSensorRows;
  hooks.applyConfigSensorForm = applyConfigSensorForm;
  hooks.addStatusFields       = addStatusFields;
  hooks.saveConfig            = [](){ saveConfig(); };
  agri::WebUI::begin(g_cfg.common, hooks, FW_NAME, FW_VERSION);

  agri::mdnsBegin(g_cfg.common.hostname);
  agri::otaBegin(g_cfg.common.hostname);

  Serial.println("[BOOT] ready");
}

void loop() {
  agri::otaHandle();
  agri::WebUI::handle(agri::Network::link_up, agri::Network::have_lease);

  uint32_t now = millis();

  static uint32_t lastSensorPoll = 0;
  if (now - lastSensorPoll >= 5000) {
    lastSensorPoll = now;
    sensorsPoll();
  }

  if (agri::networkUp() && agri::MQTT::hasHost(g_cfg.common)) {
    if (!agri::MQTT::connected()) {
      static uint32_t lastTry = 0;
      if (now - lastTry > 5000) { lastTry = now; agri::MQTT::reconnect(g_cfg.common); }
    } else {
      agri::MQTT::loop();
      static uint32_t lastPub = 0;
      uint32_t interval = (uint32_t)g_cfg.common.mqtt_interval_s * 1000UL;
      if (now - lastPub >= interval) {
        lastPub = now;
        if (mqttPublishDrain()) agri::Led::flashPublish();
      }
    }
  }

  if (agri::networkUp() && g_cfg.common.ccm_enabled) {
    static uint32_t lastCcm = 0;
    uint32_t interval = (uint32_t)g_cfg.common.ccm_interval_s * 1000UL;
    if (now - lastCcm >= interval) {
      lastCcm = now;
      if (ccmPublish()) agri::Led::flashPublish();
    }
  }

  agri::LedState desired;
  if (!agri::networkUp())                                          desired = agri::LED_NO_LINK;
  else if (!g_sensor_detected)                                     desired = agri::LED_NO_SENSOR;
  else if (agri::MQTT::hasHost(g_cfg.common) && !agri::MQTT::connected()) desired = agri::LED_NO_MQTT;
  else                                                             desired = agri::LED_OK;
  agri::Led::set(desired);

  static uint32_t lastStatus = 0;
  if (now - lastStatus >= 30000) {
    lastStatus = now;
    Serial.printf("[STATUS] link=%d lease=%d mqtt=%d sensor=%d drain=%.1f mL (%lu tips) up=%lus\n",
                  agri::Network::link_up, agri::Network::have_lease,
                  agri::MQTT::connected(), g_sensor_detected, drainageMl(),
                  (unsigned long)g_raw_tips,
                  (unsigned long)(now / 1000));
  }

  delay(20);
}
