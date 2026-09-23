#pragma once
// Helpers for the 1-bit e-paper LVGL UI.
#include "lvgl.h"
#include "lvgl_private.h"

// Strip the default theme's animated styles from a widget tree.
// LVGL's default theme gives buttons, sliders and scrollbars an 80 ms state
// transition (plus a hard-coded 70 ms delay on release), a "grow" transform and
// a pressed recolor. On e-paper every animation step is a render and a potential
// panel refresh, and a transition delays the final pixels, so a tap would need
// two refreshes instead of one. Our own styles never set these properties, so any
// non-local style that carries one of them came from the theme.
inline void ui_detheme(lv_obj_t *obj) {
  for (int i = (int) obj->style_cnt - 1; i >= 0; i--) {
    lv_obj_style_t *s = &obj->styles[i];
    if (s->is_local || s->is_trans)
      continue;
    lv_style_value_t v;
    if (lv_style_get_prop(s->style, LV_STYLE_TRANSITION, &v) == LV_STYLE_RES_FOUND ||
        lv_style_get_prop(s->style, LV_STYLE_TRANSFORM_WIDTH, &v) == LV_STYLE_RES_FOUND ||
        lv_style_get_prop(s->style, LV_STYLE_RECOLOR_OPA, &v) == LV_STYLE_RES_FOUND) {
      lv_obj_remove_style(obj, s->style, s->selector);
    }
  }
  const uint32_t n = lv_obj_get_child_count(obj);
  for (uint32_t c = 0; c < n; c++)
    ui_detheme(lv_obj_get_child(obj, c));
}

// Every screen LVGL knows about, so a generated UI needs no list of page ids.
inline void ui_detheme_all() {
  lv_display_t *d = lv_display_get_default();
  if (d == nullptr) return;
  for (uint32_t i = 0; i < d->screen_cnt; i++)
    ui_detheme(d->screens[i]);
}
