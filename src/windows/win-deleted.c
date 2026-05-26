#include <pebble.h>

#include "win-deleted.h"

#define POPUP_REFRESH_MS 35
#define POPUP_FALLBACK_DURATION_MS 1000

static void window_load(Window* window);
static void window_unload(Window* window);
static void window_disappear(Window* window);
static void layer_update(Layer* layer, GContext* ctx);
static void timer_callback(void* context);
static int64_t current_time_ms(void);
static void schedule_refresh(void);

static Window* s_window;
static Layer* s_icon_layer;
static TextLayer* s_text_layer;
static AppTimer* s_timer;
static int64_t s_started_ms;
static int64_t s_duration_ms;

#ifndef PBL_PLATFORM_APLITE
static GDrawCommandSequence* s_sequence;
static GDrawCommandFrame* s_frame;
#else
static GBitmap* s_image;
#endif

void win_deleted_init(void) {
  s_window = window_create();
  window_set_background_color(s_window, GColorWhite);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
    .disappear = window_disappear,
  });
}

void win_deleted_show(void) {
  s_started_ms = current_time_ms();
  if (! window_stack_contains_window(s_window)) {
    window_stack_push(s_window, true);
  }
  schedule_refresh();
}

static void window_load(Window* window) {
  Layer* root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  s_text_layer = text_layer_create(GRect(0, 14, bounds.size.w, 28));
  text_layer_set_background_color(s_text_layer, GColorClear);
  text_layer_set_text_color(s_text_layer, GColorBlack);
  text_layer_set_text(s_text_layer, "Timer Deleted");
  text_layer_set_font(s_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(s_text_layer, GTextAlignmentCenter);
  layer_add_child(root, text_layer_get_layer(s_text_layer));

#ifndef PBL_PLATFORM_APLITE
  s_sequence = gdraw_command_sequence_create_with_resource(RESOURCE_ID_ICON_DELETED);
  s_duration_ms = gdraw_command_sequence_get_total_duration(s_sequence);
  if (s_duration_ms <= 0) {
    s_duration_ms = POPUP_FALLBACK_DURATION_MS;
  }
  GSize pdc_size = gdraw_command_sequence_get_bounds_size(s_sequence);
  s_icon_layer = layer_create(GRect((bounds.size.w - pdc_size.w) / 2,
    (bounds.size.h - pdc_size.h) / 2 + 8, pdc_size.w, pdc_size.h));
#else
  s_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_SHREADER);
  s_duration_ms = POPUP_FALLBACK_DURATION_MS;
  GRect image_bounds = gbitmap_get_bounds(s_image);
  s_icon_layer = layer_create(GRect((bounds.size.w - image_bounds.size.w) / 2,
    (bounds.size.h - image_bounds.size.h) / 2 + 8, image_bounds.size.w, image_bounds.size.h));
#endif
  layer_set_update_proc(s_icon_layer, layer_update);
  layer_add_child(root, s_icon_layer);
}

static void window_unload(Window* window) {
  if (s_timer) {
    app_timer_cancel(s_timer);
    s_timer = NULL;
  }
#ifndef PBL_PLATFORM_APLITE
  if (s_sequence) {
    gdraw_command_sequence_destroy(s_sequence);
    s_sequence = NULL;
  }
  s_frame = NULL;
#else
  if (s_image) {
    gbitmap_destroy(s_image);
    s_image = NULL;
  }
#endif
  layer_destroy(s_icon_layer);
  text_layer_destroy(s_text_layer);
  s_icon_layer = NULL;
  s_text_layer = NULL;
}

static void window_disappear(Window* window) {
  if (s_timer) {
    app_timer_cancel(s_timer);
    s_timer = NULL;
  }
}

static void layer_update(Layer* layer, GContext* ctx) {
#ifndef PBL_PLATFORM_APLITE
  if (s_sequence && s_frame) {
    gdraw_command_frame_draw(ctx, s_sequence, s_frame, GPointZero);
  }
#else
  if (s_image) {
    graphics_draw_bitmap_in_rect(ctx, s_image, layer_get_bounds(layer));
  }
#endif
}

static void timer_callback(void* context) {
  s_timer = NULL;
  int64_t elapsed_ms = current_time_ms() - s_started_ms;
  if (elapsed_ms >= s_duration_ms) {
    window_stack_remove(s_window, true);
    return;
  }

#ifndef PBL_PLATFORM_APLITE
  s_frame = gdraw_command_sequence_get_frame_by_elapsed(s_sequence, elapsed_ms);
#endif
  layer_mark_dirty(s_icon_layer);
  schedule_refresh();
}

static int64_t current_time_ms(void) {
  uint16_t ms = 0;
  time_t sec = 0;
  time_ms(&sec, &ms);
  return (int64_t)sec * 1000 + ms;
}

static void schedule_refresh(void) {
  if (! s_timer) {
    s_timer = app_timer_register(POPUP_REFRESH_MS, timer_callback, NULL);
  }
}
