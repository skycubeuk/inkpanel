#include "touch.h"

#include "esphome/core/application.h"
#include "esphome/core/log.h"

#include "driver/gpio.h"
#include "esp_sleep.h"
#include "hal/gpio_ll.h"

namespace esphome {
namespace xteink {

static const char *const TAG = "xteink.touch";

void XteinkTouchscreen::setup() {
  touchscreen::Touchscreen::setup();
  // The SDK reports panel-native coordinates (0..width-1, 0..height-1).
  if (this->x_raw_max_ == this->x_raw_min_) {
    this->x_raw_max_ = this->parent_->display().getDisplayWidth() - 1;
    this->y_raw_max_ = this->parent_->display().getDisplayHeight() - 1;
  }
}

void IRAM_ATTR XteinkTouchscreen::irq_isr_(void *arg) {
  auto *self = static_cast<XteinkTouchscreen *>(arg);
  gpio_ll_intr_disable(&GPIO, self->irq_gpio_);  // level interrupt: fire once until re-armed
  self->irq_enabled_ = false;
  self->irq_fired_ = true;
  self->irq_count_++;
  BaseType_t woken = pdFALSE;
  App.wake_loop_isrsafe(&woken);
  if (woken)
    portYIELD_FROM_ISR();
}

void XteinkTouchscreen::enable_irq_wake(int gpio) {
  const auto pin = static_cast<gpio_num_t>(gpio);
  this->irq_gpio_ = gpio;
  gpio_intr_disable(pin);
  this->lvl_before_ = gpio_get_level(pin);
  // Also sets the pin's interrupt type to LOW level, used by irq_isr_ below.
  esp_err_t e = gpio_wakeup_enable(pin, GPIO_INTR_LOW_LEVEL);
  this->lvl_after_ = gpio_get_level(pin);
  if (e == ESP_OK)
    e = esp_sleep_enable_gpio_wakeup();
  if (e == ESP_OK) {
    const esp_err_t svc = gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
    e = (svc == ESP_OK || svc == ESP_ERR_INVALID_STATE) ? gpio_isr_handler_add(pin, irq_isr_, this) : svc;
  }
  this->irq_setup_err_ = e;
  this->irq_idle_level_ = gpio_get_level(pin);
  ESP_LOGI(TAG, "Touch IRQ wake on GPIO%d: %s (idle level %d)", gpio, esp_err_to_name(e), this->irq_idle_level_);
  if (e != ESP_OK)
    this->irq_gpio_ = -1;
}

void XteinkTouchscreen::arm_irq_wake() {
  if (this->irq_gpio_ < 0)
    return;
  this->irq_wanted_ = true;
  this->irq_fired_ = false;
  this->rearm_irq_if_idle_line_();
}

void XteinkTouchscreen::disarm_irq_wake() {
  this->irq_wanted_ = false;
  if (this->irq_gpio_ >= 0) {
    gpio_intr_disable(static_cast<gpio_num_t>(this->irq_gpio_));
    this->irq_enabled_ = false;
  }
}

// Re-enable only once INT is back HIGH (no finger, no pending report); enabling a
// LOW-level interrupt while the line is still LOW would fire again at once.
void XteinkTouchscreen::rearm_irq_if_idle_line_() {
  if (!this->irq_wanted_ || this->irq_enabled_ || this->irq_gpio_ < 0)
    return;
  const auto pin = static_cast<gpio_num_t>(this->irq_gpio_);
  if (gpio_get_level(pin) == 0)
    return;
  this->irq_enabled_ = true;
  gpio_intr_enable(pin);
}

void XteinkTouchscreen::loop() {
  if (this->irq_fired_) {
    // A finger woke us: process the touch now instead of waiting for the poller,
    // with a fresh GT911 read (the hub's snapshot may be up to one idle poll old).
    this->irq_fired_ = false;
    this->fresh_read_ = true;
    this->store_.touched = true;
    ESP_LOGD(TAG, "touch IRQ #%u -> fresh read", (unsigned) this->irq_count_);
  }
  touchscreen::Touchscreen::loop();
  this->rearm_irq_if_idle_line_();
}

void XteinkTouchscreen::update_touches() {
  if (this->fresh_read_) {
    this->fresh_read_ = false;
    this->parent_->update();
    const unsigned n = this->parent_->input().getTouchSnapshot().count;
    if (n == 0)
      this->irq_empty_++;
    ESP_LOGD(TAG, "fresh read: %u point(s)", n);
  }
  const auto snapshot = this->parent_->input().getTouchSnapshot();
  for (uint8_t i = 0; i < snapshot.count; i++) {
    const auto &p = snapshot.points[i];
    if (p.point.valid) {
      this->add_raw_touch_position_(p.id, p.point.x, p.point.y);
    }
  }
}

void XteinkTouchscreen::dump_config() {
  ESP_LOGCONFIG(TAG, "  INT level before wake setup %d, right after %d", this->lvl_before_, this->lvl_after_);
  ESP_LOGCONFIG(TAG, "  Touch IRQ wake: GPIO%d setup=%s idle_level=%d fired=%u empty=%u level_now=%d wanted=%d enabled=%d",
                this->irq_gpio_, esp_err_to_name(this->irq_setup_err_), this->irq_idle_level_, (unsigned) this->irq_count_,
                (unsigned) this->irq_empty_,
                this->irq_gpio_ >= 0 ? gpio_get_level(static_cast<gpio_num_t>(this->irq_gpio_)) : -1,
                this->irq_wanted_, this->irq_enabled_);
  ESP_LOGCONFIG(TAG, "Xteink touchscreen:\n  Touch controller present: %s",
                YESNO(this->parent_->input().hasTouch()));
  LOG_UPDATE_INTERVAL(this);
}

}  // namespace xteink
}  // namespace esphome
