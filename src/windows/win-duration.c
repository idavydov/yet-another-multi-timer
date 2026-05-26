/*

Multi Timer v3.4
http://matthewtole.com/pebble/multi-timer/

----------------------

The MIT License (MIT)
Copyright © 2013 - 2015 Matthew Tole
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

--------------------

src/windows/win-duration.c

*/

#include <pebble.h>

#include <selection_layer.h>

#include "win-duration.h"

#define NUM_FIELDS 3
#define FIELD_HOURS 0
#define FIELD_MINUTES 1
#define FIELD_SECONDS 2

#define SECONDS_IN_MINUTE 60
#define SECONDS_IN_HOUR 3600
#define MINUTES_IN_HOUR 60
#define HOURS_MAX 100
#define REPEATING_CLICK_THRESHOLD 10
#define TIMER_MINIMUM_DURATION 5
#define TIMELINE_MINIMUM_DURATION 900

static void window_load(Window* window);
static void window_unload(Window* window);
static char* selection_get_text(unsigned index, void *context);
static void selection_complete(void *context);
static void selection_increment(unsigned index, uint8_t clicks, void *context);
static void selection_decrement(unsigned index, uint8_t clicks, void *context);
static void update_timer_length(void);
static void update_sub_text(void);

static Window* s_window;
static TextLayer* s_title_layer;
static TextLayer* s_subtitle_layer;
static Layer* s_selection_layer;
static StatusBarLayer* s_status_bar;
static uint32_t s_duration = 0;
static DurationCallback s_callback;

static int32_t s_field_values[NUM_FIELDS];
static char s_field_buffers[NUM_FIELDS][3];

void win_duration_init(void) {
  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
}

void win_duration_show(uint16_t duration, DurationCallback callback) {
  s_duration = duration;
  s_callback = callback;
  s_field_values[FIELD_HOURS] = s_duration / SECONDS_IN_HOUR;
  s_field_values[FIELD_MINUTES] = (s_duration % SECONDS_IN_HOUR) / SECONDS_IN_MINUTE;
  s_field_values[FIELD_SECONDS] = s_duration % SECONDS_IN_MINUTE;
  window_stack_push(s_window, true);
  if (s_selection_layer) {
    selection_layer_set_selected_cell(s_selection_layer, FIELD_MINUTES);
  }
  update_sub_text();
}

static void window_load(Window* window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  s_title_layer = text_layer_create(GRect(0, bounds.size.h / 7, bounds.size.w, 40));
  text_layer_set_background_color(s_title_layer, GColorClear);
  text_layer_set_text_color(s_title_layer, GColorBlack);
  text_layer_set_text(s_title_layer, "Set Timer");
#if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_GABBRO)
  text_layer_set_font(s_title_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
#else
  text_layer_set_font(s_title_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
#endif
  text_layer_set_text_alignment(s_title_layer, GTextAlignmentCenter);
  layer_add_child(root, text_layer_get_layer(s_title_layer));

  s_subtitle_layer = text_layer_create(GRect(1, bounds.size.h - 43 * bounds.size.h / 168, bounds.size.w, 40));
  text_layer_set_background_color(s_subtitle_layer, GColorClear);
  text_layer_set_text_color(s_subtitle_layer, GColorBlack);
  text_layer_set_text_alignment(s_subtitle_layer, GTextAlignmentCenter);
#if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_GABBRO)
  text_layer_set_font(s_subtitle_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
#else
  text_layer_set_font(s_subtitle_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
#endif
  layer_add_child(root, text_layer_get_layer(s_subtitle_layer));

  uint16_t cell_height = (bounds.size.h + 4) / 5;
  uint16_t cell_y_start = bounds.size.h / 2 - cell_height / 2;
#ifdef PBL_ROUND
  s_selection_layer = selection_layer_create(GRect(26, cell_y_start, bounds.size.w - 52, cell_height), NUM_FIELDS);
  uint8_t cell_width = (bounds.size.w - 52 - (NUM_FIELDS - 1) * 4) / NUM_FIELDS;
#else
  s_selection_layer = selection_layer_create(GRect(8, cell_y_start, bounds.size.w - 16, cell_height), NUM_FIELDS);
  uint8_t cell_width = (bounds.size.w - 16 - (NUM_FIELDS - 1) * 4) / NUM_FIELDS;
#endif
  for (int i = 0; i < NUM_FIELDS; i++) {
    selection_layer_set_cell_width(s_selection_layer, i, cell_width);
  }
  selection_layer_set_cell_padding(s_selection_layer, 4);
  selection_layer_set_active_bg_color(s_selection_layer, GColorBlack);
  selection_layer_set_inactive_bg_color(s_selection_layer, GColorLightGray);
  selection_layer_set_selected_cell(s_selection_layer, FIELD_MINUTES);
  selection_layer_set_click_config_onto_window(s_selection_layer, window);
  selection_layer_set_callbacks(s_selection_layer, NULL, (SelectionLayerCallbacks) {
    .get_cell_text = selection_get_text,
    .complete = selection_complete,
    .increment = selection_increment,
    .decrement = selection_decrement,
  });
  layer_add_child(root, s_selection_layer);

  s_status_bar = status_bar_layer_create();
  status_bar_layer_set_colors(s_status_bar, GColorClear, GColorBlack);
  layer_add_child(root, status_bar_layer_get_layer(s_status_bar));

  update_sub_text();
}

static void window_unload(Window* window) {
  status_bar_layer_destroy(s_status_bar);
  selection_layer_destroy(s_selection_layer);
  text_layer_destroy(s_subtitle_layer);
  text_layer_destroy(s_title_layer);

  s_status_bar = NULL;
  s_selection_layer = NULL;
  s_subtitle_layer = NULL;
  s_title_layer = NULL;
}

static char* selection_get_text(unsigned index, void *context) {
  snprintf(s_field_buffers[index], sizeof(s_field_buffers[0]), "%02d", (int)s_field_values[index]);
  return s_field_buffers[index];
}

static void selection_complete(void *context) {
  update_timer_length();
  if (s_callback) {
    s_callback(s_duration);
  }
  if (window_stack_get_top_window() == s_window) {
    window_stack_pop(true);
  }
}

static void selection_increment(unsigned index, uint8_t clicks, void *context) {
  s_field_values[index] += (clicks > REPEATING_CLICK_THRESHOLD) ? 2 : 1;
  int32_t max_value = (index == FIELD_HOURS) ? HOURS_MAX : MINUTES_IN_HOUR;
  if (s_field_values[index] >= max_value) {
    s_field_values[index] -= max_value;
  }
  update_timer_length();
  update_sub_text();
}

static void selection_decrement(unsigned index, uint8_t clicks, void *context) {
  s_field_values[index] -= (clicks > REPEATING_CLICK_THRESHOLD) ? 2 : 1;
  int32_t max_value = (index == FIELD_HOURS) ? HOURS_MAX : MINUTES_IN_HOUR;
  if (s_field_values[index] < 0) {
    s_field_values[index] += max_value;
  }
  update_timer_length();
  update_sub_text();
}

static void update_timer_length(void) {
  s_duration = s_field_values[FIELD_HOURS] * SECONDS_IN_HOUR +
    s_field_values[FIELD_MINUTES] * SECONDS_IN_MINUTE +
    s_field_values[FIELD_SECONDS];
}

static void update_sub_text(void) {
  if (!s_subtitle_layer) {
    return;
  }

  if (s_duration < TIMER_MINIMUM_DURATION) {
    text_layer_set_text(s_subtitle_layer, "");
    layer_set_hidden(text_layer_get_layer(s_subtitle_layer), false);
    return;
  }

  if (s_duration < TIMELINE_MINIMUM_DURATION) {
    layer_set_hidden(text_layer_get_layer(s_subtitle_layer), true);
    return;
  }

  layer_set_hidden(text_layer_get_layer(s_subtitle_layer), false);

  time_t end = time(NULL) + s_duration;
  static char buffer[] = "End: 00:00 AM";
  struct tm *tick_time = localtime(&end);
  if (clock_is_24h_style()) {
    strftime(buffer, sizeof(buffer), "End: %k:%M", tick_time);
  }
  else {
    strftime(buffer, sizeof(buffer), "End: %l:%M %p", tick_time);
  }
  text_layer_set_text(s_subtitle_layer, buffer);
}
