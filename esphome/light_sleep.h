#pragma once
// Processor light sleep while the panel is idle (packages/light_sleep.yaml).
// Active (for a while after a touch): a CPU_FREQ_MAX power-management lock keeps
// the CPU at full speed and blocks light sleep, so nothing is slowed down.
// Idle: the lock is released and FreeRTOS tickless idle may light-sleep the chip
// between scheduled work, with Wi-Fi kept associated by modem sleep. The GT911
// INT pin wakes the chip on a touch (XteinkTouch::arm_irq_wake()).
#include "esp_pm.h"
#include "esp_wifi.h"
#include "driver/gpio.h"
#include "esphome/core/log.h"

namespace light_sleep {

inline esp_pm_lock_handle_t &active_lock() {
  static esp_pm_lock_handle_t h = nullptr;
  return h;
}
inline bool &lock_held() {
  static bool held = false;
  return held;
}

inline void set_active(bool on) {
  if (active_lock() == nullptr || lock_held() == on)
    return;  // the lock is reference-counted: keep acquire/release strictly paired
  if (on)
    esp_pm_lock_acquire(active_lock());
  else
    esp_pm_lock_release(active_lock());
  lock_held() = on;
}

// With CONFIG_PM_SLP_DISABLE_GPIO (on in this build) every pad is disconnected
// during light sleep. On the X4 Pro that floats GPIO2 (GT911 power, active LOW)
// and GPIO1 (peripheral rail latch), which powers the touch controller down on
// the first nap; it never answers I2C again. Keep these pads in their active
// configuration through light sleep.
inline void keep_pads_through_sleep() {
  static const int pins[] = {
      1, 2,                   // peripheral rail latch, GT911 power enable (active LOW)
      4, 10,                  // GT911 reset, GT911 INT (also the wake source)
      38, 39,                 // shared I2C: GT911, CW2017 gauge, RTC
      6, 11, 12, 13, 14, 18,  // e-paper BUSY, MOSI, SCLK, CS, RST, DC
      8, 9,                   // frontlight PWM (RC_FAST-clocked, runs through light sleep)
      0, 3, 7,                // buttons (pull-ups)
  };
  for (int p : pins)
    gpio_sleep_sel_dis(static_cast<gpio_num_t>(p));
}

// Beacons to skip while idle, in AP beacon intervals (router beacon 102 ms, so
// 10 = ~1.0 s between radio wakes). IDF treats 0 as 3.
static const uint16_t IDLE_LISTEN_INTERVAL = 10;

// ESPHome hardcodes sta.listen_interval = 0 and rewrites the whole STA config on
// every connect, so the interval has to be re-applied here rather than once at boot.
// It is read by the power-save module, so set it before esp_wifi_set_ps().
inline void set_listen_interval(uint16_t beacons) {
  wifi_config_t conf;
  esp_err_t e = esp_wifi_get_config(WIFI_IF_STA, &conf);
  if (e != ESP_OK) {
    ESP_LOGW("pm", "esp_wifi_get_config: %s", esp_err_to_name(e));
    return;
  }
  if (conf.sta.listen_interval == beacons)
    return;
  conf.sta.listen_interval = beacons;
  e = esp_wifi_set_config(WIFI_IF_STA, &conf);
  ESP_LOGI("pm", "listen_interval %u: %s", beacons, esp_err_to_name(e));
}

// Wi-Fi modem sleep follows the panel: active listens to every beacon (ESPHome's
// power_save_mode light, WIFI_PS_MIN_MODEM), idle only every listen interval
// (WIFI_PS_MAX_MODEM). Transmits are never delayed, so taps are unaffected; only
// traffic from HA to an idle panel waits for the next listen window.
inline void wifi_idle(bool idle) {
  if (idle)
    set_listen_interval(IDLE_LISTEN_INTERVAL);
  esp_err_t e = esp_wifi_set_ps(idle ? WIFI_PS_MAX_MODEM : WIFI_PS_MIN_MODEM);
  if (e != ESP_OK)
    ESP_LOGW("pm", "esp_wifi_set_ps: %s", esp_err_to_name(e));
}

inline void start() {
  keep_pads_through_sleep();
  esp_pm_config_t cfg = {};
  cfg.max_freq_mhz = 240;
  cfg.min_freq_mhz = 80;  // APB stays at 80 MHz, so SPI/I2C timing is unchanged
  cfg.light_sleep_enable = true;
  esp_err_t e = esp_pm_configure(&cfg);
  ESP_LOGI("pm", "esp_pm_configure: %s", esp_err_to_name(e));
  e = esp_pm_lock_create(ESP_PM_CPU_FREQ_MAX, 0, "panel_active", &active_lock());
  ESP_LOGI("pm", "lock create: %s", esp_err_to_name(e));
  set_active(true);
}

}  // namespace light_sleep
