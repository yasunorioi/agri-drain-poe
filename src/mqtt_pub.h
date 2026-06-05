// mqtt_pub.h — drainage MQTT publisher. Library handles connect/LWT;
// we just build and ship the JSON payload.

#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <AgriNode.h>
#include "config.h"
#include "sensors.h"

inline bool mqttPublishDrain() {
  if (!agri::MQTT::hasHost(g_cfg.common) || !agri::MQTT::connected()) return false;

  JsonDocument doc;
  doc["drainage_ml"] = drainageMl();
  doc["raw_tips"]    = g_raw_tips;
  doc["ml_per_tip"]  = g_cfg.ml_per_tip;
  doc["work_min"]    = g_work_minutes;
  doc["node_id"]     = g_cfg.common.node_id;
  doc["uptime_s"]    = millis() / 1000;

  char payload[256];
  size_t n = serializeJson(doc, payload, sizeof(payload));
  bool ok = agri::MQTT::mqtt.publish(g_cfg.common.mqtt_topic_prefix,
                                     (const uint8_t*)payload, n, true);
  Serial.printf("[MQTT] %s %s (%u bytes)\n",
                g_cfg.common.mqtt_topic_prefix, ok ? "OK" : "FAIL",
                (unsigned)n);
  return ok;
}
