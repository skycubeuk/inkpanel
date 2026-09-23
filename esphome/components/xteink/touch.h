#pragma once

#include "esphome/components/touchscreen/touchscreen.h"
#include "xteink.h"

namespace esphome {
namespace xteink {

/// GT911 touch (X4 Pro) read through the SDK InputManager, which the hub polls.
class XteinkTouchscreen : public touchscreen::Touchscreen {
 public:
  void set_parent(Xteink *parent) { this->parent_ = parent; }
  void setup() override;
  void loop() override;
  void update_touches() override;
  void dump_config() override;

  /// Touch-to-wake: let the GT911 INT line (held LOW for ~10 ms per report while a
  /// finger is down, HIGH and quiet otherwise) wake the chip from light sleep and
  /// wake ESPHome's main loop. One-shot: the interrupt disables itself when it
  /// fires and is re-armed with arm_irq_wake() (e.g. when the UI goes idle).
  void enable_irq_wake(int gpio);
  void arm_irq_wake();
  void disarm_irq_wake();

 protected:
  static void irq_isr_(void *arg);
  void rearm_irq_if_idle_line_();

  Xteink *parent_{nullptr};
  int irq_gpio_{-1};
  volatile bool irq_fired_{false};
  volatile bool irq_enabled_{false};  // hardware interrupt currently enabled
  bool irq_wanted_{false};            // armed by the owner (idle); re-enabled automatically
  uint32_t irq_empty_{0};             // wakes whose fresh read found no touch
  volatile uint32_t irq_count_{0};
  esp_err_t irq_setup_err_{ESP_FAIL};
  int irq_idle_level_{-1};
  int lvl_before_{-1}, lvl_after_{-1};
  bool fresh_read_{false};
};

}  // namespace xteink
}  // namespace esphome
