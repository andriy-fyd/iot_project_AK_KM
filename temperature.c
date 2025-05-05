#include "temperature.h"
#include "sl_sensor_rht.h"
#include "app_log.h"

int16_t get_ble_temperature(void) {
  uint32_t rh;
  int32_t t;
  sl_status_t sc = sl_sensor_rht_get(&rh, &t);
  if (sc != SL_STATUS_OK) {
      app_log_error("sl_sensor_rht_get failed with code: 0x%X\n", sc);
      return -32768;
  }

  if (sl_sensor_rht_get(&rh, &t) != SL_STATUS_OK) {
    app_log_error("Temperature read failed!\n");
    return -32768; // Erreur selon la spécification BLE
  }

  int16_t ble_temp = (int16_t)(t / 10); // car t est en milli-degrés C, BLE veut centièmes
  app_log_info("Temperature read and converted: %d (BLE format)\n", ble_temp);
  return ble_temp;
}
