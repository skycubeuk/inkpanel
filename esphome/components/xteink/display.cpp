#include "display.h"

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#include <cstdlib>
#include <cstring>

namespace esphome {
namespace xteink {

static const char *const TAG = "xteink.display";

static EInkDisplay::RefreshMode to_sdk(XteinkDisplay::RefreshMode m) {
  switch (m) {
    case XteinkDisplay::FULL_REFRESH:
      return EInkDisplay::FULL_REFRESH;
    case XteinkDisplay::HALF_REFRESH:
      return EInkDisplay::HALF_REFRESH;
    default:
      return EInkDisplay::FAST_REFRESH;
  }
}

void XteinkDisplay::setup() {
  if (this->writer_local_.has_value())
    return;  // lambda mode keeps the upstream blocking path; no extra buffers
  const uint32_t n = this->parent_->display().getBufferSize();
  this->shown_ = static_cast<uint8_t *>(malloc(n));
  if (this->shown_ == nullptr) {
    ESP_LOGE(TAG, "Could not allocate %u bytes for async refresh", (unsigned) n);
    this->mark_failed();
  }
}

void XteinkDisplay::draw_absolute_pixel_internal(int x, int y, Color color) {
  auto &epd = this->parent_->display();
  if (x < 0 || y < 0 || x >= epd.getDisplayWidth() || y >= epd.getDisplayHeight())
    return;
  uint8_t *buf = epd.getFrameBuffer();
  const uint32_t idx = y * epd.getDisplayWidthBytes() + (x >> 3);
  const uint8_t bit = 0x80 >> (x & 7);
  if (color.is_on()) {
    buf[idx] &= ~bit;  // 0 = black
  } else {
    buf[idx] |= bit;  // 1 = white
  }
}

void XteinkDisplay::draw_pixels_at(int x_start, int y_start, int w, int h, const uint8_t *ptr,
                                   display::ColorOrder order, display::ColorBitness bitness, bool big_endian,
                                   int x_offset, int y_offset, int x_pad) {
  if (bitness != display::COLOR_BITNESS_565 || this->rotation_ != display::DISPLAY_ROTATION_0_DEGREES ||
      this->is_clipping()) {
    display::DisplayBuffer::draw_pixels_at(x_start, y_start, w, h, ptr, order, bitness, big_endian, x_offset,
                                           y_offset, x_pad);
    return;
  }
  auto &epd = this->parent_->display();
  const int width = epd.getDisplayWidth();
  const int height = epd.getDisplayHeight();
  const int row_bytes = epd.getDisplayWidthBytes();
  uint8_t *buf = epd.getFrameBuffer();
  const size_t line_stride = x_offset + w + x_pad;  // source pixels per line
  const uint8_t *src = ptr;
  for (int y = 0; y < h; y++) {
    const int py = y_start + y;
    if (py < 0 || py >= height)
      continue;
    // Byte order does not matter: any nonzero RGB565 value is ink (matches Color::is_on()).
    const uint8_t *row = src + ((y_offset + y) * line_stride + x_offset) * 2;
    uint8_t *dst = buf + py * row_bytes;
    for (int x = 0; x < w; x++) {
      const int px = x_start + x;
      if (px < 0 || px >= width)
        continue;
      const uint8_t bit = 0x80 >> (px & 7);
      if (row[2 * x] | row[2 * x + 1]) {
        dst[px >> 3] &= ~bit;
      } else {
        dst[px >> 3] |= bit;
      }
    }
  }
}

void XteinkDisplay::update() {
  if (this->writer_local_.has_value()) {
    this->update_lambda_mode_();
    return;
  }
  if (this->refresh_mode_ < this->requested_mode_)
    this->requested_mode_ = this->refresh_mode_;
  this->refresh_mode_ = FAST_REFRESH;
  this->queued_ = true;
  this->pump_();
}

void XteinkDisplay::loop() {
  if (this->in_flight_ || this->queued_)
    this->pump_();
}

void XteinkDisplay::finish_in_flight_() {
  // BUSY has already released, so this only closes the refresh (PTOUT, OLD-plane
  // sync). The SDK re-syncs the controller's previous-frame plane from its own copy
  // of the displayed frame, so LVGL may already be drawing the next one.
  this->parent_->display().waitRefreshComplete();
  this->in_flight_ = false;
  ESP_LOGD(TAG, "refresh done after %u ms", (unsigned) (millis() - this->started_ms_));
}

void XteinkDisplay::pump_() {
  if (this->is_failed())
    return;
  auto &epd = this->parent_->display();
  if (this->in_flight_) {
    if (epd.refreshBusy())
      return;  // waveform still running
    this->finish_in_flight_();
  }
  if (!this->queued_)
    return;

  uint8_t *fb = epd.getFrameBuffer();
  const uint32_t n = epd.getBufferSize();
  RefreshMode mode = this->requested_mode_;
  if (!this->has_drawn_ && mode == FAST_REFRESH)
    mode = HALF_REFRESH;  // the first frame after boot cannot be a differential refresh
  if (mode == FAST_REFRESH && memcmp(fb, this->shown_, n) == 0) {
    this->queued_ = false;  // nothing visible changed (e.g. a press style with no pixels)
    this->requested_mode_ = FAST_REFRESH;
    return;
  }

  this->started_ms_ = millis();
  epd.displayBufferAsync(to_sdk(mode));
  memcpy(this->shown_, fb, n);
  this->in_flight_ = epd.isRefreshPending();  // false if the SDK fell back to a blocking refresh
  this->queued_ = false;
  this->requested_mode_ = FAST_REFRESH;
  this->has_drawn_ = true;
  this->update_count++;
  ESP_LOGD(TAG, "refresh #%u mode=%d started, push took %u ms", (unsigned) this->update_count, (int) mode,
           (unsigned) (millis() - this->started_ms_));
}

void XteinkDisplay::on_shutdown() {
  // Deep sleep / reboot: finish the running refresh correctly, then make sure the
  // frame left on the glass is the latest one.
  if (this->writer_local_.has_value() || this->is_failed())
    return;
  auto &epd = this->parent_->display();
  if (this->in_flight_) {
    while (epd.refreshBusy())
      delay(5);
    this->finish_in_flight_();
  }
  if (this->has_drawn_ && memcmp(epd.getFrameBuffer(), this->shown_, epd.getBufferSize()) != 0) {
    epd.displayBuffer(EInkDisplay::FAST_REFRESH);
    memcpy(this->shown_, epd.getFrameBuffer(), epd.getBufferSize());
  }
  this->queued_ = false;
}

void XteinkDisplay::update_lambda_mode_() {
  auto &epd = this->parent_->display();
  (*this->writer_local_)(*this);
  EInkDisplay::RefreshMode mode = EInkDisplay::FULL_REFRESH;
  if (this->refresh_mode_ == HALF_REFRESH || (this->refresh_mode_ == FAST_REFRESH && !this->has_drawn_)) {
    mode = EInkDisplay::HALF_REFRESH;  // the very first frame cannot be a fast (diff) refresh
  } else if (this->refresh_mode_ == FAST_REFRESH) {
    mode = EInkDisplay::FAST_REFRESH;
  }
  epd.displayBuffer(mode);
  epd.clearScreen(0xFF);  // every update() repaints the whole frame
  this->has_drawn_ = true;
  this->update_count++;
}

void XteinkDisplay::dump_config() {
  LOG_DISPLAY("", "Xteink e-paper", this);
  ESP_LOGCONFIG(TAG, "  Mode: %s", this->writer_local_.has_value() ? "lambda (blocking)" : "external draw (async)");
  ESP_LOGCONFIG(TAG, "  Async refresh supported: %s", YESNO(this->parent_->display().supportsAsyncRefresh()));
  LOG_UPDATE_INTERVAL(this);
}

}  // namespace xteink
}  // namespace esphome
