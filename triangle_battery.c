/*
 * InkCorner — Triangle Battery Indicator
 * Standalone module — drop into any Pebble watchface.
 *
 * 10 equilateral downward-pointing (▽) triangles
 * arranged in an inverted pyramid: rows of 4, 3, 2, 1
 * Each triangle = 10% battery.
 * Drains from the bottom tip upward.
 *
 * Usage:
 *   1. Create a Layer sized BATT_W x BATT_H and set its
 *      update_proc to batt_layer_draw.
 *   2. Call battery_state_service_subscribe(batt_handler)
 *      and layer_mark_dirty on the layer from batt_handler.
 *   3. Call batt_layer_draw_set_colors() to match your bg.
 *
 * Location presets (144x168 screen):
 *   case 0: center
 *   case 1: top-left
 *   case 2: bottom-right
 *   case 3: bottom-left, above time
 */

#include <pebble.h>

#define TRI_S    18          // equilateral side length px
#define TRI_H    16          // floor(TRI_S * sqrt(3)/2)
#define TRI_HGAP  5          // horizontal gap between triangles
#define TRI_VGAP  6          // vertical gap between rows
#define BATT_W  100          // layer width
#define BATT_H  100          // layer height

static GColor s_batt_fill_color  = {.argb = 0b11000000};  // GColorBlack
static GColor s_batt_empty_color = {.argb = 0b00000000};  // GColorClear
static int    s_batt_pct         = 100;

// Fill order: bottom tip first (index 9), top row last (0-3)
static const int FILL_ORDER[10] = { 9, 7, 8, 4, 5, 6, 0, 1, 2, 3 };

void batt_layer_set_colors(GColor fill, GColor empty) {
  s_batt_fill_color  = fill;
  s_batt_empty_color = empty;
}

void batt_layer_draw(Layer *layer, GContext *ctx) {
  int filled = (s_batt_pct + 9) / 10;  // 0–10, rounds up

  int row_counts[4] = { 4, 3, 2, 1 };
  int total_w = 4 * TRI_S + 3 * TRI_HGAP;
  int total_h = 4 * TRI_H + 3 * TRI_VGAP;
  GRect bounds = layer_get_bounds(layer);
  int ox = (bounds.size.w - total_w) / 2;
  int oy = (bounds.size.h - total_h) / 2;
  int tri_index = 0;

  for (int r = 0; r < 4; r++) {
    int count   = row_counts[r];
    int row_w   = count * TRI_S + (count - 1) * TRI_HGAP;
    int rx      = ox + (total_w - row_w) / 2;
    int ry      = oy + r * (TRI_H + TRI_VGAP);

    for (int i = 0; i < count; i++) {
      int cx = rx + i * (TRI_S + TRI_HGAP) + TRI_S / 2;

      bool is_filled = false;
      for (int f = 0; f < filled; f++) {
        if (FILL_ORDER[f] == tri_index) { is_filled = true; break; }
      }

      GPathInfo info = {
        .num_points = 3,
        .points = (GPoint[]) {
          { cx - TRI_S / 2, ry          },
          { cx + TRI_S / 2, ry          },
          { cx,             ry + TRI_H  }
        }
      };
      GPath *path = gpath_create(&info);

      graphics_context_set_fill_color(ctx, is_filled ? s_batt_fill_color : s_batt_empty_color);
      gpath_draw_filled(ctx, path);
      graphics_context_set_stroke_color(ctx, is_filled ? s_batt_fill_color : s_batt_fill_color);
      graphics_context_set_stroke_width(ctx, 1);
      gpath_draw_outline(ctx, path);

      gpath_destroy(path);
      tri_index++;
    }
  }
}

void batt_handler(BatteryChargeState state) {
  s_batt_pct = state.charge_percent;
}

// Returns the GRect for each location preset
GRect batt_frame_for_location(int loc, int time_layer_h) {
  switch (loc) {
    case 0: return GRect((144 - BATT_W) / 2, (168 - BATT_H) / 2, BATT_W, BATT_H);  // center
    case 1: return GRect(4, 4, BATT_W, BATT_H);                                      // top-left
    case 2: return GRect(144 - BATT_W - 4, 168 - BATT_H - 4, BATT_W, BATT_H);       // bottom-right
    case 3: return GRect(4, 168 - time_layer_h - BATT_H - 8, BATT_W, BATT_H);       // bottom-left above time
    default: return GRect(4, 4, BATT_W, BATT_H);
  }
}
