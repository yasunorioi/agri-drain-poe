// ccm_pub.h — drainage (Drainage, cumulative mL) UECS-CCM publisher.
// "Drainage" is not standard UECS vocabulary — the central (RPI duty
// loop / bridge) just needs a stable name. Adjust the literal here if
// your central uses a different one.

#pragma once

#include <Arduino.h>
#include <AgriNode.h>
#include "config.h"
#include "sensors.h"

inline bool ccmPublish() {
  if (!g_cfg.common.ccm_enabled || !g_sensor_detected) return false;

  char buf[16];
  dtostrf(drainageMl(), 1, 1, buf);

  String xml = agri::ccmEnvelopeOpen();
  xml += agri::ccmDatumNT("Drainage", g_cfg.common.ccm_ntype,
                        g_cfg.common.ccm_room,
                        g_cfg.common.ccm_region,
                        g_cfg.ccm_order,
                        g_cfg.common.ccm_priority,
                        buf);
  xml += agri::ccmEnvelopeClose();
  return agri::ccmSend(xml);
}
