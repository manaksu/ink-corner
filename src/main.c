#include <pebble.h>

// ── Keys ─────────────────────────────────────────────────
#define KEY_FONT_CHOICE    0
#define KEY_BG_CHOICE      1
#define KEY_BATT_SHOW      2  // 0=off 1=triangles
#define KEY_BATT_LOC       3  // 0=center 1=top-left 2=bottom-right 3=bottom-left (above time)

// ── State ─────────────────────────────────────────────────
static Window      *s_window;
static BitmapLayer *s_bg_layer;
static GBitmap     *s_bg_bitmap;
static TextLayer   *s_time_layer;
static TextLayer   *s_seconds_layer;
static Layer       *s_batt_layer;
static GFont        s_time_font;

static int s_font_choice = 0;
static int s_bg_choice   = 0;
static int s_batt_show   = 0;
static int s_batt_loc    = 0;
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

// ── Colors ────────────────────────────────────────────────
static GColor get_text_color() {
  return (s_bg_choice == 1) ? GColorWhite : GColorBlack;
}
static GColor get_sec_color() {
  return (s_bg_choice == 1) ? GColorLightGray : GColorDarkGray;
}
static GColor get_batt_fill() {
  return (s_bg_choice == 1) ? GColorWhite : GColorBlack;
}
static GColor get_batt_empty() {
  // very faint — just outline visible
  return GColorClear;
}

// ── Triangle battery drawing ───────────────────────────────
// 10 equilateral downward-pointing triangles in inverted pyramid
// rows: 4, 3, 2, 1  (top to bottom)
// side = 10px  height = side * sqrt(3)/2 ≈ 8.66 → use 9
// hgap = 3, vgap = 4
#define TRI_S    18
#define TRI_H    16    // floor(18 * sqrt(3)/2)
#define TRI_HGAP 5
#define TRI_VGAP 6

// fill order: bottom tri first (index 9), top row last (0-3)
static const int FILL_ORDER[10] = { 9, 7, 8, 4, 5, 6, 0, 1, 2, 3 };

static void batt_layer_draw(Layer *layer, GContext *ctx) {
  if (!s_batt_show) return;

  GRect bounds = layer_get_bounds(layer);
  int filled = (s_batt_pct + 9) / 10;  // round up, 0-10

  int row_counts[4] = { 4, 3, 2, 1 };
  int tri_index = 0;

  // total height of arrangement
  int total_h = 4 * TRI_H + 3 * TRI_VGAP;
  int total_w = 4 * TRI_S + 3 * TRI_HGAP;
  int ox = (bounds.size.w - total_w) / 2;
  int oy = (bounds.size.h - total_h) / 2;

  for (int r = 0; r < 4; r++) {
    int count = row_counts[r];
    int row_w = count * TRI_S + (count - 1) * TRI_HGAP;
    int rx = ox + (total_w - row_w) / 2;
    int ry = oy + r * (TRI_H + TRI_VGAP);

    for (int i = 0; i < count; i++) {
      int cx = rx + i * (TRI_S + TRI_HGAP) + TRI_S / 2;
      int ty = ry;

      // check if this triangle is filled
      bool is_filled = false;
      for (int f = 0; f < filled; f++) {
        if (FILL_ORDER[f] == tri_index) { is_filled = true; break; }
      }

      // draw equilateral tri tip-down: flat top, point at bottom
      GPathInfo info = {
        .num_points = 3,
        .points = (GPoint[]) {
          { cx - TRI_S/2, ty },
          { cx + TRI_S/2, ty },
          { cx,           ty + TRI_H }
        }
      };
      GPath *path = gpath_create(&info);

      if (is_filled) {
        graphics_context_set_fill_color(ctx, get_batt_fill());
        gpath_draw_filled(ctx, path);
      } else {
        // draw just the outline
        graphics_context_set_stroke_color(ctx, get_batt_fill());
        graphics_context_set_stroke_width(ctx, 1);
        graphics_context_set_fill_color(ctx, get_batt_empty());
        gpath_draw_filled(ctx, path);
        gpath_draw_outline(ctx, path);
      }

      gpath_destroy(path);
      tri_index++;
    }
  }
}

// ── Battery location → GRect ───────────────────────────────
// layer is 60x60 (fits 4×10 + gaps comfortably)
#define BATT_W 100
#define BATT_H 100

static GRect batt_frame() {
  // screen is 144x168
  switch (s_batt_loc) {
    case 0:  // center
      return GRect((144 - BATT_W) / 2, (168 - BATT_H) / 2, BATT_W, BATT_H);
    case 1:  // top-left
      return GRect(4, 4, BATT_W, BATT_H);
    case 2:  // bottom-right
      return GRect(144 - BATT_W - 4, 168 - BATT_H - 4, BATT_W, BATT_H);
    case 3:  // bottom-left, above time
    default: {
      int lh = TIME_LAYER_H[s_font_choice];
      return GRect(4, 168 - lh - BATT_H - 8, BATT_W, BATT_H);
    }
  }
}

// ── Time ──────────────────────────────────────────────────
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

// ── Battery service ───────────────────────────────────────
static void batt_handler(BatteryChargeState state) {
  s_batt_pct = state.charge_percent;
  if (s_batt_layer) layer_mark_dirty(s_batt_layer);
}

// ── Apply settings ────────────────────────────────────────
static void apply_settings() {
  Layer *root   = window_get_root_layer(s_window);
  GRect  bounds = layer_get_bounds(root);

  // Background
  if (s_bg_bitmap) gbitmap_destroy(s_bg_bitmap);
  s_bg_bitmap = gbitmap_create_with_resource(BG_RESOURCE_IDS[s_bg_choice]);
  bitmap_layer_set_bitmap(s_bg_layer, s_bg_bitmap);

  // Font
  if (s_time_font) fonts_unload_custom_font(s_time_font);
  s_time_font = fonts_load_custom_font(resource_get_handle(FONT_RESOURCE_IDS[s_font_choice]));
  text_layer_set_font(s_time_layer, s_time_font);

  // Text colors
  text_layer_set_text_color(s_time_layer,    get_text_color());
  text_layer_set_text_color(s_seconds_layer, get_sec_color());

  // Time layer position
  int lh = TIME_LAYER_H[s_font_choice];
  layer_set_frame(text_layer_get_layer(s_time_layer),
                  GRect(4, bounds.size.h - lh - 4, 130, lh));

  // Battery layer visibility + position
  layer_set_hidden(s_batt_layer, !s_batt_show);
  if (s_batt_show) {
    layer_set_frame(s_batt_layer, batt_frame());
    layer_mark_dirty(s_batt_layer);
  }
}

// ── AppMessage ────────────────────────────────────────────
static void inbox_received(DictionaryIterator *iter, void *ctx) {
  Tuple *t;
  t = dict_find(iter, KEY_FONT_CHOICE);
  if (t) { s_font_choice = (int)t->value->int32; persist_write_int(KEY_FONT_CHOICE, s_font_choice); }
  t = dict_find(iter, KEY_BG_CHOICE);
  if (t) { s_bg_choice   = (int)t->value->int32; persist_write_int(KEY_BG_CHOICE,   s_bg_choice);   }
  t = dict_find(iter, KEY_BATT_SHOW);
  if (t) { s_batt_show   = (int)t->value->int32; persist_write_int(KEY_BATT_SHOW,   s_batt_show);   }
  t = dict_find(iter, KEY_BATT_LOC);
  if (t) { s_batt_loc    = (int)t->value->int32; persist_write_int(KEY_BATT_LOC,    s_batt_loc);    }
  apply_settings();
}

// ── Window lifecycle ──────────────────────────────────────
static void window_load(Window *window) {
  Layer *root   = window_get_root_layer(window);
  GRect  bounds = layer_get_bounds(root);

  s_font_choice = persist_exists(KEY_FONT_CHOICE) ? persist_read_int(KEY_FONT_CHOICE) : 0;
  s_bg_choice   = persist_exists(KEY_BG_CHOICE)   ? persist_read_int(KEY_BG_CHOICE)   : 0;
  s_batt_show   = persist_exists(KEY_BATT_SHOW)   ? persist_read_int(KEY_BATT_SHOW)   : 0;
  s_batt_loc    = persist_exists(KEY_BATT_LOC)    ? persist_read_int(KEY_BATT_LOC)    : 0;

  // Background
  s_bg_bitmap = NULL;
  s_bg_layer  = bitmap_layer_create(bounds);
  bitmap_layer_set_compositing_mode(s_bg_layer, GCompOpAssign);
  layer_add_child(root, bitmap_layer_get_layer(s_bg_layer));

  // Battery layer
  s_batt_layer = layer_create(batt_frame());
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

  // Get initial battery
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

// ── Init / deinit ─────────────────────────────────────────
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
