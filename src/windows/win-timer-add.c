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

src/windows/win-timer-add.c

*/

#include <pebble.h>

#include "win-duration.h"
#include "../timers.h"
#include "../timer.h"
#include "../settings.h"

static void new_timer_duration_callback(uint32_t duration);
static void edit_timer_duration_callback(uint32_t duration);

static Timer* s_new_timer = NULL;
static Timer* s_edit_timer = NULL;

void win_timer_add_show_new(void) {
  if (s_new_timer) {
    free(s_new_timer);
  }
  s_new_timer = timer_create_timer();
  s_edit_timer = NULL;
  win_duration_show(s_new_timer->length, new_timer_duration_callback);
}

void win_timer_add_show_edit(Timer* timer) {
  s_edit_timer = timer;
  win_duration_show(timer->length, edit_timer_duration_callback);
}

static void new_timer_duration_callback(uint32_t duration) {
  if (! s_new_timer) {
    return;
  }

  if (duration == 0) {
    vibes_short_pulse();
    free(s_new_timer);
    s_new_timer = NULL;
    return;
  }

  Timer* timer = s_new_timer;
  s_new_timer = NULL;
  timer->length = duration;
  timer_reset(timer);
  if (settings()->timers_start_auto) {
    timer_start(timer);
  }
  timers_add(timer);
  timers_mark_updated();
  timers_highlight(timer);
}

static void edit_timer_duration_callback(uint32_t duration) {
  if (! s_edit_timer) {
    return;
  }

  if (duration == 0) {
    vibes_short_pulse();
    return;
  }

  s_edit_timer->length = duration;
  timer_reset(s_edit_timer);
  timers_mark_updated();
  timers_highlight(s_edit_timer);
  s_edit_timer = NULL;
}
