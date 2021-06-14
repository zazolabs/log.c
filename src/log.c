/*
 * Copyright (c) 2020 rxi
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "log.h"
#include <sys/time.h>
#define MAX_CALLBACKS 32

typedef struct {
  log_LogFn fn;
  void *udata;
  int level;
} Callback;

static struct {
  void *udata;
  log_LockFn lock;
  int level;
  bool quiet;
  Callback callbacks[MAX_CALLBACKS];
} L;


static const char *level_strings[] = {
  "[[DEBUG]]", "[[TRACE]]", "[[ INFO]]", "[[ WARN]]", "[[ERROR]]", "[[FATAL]]"
};


static inline const char *get_level_string(int level)
{
    return level_strings[(level + 32) / 32];
}

#ifdef LOG_USE_COLOR
static const char *level_colors[] = {
  "\x1b[36m", "\x1b[94m", "\x1b[32m", "\x1b[33m", "\x1b[31m", "\x1b[35m"
};

static inline const char *get_level_color(int level)
{
    return level_colors[(level + 32) / 32];
}
#endif


static void stdout_callback(log_Event *ev)
{
    struct timeval curTime;
    int milli;
    char buffer [80];
    gettimeofday(&curTime, NULL);

    milli = curTime.tv_usec / 1000;
    strftime(buffer, 80, "%Y-%m-%d %H:%M:%S", localtime(&curTime.tv_sec));
#ifdef LOG_USE_COLOR
  fprintf(
    ev->udata, "%s:%03d %s%-7s\x1b[0m \x1b[90m%s:%d:\x1b[0m ",
    buffer, milli, get_level_color(ev->level), get_level_string(ev->level),
    ev->file, ev->line);
#else
  fprintf(
    ev->udata, "%s:%03d %-7s %s:%d: ",
    buffer, milli, get_level_string(ev->level), ev->file, ev->line);
#endif
  vfprintf(ev->udata, ev->fmt, ev->ap);
  fflush(ev->udata);
}


static void file_callback(log_Event *ev) {
    struct timeval curTime;
    int milli;
    char buffer [80];
    gettimeofday(&curTime, NULL);

    milli = curTime.tv_usec / 1000;
    strftime(buffer, 80, "%Y-%m-%d %H:%M:%S", localtime(&curTime.tv_sec));
  fprintf(
    ev->udata, "%s:%03d %-7s %s:%d: ",
    buffer, milli, get_level_string(ev->level), ev->file, ev->line);
  vfprintf(ev->udata, ev->fmt, ev->ap);
  fflush(ev->udata);
}


static void lock(void)   {
  if (L.lock) { L.lock(true, L.udata); }
}


static void unlock(void) {
  if (L.lock) { L.lock(false, L.udata); }
}


void log_set_lock(log_LockFn fn, void *udata) {
  L.lock = fn;
  L.udata = udata;
}


void log_set_level(int level) {
  L.level = level;
}


void log_set_quiet(bool enable) {
  L.quiet = enable;
}


int log_add_callback(log_LogFn fn, void *udata, int level) {
  for (int i = 0; i < MAX_CALLBACKS; i++) {
    if (!L.callbacks[i].fn) {
      L.callbacks[i] = (Callback) { fn, udata, level };
      return 0;
    }
  }
  return -1;
}


int log_add_fp(FILE *fp, int level) {
  return log_add_callback(file_callback, fp, level);
}


static void init_event(log_Event *ev, void *udata) {
  if (!ev->time) {
    time_t t = time(NULL);
    ev->time = localtime(&t);
  }
  ev->udata = udata;
}



void log_log(int level, const char *file, int line, const char *fmt, ...) {
  log_Event ev = {
    .fmt   = fmt,
    .file  = file,
    .line  = line,
    .level = level,
  };

  lock();

  if (!L.quiet && level >= L.level) {
    init_event(&ev, stderr);
    va_start(ev.ap, fmt);
    stdout_callback(&ev);
    va_end(ev.ap);
  }

  for (int i = 0; i < MAX_CALLBACKS && L.callbacks[i].fn; i++) {
    Callback *cb = &L.callbacks[i];
    if (level >= cb->level) {
      init_event(&ev, cb->udata);
      va_start(ev.ap, fmt);
      cb->fn(&ev);
      va_end(ev.ap);
    }
  }

  unlock();
}
