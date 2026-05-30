#include <pebble.h>
#include <ctype.h>

// ── Keys ──────────────────────────────────────────────────
#define KEY_FONT_CHOICE  0
#define KEY_BG_CHOICE    1
#define KEY_DATE_STYLE   2  // 0=off  1=standard(right of time)  2=fill

// ── State ─────────────────────────────────────────────────
static Window      *s_window;
static BitmapLayer *s_bg_layer;
static GBitmap     *s_bg_bitmap;
static TextLayer   *s_time_layer;
static TextLayer   *s_seconds_layer;
static TextLayer   *s_date_a_layer;   // option A: two lines right of time
static Layer       *s_date_b_layer;   // option B: tiled canvas
static GFont        s_time_font;
static GFont        s_date_font;      // shared small font for date

static int s_font_choice = 0;
static int s_bg_choice   = 0;
static int s_date_style  = 0;

// ── Resources ─────────────────────────────────────────────
static const uint32_t FONT_RESOURCE_IDS[4] = {
  RESOURCE_ID_FONT_SG_REGULAR_24,
  RESOURCE_ID_FONT_SG_REGULAR_28,
  RESOURCE_ID_FONT_SG_MEDIUM_24,
  RESOURCE_ID_FONT_SG_MEDIUM_28,
};
static const uint32_t BG_RESOURCE_IDS[3] = {
  RESOURCE_ID_BG_CREAM,
  RESOURCE_ID_BG_BLACK,
  RESOURCE_ID_BG_WHITE,
};
static const int TIME_LAYER_H[4] = { 34, 40, 34, 40 };

// ── Colors ─────────────────────────────────────────────────
static GColor get_text_color() {
  return (s_bg_choice == 1) ? GColorWhite : GColorBlack;
}
static GColor get_sec_color() {
  return (s_bg_choice == 1) ? GColorLightGray : GColorDarkGray;
}
static GColor get_date_muted() {
  return (s_bg_choice == 1) ? GColorDarkGray : GColorLightGray;
}

// ── Seeded PRNG ────────────────────────────────────────────
static uint32_t s_prng_state = 0;
static void prng_seed(uint32_t seed) { s_prng_state = seed; }
static uint32_t prng_next() {
  s_prng_state = s_prng_state * 1664525 + 1013904223;
  return s_prng_state;
}
static int prng_range(int n) { return (int)(prng_next() % (uint32_t)n); }

// ── Date strings ───────────────────────────────────────────
static char s_day_str[8];    // e.g. "24"
static char s_month_str[8];  // e.g. "May"
static char s_wday_str[10];  // e.g. "Monday"
static char s_date_a_buf[24]; // "24 May\nMonday"

static void build_date_strings() {
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  strftime(s_day_str,   sizeof(s_day_str),   "%d",  t);
  strftime(s_month_str, sizeof(s_month_str), "%b",  t);
  strftime(s_wday_str,  sizeof(s_wday_str),  "%A",  t);
  snprintf(s_date_a_buf, sizeof(s_date_a_buf), "%s %s\n%s",
           s_day_str, s_month_str, s_wday_str);
}

// ── Option B: tiled canvas ─────────────────────────────────
// Protected zones (no highlights):
//   top-right:   x > 104, y < 28  (seconds)
//   bottom-left: x < 110, y > 128 (time)
static bool in_protected_zone(int x, int y) {
  if (x > 104 && y < 28)  return true;
  if (x < 110 && y > 128) return true;
  return false;
}

// Word variants: 0=UPPER 1=lower 2=Natural
static void apply_variant(char *out, int size, const char *src, int v) {
  snprintf(out, size, "%s", src);
  if (v == 0) { for (int i=0; out[i]; i++) out[i] = toupper((unsigned char)out[i]); }
  if (v == 1) { for (int i=0; out[i]; i++) out[i] = tolower((unsigned char)out[i]); }
}

// Tile layout stored so draw proc can render it
#define MAX_TILES 120
typedef struct { int16_t x, y; uint8_t sz; uint8_t type;  // 0=day 1=wday
                 uint8_t variant; bool highlight; } Tile;
static Tile s_tiles[MAX_TILES];
static int  s_tile_count = 0;

static void build_tiles(uint32_t seed) {
  prng_seed(seed);
  s_tile_count = 0;

  // char widths (approx pixels per char at each size)
  // sizes we cycle through: 8, 9, 10, 11, 12, 13, 14
  int sizes[] = { 8, 10, 9, 12, 11, 13, 10, 8, 14, 9 };
  int n_sizes = 10;

  int y = -4;
  while (y < 172 && s_tile_count < MAX_TILES - 1) {
    int sz = sizes[prng_range(n_sizes)];
    int x  = -4;
    while (x < 148 && s_tile_count < MAX_TILES - 1) {
      int   type    = prng_range(2);              // 0=day 1=wday
      int   variant = prng_range(3);              // 0=upper 1=lower 2=natural
      // approx width: day~6chars, wday~6chars, at sz px ≈ sz*0.58 per char
      int   wlen    = (type == 0) ? 6 : 6;
      int   approx_w = (wlen * sz * 58) / 100;

      s_tiles[s_tile_count++] = (Tile){
        .x = (int16_t)x, .y = (int16_t)y,
        .sz = (uint8_t)sz, .type = (uint8_t)type,
        .variant = (uint8_t)variant, .highlight = false
      };
      x += approx_w + 3 + prng_range(8);
    }
    y += sz + 2 + prng_range(4);
  }

  // Pick highlights — skip protected zones
  int hi_count = 3 + prng_range(3);
  // shuffle indices
  int idx[MAX_TILES];
  for (int i = 0; i < s_tile_count; i++) idx[i] = i;
  for (int i = s_tile_count - 1; i > 0; i--) {
    int j = prng_range(i + 1);
    int tmp = idx[i]; idx[i] = idx[j]; idx[j] = tmp;
  }
  int day_hi = 0, wk_hi = 0;
  for (int i = 0; i < s_tile_count && (day_hi < hi_count || wk_hi < hi_count); i++) {
    Tile *t = &s_tiles[idx[i]];
    if (in_protected_zone(t->x, t->y)) continue;
    if (t->type == 0 && day_hi < hi_count) { t->highlight = true; day_hi++; }
    if (t->type == 1 && wk_hi  < hi_count) { t->highlight = true; wk_hi++;  }
  }
}

static void date_b_layer_draw(Layer *layer, GContext *ctx) {
  char word[12];
  for (int i = 0; i < s_tile_count; i++) {
    Tile *t = &s_tiles[i];
    const char *src = (t->type == 0) ? s_day_str : s_wday_str;
    // compose "24 May" for day tiles
    char combined[12];
    if (t->type == 0) {
      snprintf(combined, sizeof(combined), "%s %s", s_day_str, s_month_str);
      src = combined;
    }
    apply_variant(word, sizeof(word), src, t->variant);

    GColor color;
    if (t->highlight) {
      color = get_text_color();
    } else {
      color = get_date_muted();
    }

    GFont font;
    if      (t->sz <= 9)  font = fonts_get_system_font(FONT_KEY_GOTHIC_09);
    else if (t->sz <= 11) font = fonts_get_system_font(FONT_KEY_GOTHIC_14);
    else                  font = fonts_get_system_font(FONT_KEY_GOTHIC_18);

    graphics_context_set_text_color(ctx, color);
    graphics_draw_text(ctx, word,
      font,
      GRect(t->x, t->y, 100, t->sz + 4),
      GTextOverflowModeWordWrap,
      GTextAlignmentLeft,
      NULL);
  }
}

// ── Time & date update ─────────────────────────────────────
static void update_time() {
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  static char time_buf[6];
  static char sec_buf[3];
  strftime(time_buf, sizeof(time_buf), "%H:%M", t);
  strftime(sec_buf,  sizeof(sec_buf),  "%S",    t);
  text_layer_set_text(s_time_layer,    time_buf);
  text_layer_set_text(s_seconds_layer, sec_buf);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_time();
  // Rebuild tile layout on hour change
  if ((units_changed & HOUR_UNIT) && s_date_style == 2) {
    build_date_strings();
    build_tiles((uint32_t)(tick_time->tm_hour * 1000 + tick_time->tm_mday * 100));
    layer_mark_dirty(s_date_b_layer);
  }
  // Update option A date text each minute
  if ((units_changed & MINUTE_UNIT) && s_date_style == 1) {
    build_date_strings();
    text_layer_set_text(s_date_a_layer, s_date_a_buf);
  }
}

// ── Apply settings ─────────────────────────────────────────
static void apply_settings() {
  Layer *root   = window_get_root_layer(s_window);
  GRect  bounds = layer_get_bounds(root);

  // Background
  if (s_bg_bitmap) gbitmap_destroy(s_bg_bitmap);
  s_bg_bitmap = gbitmap_create_with_resource(BG_RESOURCE_IDS[s_bg_choice]);
  bitmap_layer_set_bitmap(s_bg_layer, s_bg_bitmap);

  // Time font
  if (s_time_font) fonts_unload_custom_font(s_time_font);
  s_time_font = fonts_load_custom_font(resource_get_handle(FONT_RESOURCE_IDS[s_font_choice]));
  text_layer_set_font(s_time_layer, s_time_font);

  // Colors
  text_layer_set_text_color(s_time_layer,    get_text_color());
  text_layer_set_text_color(s_seconds_layer, get_sec_color());
  text_layer_set_text_color(s_date_a_layer,  get_date_muted());

  // Time layer position
  int lh = TIME_LAYER_H[s_font_choice];
  layer_set_frame(text_layer_get_layer(s_time_layer),
                  GRect(4, bounds.size.h - lh - 4, 80, lh));

  // Option A: date right of time, two lines
  layer_set_hidden(text_layer_get_layer(s_date_a_layer), s_date_style != 1);
  if (s_date_style == 1) {
    layer_set_frame(text_layer_get_layer(s_date_a_layer),
                    GRect(88, bounds.size.h - lh - 2, 52, lh + 4));
    build_date_strings();
    text_layer_set_text(s_date_a_layer, s_date_a_buf);
  }

  // Option B: tiled canvas
  layer_set_hidden(s_date_b_layer, s_date_style != 2);
  if (s_date_style == 2) {
    build_date_strings();
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    build_tiles((uint32_t)(t->tm_hour * 1000 + t->tm_mday * 100));
    layer_mark_dirty(s_date_b_layer);
  }
}

// ── AppMessage ─────────────────────────────────────────────
static void inbox_received(DictionaryIterator *iter, void *ctx) {
  Tuple *t;
  t = dict_find(iter, KEY_FONT_CHOICE);
  if (t) { s_font_choice = (int)t->value->int32; persist_write_int(KEY_FONT_CHOICE, s_font_choice); }
  t = dict_find(iter, KEY_BG_CHOICE);
  if (t) { s_bg_choice   = (int)t->value->int32; persist_write_int(KEY_BG_CHOICE,   s_bg_choice);   }
  t = dict_find(iter, KEY_DATE_STYLE);
  if (t) { s_date_style  = (int)t->value->int32; persist_write_int(KEY_DATE_STYLE,  s_date_style);  }
  // rebuild tiles on settings change too
  if (s_date_style == 2) {
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    build_tiles((uint32_t)(tm->tm_hour * 1000 + tm->tm_mday * 100 + prng_next() % 97));
  }
  apply_settings();
}

// ── Window lifecycle ───────────────────────────────────────
static void window_load(Window *window) {
  Layer *root   = window_get_root_layer(window);
  GRect  bounds = layer_get_bounds(root);

  s_font_choice = persist_exists(KEY_FONT_CHOICE) ? persist_read_int(KEY_FONT_CHOICE) : 0;
  s_bg_choice   = persist_exists(KEY_BG_CHOICE)   ? persist_read_int(KEY_BG_CHOICE)   : 0;
  s_date_style  = persist_exists(KEY_DATE_STYLE)  ? persist_read_int(KEY_DATE_STYLE)  : 0;

  // Background
  s_bg_bitmap = NULL;
  s_bg_layer  = bitmap_layer_create(bounds);
  bitmap_layer_set_compositing_mode(s_bg_layer, GCompOpAssign);
  layer_add_child(root, bitmap_layer_get_layer(s_bg_layer));

  // Option B tiled layer — full screen, below time/secs
  s_date_b_layer = layer_create(bounds);
  layer_set_update_proc(s_date_b_layer, date_b_layer_draw);
  layer_add_child(root, s_date_b_layer);

  // Seconds — top right
  s_seconds_layer = text_layer_create(GRect(bounds.size.w - 36, 5, 32, 18));
  text_layer_set_background_color(s_seconds_layer, GColorClear);
  text_layer_set_font(s_seconds_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(s_seconds_layer, GTextAlignmentRight);
  layer_add_child(root, text_layer_get_layer(s_seconds_layer));

  // Time — bottom left
  s_time_font  = NULL;
  s_time_layer = text_layer_create(GRect(4, bounds.size.h - 40, 80, 40));
  text_layer_set_background_color(s_time_layer, GColorClear);
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentLeft);
  layer_add_child(root, text_layer_get_layer(s_time_layer));

  // Option A date — right of time
  s_date_font  = NULL;
  s_date_a_layer = text_layer_create(GRect(88, bounds.size.h - 40, 52, 40));
  text_layer_set_background_color(s_date_a_layer, GColorClear);
  text_layer_set_font(s_date_a_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(s_date_a_layer, GTextAlignmentLeft);
  layer_add_child(root, text_layer_get_layer(s_date_a_layer));

  apply_settings();
  update_time();
}

static void window_unload(Window *window) {
  bitmap_layer_destroy(s_bg_layer);
  if (s_bg_bitmap)  gbitmap_destroy(s_bg_bitmap);
  if (s_time_font)  fonts_unload_custom_font(s_time_font);
  layer_destroy(s_date_b_layer);
  text_layer_destroy(s_time_layer);
  text_layer_destroy(s_seconds_layer);
  text_layer_destroy(s_date_a_layer);
}

// ── Init / deinit ──────────────────────────────────────────
static void init() {
  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers){
    .load   = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);
  app_message_open(64, 64);
  app_message_register_inbox_received(inbox_received);
  tick_timer_service_subscribe(SECOND_UNIT, tick_handler);
}

static void deinit() {
  tick_timer_service_unsubscribe();
  window_destroy(s_window);
}

int main() {
  init();
  app_event_loop();
  deinit();
}
