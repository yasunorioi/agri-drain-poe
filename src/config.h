// config.h — agri-drain-poe NVS-backed config. Wraps the library's
// CommonConfig with a single Drainage CCM channel plus the tip→volume
// calibration constant.

#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <AgriCommonConfig.h>

struct AppConfig {
  agri::CommonConfig common;
  int16_t            ccm_order;   // single drainage channel
  float              ml_per_tip;  // 1転倒あたりmL (0.2mm × 集水面積 — 実測校正)
};

extern AppConfig g_cfg;

inline void setDefaults() {
  // ccm_rp2350_relay convention: irrigation/water gear sits at
  // sensor_region (11) + 20 = 31 (same region as agri-flow-poe; the
  // drain channel is distinguished by its CCM type, not region).
  agri::commonDefaults(g_cfg.common,
                       "drain_node_01", "agri-drain-01",
                       "agriha/h01/sensor/Drain",
                       /*default_ccm_region=*/31);
  g_cfg.common.mqtt_interval_s = 10;
  g_cfg.ccm_order  = 1;
  g_cfg.ml_per_tip = 3.6f;   // OGMS default: 0.2 mm/tip × collection area
}

inline void loadConfig() {
  setDefaults();
  Preferences p;
  if (!p.begin("drain-cfg", true)) return;
  agri::commonLoad(g_cfg.common, p);
  g_cfg.ccm_order  = p.getShort("ccm_ord", g_cfg.ccm_order);
  g_cfg.ml_per_tip = p.getFloat("ml_tip",  g_cfg.ml_per_tip);
  p.end();
}

inline bool saveConfig() {
  Preferences p;
  if (!p.begin("drain-cfg", false)) return false;
  agri::commonSave(g_cfg.common, p);
  p.putShort("ccm_ord", g_cfg.ccm_order);
  p.putFloat("ml_tip",  g_cfg.ml_per_tip);
  p.end();
  return true;
}
