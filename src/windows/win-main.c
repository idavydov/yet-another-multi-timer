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

src/windows/win-main.c

*/

#include <pebble.h>

#include <pebble-assist.h>
#include <bitmap-loader.h>

#include "win-about.h"
#include "win-controls.h"
#include "win-timer-add.h"
#include "win-timer.h"
#include "win-settings.h"
#include "win-vibration.h"
#include "win-duration.h"
#include "win-vibrate.h"
#include "win-deleted.h"

#include "../common.h"
#include "../icons.h"
#include "../timers.h"
#include "../settings.h"

#define MENU_SECTION_ADD_TIMER           0
#define MENU_SECTION_TIMERS              1
#define MENU_SECTION_ADD_STOPWATCH       2
#define MENU_SECTION_STOPWATCHES         3
#define MENU_SECTION_SETTINGS            4

#define MENU_ROW_SETTINGS            0

static void window_load(Window* window);
static void window_unload(Window* window);
static uint16_t menu_num_sections(struct MenuLayer* menu, void* callback_context);
static uint16_t menu_num_rows(struct MenuLayer* menu, uint16_t section_index, void* callback_context);
static int16_t menu_cell_height(struct MenuLayer *menu, MenuIndex *cell_index, void *callback_context);
static void menu_draw_row(GContext* ctx, const Layer* cell_layer, MenuIndex* cell_index, void* callback_context);
static void menu_draw_row_timers(GContext* ctx, const Layer* cell_layer, uint16_t section_index, uint16_t row_index);
static void menu_draw_row_add_timer(GContext* ctx, const Layer* cell_layer);
static void menu_draw_row_add_stopwatch(GContext* ctx, const Layer* cell_layer);
static void menu_draw_row_settings(GContext* ctx, const Layer* cell_layer);
static void menu_select(struct MenuLayer* menu, MenuIndex* cell_index, void* callback_context);
static void menu_select_timers(uint16_t section_index, uint16_t row_index);
static void menu_select_add_stopwatch(void);
static void menu_select_long(struct MenuLayer* menu, MenuIndex* cell_index, void* callback_context);
static void timers_update_handler(void);
static void timer_highlight_handler(Timer* timer);
static uint8_t timers_count_for_section(uint16_t section_index);
static int16_t timers_index_for_section_row(uint16_t section_index, uint16_t row_index);
static int16_t sorted_timer_index_for_row(uint16_t row_index);
static uint16_t timer_row_for_index(uint16_t timer_index, uint16_t section_index);
static bool timer_is_in_section(Timer* timer, uint16_t section_index);
static bool timer_is_stopwatch(Timer* timer);
static MenuIndex menu_index_for_timer(Timer* timer);

static Window*    s_window;
static MenuLayer* s_menu;
static StatusBarLayer* s_status_bar;

void win_main_init(void) {
  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload
  });
  timers_register_update_handler(timers_update_handler);
  timers_register_highlight_handler(timer_highlight_handler);
  win_timer_add_init();
  win_timer_init();
  win_settings_init();
  win_vibration_init();
  win_duration_init();
  win_vibrate_init();
  win_deleted_init();
}

void win_main_show(void) {
  window_stack_push(s_window, false);
}

static void window_load(Window* window) {
  Layer* root_layer = window_get_root_layer(s_window);
  GRect bounds = layer_get_bounds(root_layer);

  s_status_bar = status_bar_layer_create();
  status_bar_layer_set_colors(s_status_bar, GColorWhite, GColorBlack);
  status_bar_layer_set_separator_mode(s_status_bar, StatusBarLayerSeparatorModeDotted);
  layer_set_frame(status_bar_layer_get_layer(s_status_bar), GRect(0, 0, bounds.size.w, STATUS_HEIGHT));
  layer_add_child(root_layer, status_bar_layer_get_layer(s_status_bar));

  s_menu = menu_layer_create(GRect(0, STATUS_HEIGHT, bounds.size.w, bounds.size.h - STATUS_HEIGHT));
  menu_layer_set_callbacks(s_menu, NULL, (MenuLayerCallbacks) {
    .get_num_sections = menu_num_sections,
    .get_num_rows = menu_num_rows,
    .get_cell_height = menu_cell_height,
    .draw_row = menu_draw_row,
    .select_click = menu_select,
    .select_long_click = menu_select_long,
  });
  menu_layer_add_to_window(s_menu, s_window);
}

static void window_unload(Window* window) {
  menu_layer_destroy(s_menu);
  status_bar_layer_destroy(s_status_bar);
}

static uint16_t menu_num_sections(struct MenuLayer* menu, void* callback_context) {
  return 5;
}

static uint16_t menu_num_rows(struct MenuLayer* menu, uint16_t section_index, void* callback_context) {
  switch (section_index) {
    case MENU_SECTION_ADD_TIMER:
    case MENU_SECTION_ADD_STOPWATCH:
      return 1;
    case MENU_SECTION_TIMERS:
      return timers_count_for_section(MENU_SECTION_TIMERS);
    case MENU_SECTION_STOPWATCHES:
      return timers_count_for_section(MENU_SECTION_STOPWATCHES);
    case MENU_SECTION_SETTINGS:
      return 1;
    default:
      return 0;
  }
}

static int16_t menu_cell_height(struct MenuLayer *menu, MenuIndex *cell_index, void *callback_context) {
  switch (cell_index->section) {
    case MENU_SECTION_STOPWATCHES:
    case MENU_SECTION_TIMERS: {
      Timer* timer = timers_get(timers_index_for_section_row(cell_index->section, cell_index->row));
      if (! timer) {
        return 32;
      }
      switch (timer->type) {
        case TIMER_TYPE_TIMER:
          return 34;
        case TIMER_TYPE_STOPWATCH:
          return 32;
      }
      break;
    }
  }
  return 32;
}

static void menu_draw_row(GContext* ctx, const Layer* cell_layer, MenuIndex* cell_index, void* callback_context) {
  switch (cell_index->section) {
    case MENU_SECTION_ADD_TIMER:
      menu_draw_row_add_timer(ctx, cell_layer);
      break;
    case MENU_SECTION_ADD_STOPWATCH:
      menu_draw_row_add_stopwatch(ctx, cell_layer);
      break;
    case MENU_SECTION_STOPWATCHES:
    case MENU_SECTION_TIMERS:
      menu_draw_row_timers(ctx, cell_layer, cell_index->section, cell_index->row);
      break;
    case MENU_SECTION_SETTINGS:
      menu_draw_row_settings(ctx, cell_layer);
      break;
  }
}

static void menu_draw_row_timers(GContext* ctx, const Layer* cell_layer, uint16_t section_index, uint16_t row_index) {
  Timer* timer = timers_get(timers_index_for_section_row(section_index, row_index));
  if (! timer) { return; }
  timer_draw_row(timer, ctx, menu_cell_layer_is_highlighted(cell_layer));
}

static void menu_draw_row_add_timer(GContext* ctx, const Layer* cell_layer) {
  menu_draw_row_icon_text(ctx, "Timer", ICON_RECT_ADD, menu_cell_layer_is_highlighted(cell_layer));
}

static void menu_draw_row_add_stopwatch(GContext* ctx, const Layer* cell_layer) {
  menu_draw_row_icon_text(ctx, "Stopwatch", ICON_RECT_ADD, menu_cell_layer_is_highlighted(cell_layer));
}

static void menu_draw_row_settings(GContext* ctx, const Layer* cell_layer) {
  menu_draw_row_icon_text(ctx, "Settings", ICON_RECT_SETTINGS, menu_cell_layer_is_highlighted(cell_layer));
}

static void menu_select(struct MenuLayer* menu, MenuIndex* cell_index, void* callback_context) {
  switch (cell_index->section) {
    case MENU_SECTION_STOPWATCHES:
    case MENU_SECTION_TIMERS:
      menu_select_timers(cell_index->section, cell_index->row);
      break;
    case MENU_SECTION_ADD_TIMER:
      win_timer_add_show_new();
      break;
    case MENU_SECTION_ADD_STOPWATCH:
      menu_select_add_stopwatch();
      break;
    case MENU_SECTION_SETTINGS:
      win_settings_show();
      break;
  }
}

static void menu_select_timers(uint16_t section_index, uint16_t row_index) {
  Timer* timer = timers_get(timers_index_for_section_row(section_index, row_index));
  if (! timer) { return; }

  switch (timer->status) {
    case TIMER_STATUS_STOPPED: {
      timer_start(timer);
      break;
    }
    case TIMER_STATUS_RUNNING:
      timer_pause(timer);
      break;
    case TIMER_STATUS_PAUSED:
      timer_resume(timer);
      break;
    case TIMER_STATUS_DONE:
      timer_reset(timer);
      break;
  }
}

static void menu_select_add_stopwatch(void) {
  Timer* stopwatch = timer_create_stopwatch();
  if (settings()->timers_start_auto) {
    timer_start(stopwatch);
  }
  timers_add(stopwatch);
  timers_mark_updated();
  timers_highlight(stopwatch);
}

static void menu_select_long(struct MenuLayer* menu, MenuIndex* cell_index, void* callback_context) {
  if (cell_index->section == MENU_SECTION_STOPWATCHES || cell_index->section == MENU_SECTION_TIMERS) {
    Timer* timer = timers_get(timers_index_for_section_row(cell_index->section, cell_index->row));
    win_timer_set_timer(timer);
    win_timer_show();
  }
}

static void timers_update_handler(void) {
  menu_layer_reload_data(s_menu);
}

static void timer_highlight_handler(Timer* timer) {
  menu_layer_set_selected_index(s_menu, menu_index_for_timer(timer), MenuRowAlignCenter, true);
}

static uint8_t timers_count_for_section(uint16_t section_index) {
  uint8_t count = 0;
  for (uint8_t i = 0; i < timers_count(); i += 1) {
    if (timer_is_in_section(timers_get(i), section_index)) {
      count += 1;
    }
  }
  return count;
}

static int16_t timers_index_for_section_row(uint16_t section_index, uint16_t row_index) {
  if (section_index == MENU_SECTION_TIMERS && settings()->sort_timers_by_duration) {
    return sorted_timer_index_for_row(row_index);
  }

  uint8_t row = 0;
  for (uint8_t i = 0; i < timers_count(); i += 1) {
    if (timer_is_in_section(timers_get(i), section_index)) {
      if (row == row_index) {
        return i;
      }
      row += 1;
    }
  }
  return -1;
}

static int16_t sorted_timer_index_for_row(uint16_t row_index) {
  for (uint8_t candidate_index = 0; candidate_index < timers_count(); candidate_index += 1) {
    Timer* candidate = timers_get(candidate_index);
    if (! timer_is_in_section(candidate, MENU_SECTION_TIMERS)) {
      continue;
    }

    uint16_t rank = 0;
    for (uint8_t compare_index = 0; compare_index < timers_count(); compare_index += 1) {
      Timer* compare = timers_get(compare_index);
      if (! timer_is_in_section(compare, MENU_SECTION_TIMERS)) {
        continue;
      }
      if (compare->length < candidate->length ||
          (compare->length == candidate->length && compare_index < candidate_index)) {
        rank += 1;
      }
    }
    if (rank == row_index) {
      return candidate_index;
    }
  }
  return -1;
}

static uint16_t timer_row_for_index(uint16_t timer_index, uint16_t section_index) {
  if (section_index == MENU_SECTION_TIMERS && settings()->sort_timers_by_duration) {
    Timer* timer = timers_get(timer_index);
    if (! timer) {
      return 0;
    }

    uint16_t row = 0;
    for (uint8_t i = 0; i < timers_count(); i += 1) {
      Timer* compare = timers_get(i);
      if (! timer_is_in_section(compare, MENU_SECTION_TIMERS)) {
        continue;
      }
      if (compare->length < timer->length ||
          (compare->length == timer->length && i < timer_index)) {
        row += 1;
      }
    }
    return row;
  }

  uint16_t row = 0;
  for (uint8_t i = 0; i < timer_index; i += 1) {
    if (timer_is_in_section(timers_get(i), section_index)) {
      row += 1;
    }
  }
  return row;
}

static bool timer_is_in_section(Timer* timer, uint16_t section_index) {
  if (! timer) {
    return false;
  }
  switch (section_index) {
    case MENU_SECTION_STOPWATCHES:
      return timer_is_stopwatch(timer);
    case MENU_SECTION_TIMERS:
      return ! timer_is_stopwatch(timer);
  }
  return false;
}

static bool timer_is_stopwatch(Timer* timer) {
  return timer->type == TIMER_TYPE_STOPWATCH;
}

static MenuIndex menu_index_for_timer(Timer* timer) {
  MenuIndex index = (MenuIndex) { .section = MENU_SECTION_TIMERS, .row = 0 };
  int16_t timer_index = timers_index_of(timer->id);
  if (timer_index < 0) {
    return index;
  }
  index.section = timer_is_stopwatch(timer) ? MENU_SECTION_STOPWATCHES : MENU_SECTION_TIMERS;
  index.row = timer_row_for_index(timer_index, index.section);
  return index;
}
