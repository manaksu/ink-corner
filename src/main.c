#include <pebble.h>

#define KEY_FONT_CHOICE 0
#define KEY_BG_CHOICE   1

static Window      *s_window;
static BitmapLayer *s_bg_layer;
static GBitmap     *s_bg_bitmap;
static TextLayer   *s_time_layer;
static TextLayer   *s_seconds_layer;
static GFont        s_time_font;

static int s_font_choice = 0;  // 0=Regular24 1=Regular28 2=Medium24 3=Medium28
static int s_bg_choice   = 0;  // 0=Cream 1=Black 2=White

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

// Text color per background (Basalt 64-color palette)
static GColor get_text_color() {
  return (s_bg_choice == 1) ? GColorWhite : GColorBlack;
}

static GColor get_sec_color() {
  if (s_bg_choice == 1) return GColorLightGray;
  return GColorDarkGray;
}

static const int TIME_LAYER_H[4] = { 34, 40, 34, 40 };

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

  // Colors
  text_layer_set_text_color(s_time_layer,    get_text_color());
  text_layer_set_text_color(s_seconds_layer, get_sec_color());

  // Reposition time layer
  int lh = TIME_LAYER_H[s_font_choice];
  layer_set_frame(text_layer_get_layer(s_time_layer),
                  GRect(4, bounds.size.h - lh - 4, 130, lh));
}

static void inbox_received(DictionaryIterator *iter, void *ctx) {
  Tuple *t;
  t = dict_find(iter, MESSAGE_KEY_FONT_CHOICE);
  if (t) { s_font_choice = (int)t->value->int32; persist_write_int(KEY_FONT_CHOICE, s_font_choice); }
  t = dict_find(iter, MESSAGE_KEY_BG_CHOICE);
  if (t) { s_bg_choice   = (int)t->value->int32; persist_write_int(KEY_BG_CHOICE,   s_bg_choice);   }
  apply_settings();
}

static void window_load(Window *window) {
  Layer *root   = window_get_root_layer(window);
  GRect  bounds = layer_get_bounds(root);

  s_font_choice = persist_exists(KEY_FONT_CHOICE) ? persist_read_int(KEY_FONT_CHOICE) : 0;
  s_bg_choice   = persist_exists(KEY_BG_CHOICE)   ? persist_read_int(KEY_BG_CHOICE)   : 0;

  // BG
  s_bg_bitmap = NULL;
  s_bg_layer  = bitmap_layer_create(bounds);
  bitmap_layer_set_compositing_mode(s_bg_layer, GCompOpAssign);
  layer_add_child(root, bitmap_layer_get_layer(s_bg_layer));

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

  apply_settings();
  update_time();
}

static void window_unload(Window *window) {
  bitmap_layer_destroy(s_bg_layer);
  if (s_bg_bitmap) gbitmap_destroy(s_bg_bitmap);
  if (s_time_font) fonts_unload_custom_font(s_time_font);
  text_layer_destroy(s_time_layer);
  text_layer_destroy(s_seconds_layer);
}

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
