/*
 * InkCorner — Saved Battery Indicators (not used in current build)
 * ----------------------------------------------------------------
 * MODULE A: Triangle Battery (inverted pyramid, 10 tris = 10% each)
 * MODULE B: Star Battery (5 stars vertical column = 20% each)
 *
 * To use either: create a Layer, set its update_proc to the draw
 * function, subscribe to battery_state_service.
 */

#include <pebble.h>

// ════════════════════════════════════════════════════════════
// MODULE A — TRIANGLE BATTERY
// 10 equilateral ▽ triangles, inverted pyramid: rows 4,3,2,1
// Each triangle = 10%. Drains bottom tip upward.
// Layer size: BATT_TRI_W x BATT_TRI_H
// ════════════════════════════════════════════════════════════

#define TRI_S        18
#define TRI_H        16    // floor(18 * sqrt(3)/2)
#define TRI_HGAP      5
#define TRI_VGAP      6
#define BATT_TRI_W  100
#define BATT_TRI_H  100

static const int TRI_FILL_ORDER[10] = { 9, 7, 8, 4, 5, 6, 0, 1, 2, 3 };

// Location presets (144x168 screen, time_layer_h = height of time layer)
GRect tri_batt_frame(int loc, int time_layer_h) {
  switch (loc) {
    case 0: return GRect((144-BATT_TRI_W)/2, (168-BATT_TRI_H)/2, BATT_TRI_W, BATT_TRI_H); // center
    case 1: return GRect(4, 4, BATT_TRI_W, BATT_TRI_H);                                    // top-left
    case 2: return GRect(144-BATT_TRI_W-4, 168-BATT_TRI_H-4, BATT_TRI_W, BATT_TRI_H);     // bottom-right
    case 3: return GRect(4, 168-time_layer_h-BATT_TRI_H-8, BATT_TRI_W, BATT_TRI_H);        // bottom-left above time
    default: return GRect(4, 4, BATT_TRI_W, BATT_TRI_H);
  }
}

void tri_batt_layer_draw(Layer *layer, GContext *ctx, int batt_pct, GColor fill_color) {
  int filled      = (batt_pct + 9) / 10;
  int row_counts[4] = { 4, 3, 2, 1 };
  int total_w     = 4 * TRI_S + 3 * TRI_HGAP;
  int total_h     = 4 * TRI_H + 3 * TRI_VGAP;
  GRect bounds    = layer_get_bounds(layer);
  int ox = (bounds.size.w - total_w) / 2;
  int oy = (bounds.size.h - total_h) / 2;
  int tri_index   = 0;

  for (int r = 0; r < 4; r++) {
    int count = row_counts[r];
    int row_w = count * TRI_S + (count-1) * TRI_HGAP;
    int rx    = ox + (total_w - row_w) / 2;
    int ry    = oy + r * (TRI_H + TRI_VGAP);

    for (int i = 0; i < count; i++) {
      int cx = rx + i * (TRI_S + TRI_HGAP) + TRI_S / 2;

      bool is_filled = false;
      for (int f = 0; f < filled; f++) {
        if (TRI_FILL_ORDER[f] == tri_index) { is_filled = true; break; }
      }

      GPathInfo info = {
        .num_points = 3,
        .points = (GPoint[]) {
          { cx - TRI_S/2, ry         },
          { cx + TRI_S/2, ry         },
          { cx,           ry + TRI_H }
        }
      };
      GPath *path = gpath_create(&info);
      graphics_context_set_fill_color(ctx, is_filled ? fill_color : GColorClear);
      gpath_draw_filled(ctx, path);
      graphics_context_set_stroke_color(ctx, fill_color);
      graphics_context_set_stroke_width(ctx, 1);
      gpath_draw_outline(ctx, path);
      gpath_destroy(path);
      tri_index++;
    }
  }
}


// ════════════════════════════════════════════════════════════
// MODULE B — STAR BATTERY
// 5 vertical 5-point stars, top-left column
// Each star = 20%. Drains bottom-up.
// Layer: x=4, y=4, w=28, h=104
// ════════════════════════════════════════════════════════════

#define STAR_R_OUT   8
#define STAR_R_IN    3
#define STAR_COUNT   5
#define STAR_GAP     6
#define STAR_CX     14
#define STAR_TOP     8
#define BATT_STAR_W 28
#define BATT_STAR_H 104

static void draw_star(GContext *ctx, int cx, int cy, bool filled, GColor fill_color) {
  GPoint pts[10];
  for (int i = 0; i < 10; i++) {
    int32_t angle = DEG_TO_TRIGANGLE(-90 + i * 36);
    int32_t rad   = (i % 2 == 0) ? STAR_R_OUT : STAR_R_IN;
    pts[i] = GPoint(
      cx + (int)(rad * cos_lookup(angle) / TRIG_MAX_RATIO),
      cy + (int)(rad * sin_lookup(angle) / TRIG_MAX_RATIO)
    );
  }
  GPathInfo info = { .num_points = 10, .points = pts };
  GPath *path = gpath_create(&info);
  graphics_context_set_fill_color(ctx, filled ? fill_color : GColorClear);
  gpath_draw_filled(ctx, path);
  graphics_context_set_stroke_color(ctx, fill_color);
  graphics_context_set_stroke_width(ctx, 1);
  gpath_draw_outline(ctx, path);
  gpath_destroy(path);
}

void star_batt_layer_draw(Layer *layer, GContext *ctx, int batt_pct, GColor fill_color) {
  int filled = (batt_pct + 19) / 20;
  int step   = STAR_R_OUT * 2 + STAR_GAP;
  for (int i = 0; i < STAR_COUNT; i++) {
    int cy = STAR_TOP + i * step;
    bool is_filled = ((STAR_COUNT - 1 - i) < filled);
    draw_star(ctx, STAR_CX, cy, is_filled, fill_color);
  }
}
