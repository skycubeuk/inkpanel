#pragma once

#include "esphome/components/display/display_buffer.h"
#include "xteink.h"

namespace esphome {
namespace xteink {

class XteinkDisplay;
using xteink_writer_t = std::function<void(XteinkDisplay &)>;

/// Two operating modes:
///  - lambda mode (a `lambda:` is configured): upstream behaviour. Every update()
///    repaints the whole frame, refreshes blocking, then clears the framebuffer.
///  - external-draw mode (no lambda, e.g. LVGL): the framebuffer persists between
///    updates and refreshes run asynchronously. update() returns in ~25 ms while
///    the panel runs its waveform; loop() notices completion and fires any refresh
///    requested meanwhile, once, with the latest frame (latest state wins).
///    Touch and the rest of ESPHome keep running during the waveform.
class XteinkDisplay : public display::DisplayBuffer {
 public:
  // Same numbering as ngxson/esphome-component-xteink: it.set_refresh_mode(0|1|2)
  enum RefreshMode { FULL_REFRESH = 0, HALF_REFRESH = 1, FAST_REFRESH = 2 };

  void set_parent(Xteink *parent) { this->parent_ = parent; }
  void set_writer(xteink_writer_t &&writer) { this->writer_local_ = writer; }
  /// Mode for the next refresh. In external-draw mode the strongest mode requested
  /// since the last refresh started wins (FULL > HALF > FAST), then it resets to FAST.
  void set_refresh_mode(int mode) { this->refresh_mode_ = static_cast<RefreshMode>(mode); }
  display::DisplayType get_display_type() override { return display::DisplayType::DISPLAY_TYPE_BINARY; }
  int get_width_internal() override { return this->parent_->display().getDisplayWidth(); }
  int get_height_internal() override { return this->parent_->display().getDisplayHeight(); }
  void draw_absolute_pixel_internal(int x, int y, Color color) override;
  /// Fast path for LVGL flushes (RGB565, no display rotation): writes 1-bit pixels
  /// straight into the framebuffer instead of one virtual call per pixel.
  void draw_pixels_at(int x_start, int y_start, int w, int h, const uint8_t *ptr, display::ColorOrder order,
                      display::ColorBitness bitness, bool big_endian, int x_offset, int y_offset,
                      int x_pad) override;
  void setup() override;
  void loop() override;
  void update() override;
  void on_shutdown() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  /// True while a refresh is running on the panel or queued behind one.
  bool is_refreshing() const { return this->in_flight_ || this->queued_; }

  uint64_t update_count{0};  ///< number of panel refreshes so far, usable from the lambda

 protected:
  void update_lambda_mode_();
  /// Complete a finished async refresh and start a queued one. Non-blocking.
  void pump_();
  /// Close out the in-flight refresh (requires FreeInk SDK >= 39606d5, which passes
  /// the displayed frame, not the live framebuffer, to the driver's displayFinish()).
  void finish_in_flight_();

  Xteink *parent_{nullptr};
  optional<xteink_writer_t> writer_local_{};
  RefreshMode refresh_mode_{HALF_REFRESH};
  bool has_drawn_{false};

  // external-draw (async) mode state
  RefreshMode requested_mode_{FAST_REFRESH};
  bool queued_{false};
  bool in_flight_{false};
  uint8_t *shown_{nullptr};  ///< copy of the frame last pushed to the panel
  uint32_t started_ms_{0};
};

}  // namespace xteink
}  // namespace esphome
