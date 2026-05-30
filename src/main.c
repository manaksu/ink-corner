#include <pebble.h>

// ── Keys ──────────────────────────────────────────────────
#define KEY_FONT_CHOICE  0
#define KEY_BG_CHOICE    1
#define KEY_BATT_STYLE   2  // 0=off  1=stars

// ── Star geometry ─────────────────────────────────────────
#define STAR_R_OUT  8    // outer radius
#define STAR_R_IN   3    // inner radius
#define STAR_COUNT  5    // 5 stars = 20% each
#define STAR_GAP    6    // vertical gap between stars
#define STAR_CX     14   // center x of star column (top-left area)
#define STAR_TOP    8    // y of first star center

// layer is fixed top-left: x=4, y=4, w=28, h=(5*16 + 4*6) = 104
#define BATT_X  4
#define BATT_Y  4
#define BATT_W  28
#define BATT_H  104

// ── State ──────────────────────────────────────────────────
static Window      *s_window;
static BitmapLayer *s_bg_layer;
static GBitmap     *s_bg_bitmap;
static TextLayer   *s_time_layer;
static TextLayer   *s_seconds_layer;
static Layer       *s_batt_layer;
static GFont        s_time_font;

static int s_font_choice = 0;
static int s_bg_choice   = 0;
static int s_batt_style  = 0;
static int s_batt_pct    = 100;

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
static GColor get_star_fill() {
  return (s_bg_choice == 1) ? GColorWhite : GColorBlack;
}

// ── Star draw helper ───────────────────────────────────────
// Draws a single 5-point star centered at (cx, cy)
// filled=true → solid, filled=false → outline only
static void draw_star(GContext *ctx, int cx, int cy, bool filled) {
  // 10 points alternating outer/inner radius, starting from top (270°)
  GPoint pts[10];
  for (int i = 0; i < 10; i++) {
    // angle in degrees: start at -90 (top), step 36°
    int32_t angle = DEG_TO_TRIGANGLE(-90 + i * 36);
    int32_t rad   = (i % 2 == 0) ? STAR_R_OUT : STAR_R_IN;
    pts[i] = GPoint(
      cx + (int)(rad * cos_lookup(angle) / TRIG_MAX_RATIO),
      cy + (int)(rad * sin_lookup(angle) / TRIG_MAX_RATIO)
    );
  }

  GPathInfo info = { .num_points = 10, .points = pts };
  GPath *path = gpath_create(&info);

  if (filled) {
    graphics_context_set_fill_color(ctx, get_star_fill());
    gpath_draw_filled(ctx, path);
  } else {
    graphics_context_set_fill_color(ctx, GColorClear);
    gpath_draw_filled(ctx, path);
    graphics_context_set_stroke_color(ctx, get_star_fill());
    graphics_context_set_stroke_width(ctx, 1);
    gpath_draw_outline(ctx, path);
  }

  gpath_destroy(path);
}

// ── Battery layer draw ─────────────────────────────────────
static void batt_layer_draw(Layer *layer, GContext *ctx) {
  if (!s_batt_style) return;

  // filled = how many stars lit (each = 20%), drain bottom-up
  int filled = (s_batt_pct + 19) / 20;  // 0–5, rounds up
  int step   = STAR_R_OUT * 2 + STAR_GAP;

  for (int i = 0; i < STAR_COUNT; i++) {
    int cy = STAR_TOP + i * step;
    // star 0 = top, star 4 = bottom
    // bottom drains first: star (STAR_COUNT-1-i) < filled means lit
    bool is_filled = ((STAR_COUNT - 1 - i) < filled);
    draw_star(ctx, STAR_CX, cy, is_filled);
  }
}

// ── Battery service ────────────────────────────────────────
static void batt_handler(BatteryChargeState state) {
  s_batt_pct = state.charge_percent;
  if (s_batt_layer) layer_mark_dirty(s_batt_layer);
}

// ── Time ───────────────────────────────────────────────────
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
}

// ── Apply settings ─────────────────────────────────────────
static void apply_settings() {
  Layer *root   = window_get_root_layer(s_window);
  GRect  bounds = layer_get_bounds(root);

  if (s_bg_bitmap) gbitmap_destroy(s_bg_bitmap);
  s_bg_bitmap = gbitmap_create_with_resource(BG_RESOURCE_IDS[s_bg_choice]);
  bitmap_layer_set_bitmap(s_bg_layer, s_bg_bitmap);

  if (s_time_font) fonts_unload_custom_font(s_time_font);
  s_time_font = fonts_load_custom_font(resource_get_handle(FONT_RESOURCE_IDS[s_font_choice]));
  text_layer_set_font(s_time_layer, s_time_font);

  text_layer_set_text_color(s_time_layer,    get_text_color());
  text_layer_set_text_color(s_seconds_layer, get_sec_color());

  int lh = TIME_LAYER_H[s_font_choice];
  layer_set_frame(text_layer_get_layer(s_time_layer),
                  GRect(4, bounds.size.h - lh - 4, 130, lh));

  layer_set_hidden(s_batt_layer, !s_batt_style);
  layer_mark_dirty(s_batt_layer);
}

// ── AppMessage ─────────────────────────────────────────────
static void inbox_received(DictionaryIterator *iter, void *ctx) {
  Tuple *t;
  t = dict_find(iter, KEY_FONT_CHOICE);
  if (t) { s_font_choice = (int)t->value->int32; persist_write_int(KEY_FONT_CHOICE, s_font_choice); }
  t = dict_find(iter, KEY_BG_CHOICE);
  if (t) { s_bg_choice   = (int)t->value->int32; persist_write_int(KEY_BG_CHOICE,   s_bg_choice);   }
  t = dict_find(iter, KEY_BATT_STYLE);
  if (t) { s_batt_style  = (int)t->value->int32; persist_write_int(KEY_BATT_STYLE,  s_batt_style);  }
  apply_settings();
}

// ── Window lifecycle ───────────────────────────────────────
static void window_load(Window *window) {
  Layer *root   = window_get_root_layer(window);
  GRect  bounds = layer_get_bounds(root);

  s_font_choice = persist_exists(KEY_FONT_CHOICE) ? persist_read_int(KEY_FONT_CHOICE) : 0;
  s_bg_choice   = persist_exists(KEY_BG_CHOICE)   ? persist_read_int(KEY_BG_CHOICE)   : 0;
  s_batt_style  = persist_exists(KEY_BATT_STYLE)  ? persist_read_int(KEY_BATT_STYLE)  : 0;

  // Background
  s_bg_bitmap = NULL;
  s_bg_layer  = bitmap_layer_create(bounds);
  bitmap_layer_set_compositing_mode(s_bg_layer, GCompOpAssign);
  layer_add_child(root, bitmap_layer_get_layer(s_bg_layer));

  // Battery layer — fixed top-left
  s_batt_layer = layer_create(GRect(BATT_X, BATT_Y, BATT_W, BATT_H));
  layer_set_update_proc(s_batt_layer, batt_layer_draw);
  layer_add_child(root, s_batt_layer);

  // Seconds — top right
  s_seconds_layer = text_layer_create(GRect(bounds.size.w - 36, 5, 32, 18));
  text_layer_set_background_color(s_seconds_layer, GColorClear);
  text_layer_set_font(s_seconds_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(s_seconds_layer, GTextAlignmentRight);
  layer_add_child(root, text_layer_get_layer(s_seconds_layer));

  // Time — bottom left
  s_time_font  = NULL;
  s_time_layer = text_layer_create(GRect(4, bounds.size.h - 40, 130, 40));
  text_layer_set_background_color(s_time_layer, GColorClear);
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentLeft);
  layer_add_child(root, text_layer_get_layer(s_time_layer));

  s_batt_pct = battery_state_service_peek().charge_percent;
  battery_state_service_subscribe(batt_handler);

  apply_settings();
  update_time();
}

static void window_unload(Window *window) {
  battery_state_service_unsubscribe();
  bitmap_layer_destroy(s_bg_layer);
  if (s_bg_bitmap) gbitmap_destroy(s_bg_bitmap);
  if (s_time_font) fonts_unload_custom_font(s_time_font);
  layer_destroy(s_batt_layer);
  text_layer_destroy(s_time_layer);
  text_layer_destroy(s_seconds_layer);
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
