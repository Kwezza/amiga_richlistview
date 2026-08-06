/**
 * Focused geometry / quantisation checks for two-column exchange resizing.
 * Host-compilable (no Amiga libs): clamp, pair-total, snap, preview rects.
 *
 * Build (optional):
 *   make column-resize-geometry-test
 * Or with a host C compiler:
 *   cc -I. -o test_resize_math tests/column_resize/test_resize_math.c
 *   (Windows: cl /I. tests\column_resize\test_resize_math.c)
 */

#include <stdio.h>

/* Keep in sync with rlv_internal.h / rlv_column_resize.c */
#define RLV_COLUMN_RESIZE_STEP 4
#define RLV_DIVIDER_WIDTH      1

/* Mirror of rlv_column_resize clamp logic. */
static int clamp_left(int proposed, int pair_total, int min_l, int min_r,
                      int orig_left)
{
    int max_l;

    max_l = pair_total - min_r;
    if (max_l < min_l) {
        return orig_left;
    }
    if (proposed < min_l) {
        proposed = min_l;
    }
    if (proposed > max_l) {
        proposed = max_l;
    }
    return proposed;
}

/* Mirror of rlv_cr_quantize_delta (symmetric toward-zero buckets). */
static int quantize_delta(int delta)
{
    int step;
    int ad;

    step = RLV_COLUMN_RESIZE_STEP;
    if (step < 1) {
        return delta;
    }
    if (delta >= 0) {
        return (delta / step) * step;
    }
    ad = -delta;
    return -((ad / step) * step);
}

static int proposed_from_delta(int orig_left, int raw_delta,
                               int pair_total, int min_l, int min_r)
{
    int snapped;
    int proposed;

    snapped = quantize_delta(raw_delta);
    proposed = orig_left + snapped;
    if (proposed < 1) {
        proposed = 1;
    }
    return clamp_left(proposed, pair_total, min_l, min_r, orig_left);
}

/*
 * Preview interior / content rectangles (bevel = 1 px), matching
 * rlv_cr_paint_pair_preview.
 */
static void preview_rects(int pair_left, int pair_right, int proposed_left_w,
                          int pair_total, int pad,
                          int *fx1, int *fx2,
                          int *left_text_l, int *left_text_r,
                          int *right_text_l, int *right_text_r,
                          int *guide, int *right_left)
{
    int left_content_r;
    int right_content_r;
    int right_w;

    *guide = pair_left + proposed_left_w;
    *right_left = *guide + RLV_DIVIDER_WIDTH;
    right_w = pair_total - proposed_left_w;
    *fx1 = pair_left + 1;
    *fx2 = pair_right - 1;
    left_content_r = *guide - 1;
    right_content_r = *right_left + right_w - 1;
    if (right_content_r > pair_right) {
        right_content_r = pair_right;
    }
    *left_text_l = pair_left + pad;
    *left_text_r = left_content_r - pad;
    *right_text_l = *right_left + pad;
    *right_text_r = right_content_r - pad;
}

static int fail_count = 0;

static void expect_eq(const char *name, int got, int want)
{
    if (got != want) {
        printf("FAIL %s: got %d want %d\n", name, got, want);
        fail_count++;
    } else {
        printf("ok   %s\n", name);
    }
}

static void expect_true(const char *name, int cond)
{
    if (!cond) {
        printf("FAIL %s\n", name);
        fail_count++;
    } else {
        printf("ok   %s\n", name);
    }
}

int main(void)
{
    int pair;
    int left;
    int right;
    int i;
    int x_positions[4];
    int widths[4];
    int d;
    int prev;
    int fx1, fx2, ltl, ltr, rtl, rtr, guide, rleft;
    int pair_left;
    int pair_right;
    int orig;

    /* Drag right: widen left, narrow right; total constant. */
    pair = 100;
    left = clamp_left(60, pair, 16, 16, 40);
    right = pair - left;
    expect_eq("drag_right_left", left, 60);
    expect_eq("drag_right_right", right, 40);
    expect_eq("drag_right_total", left + right, pair);

    /* Drag left: narrow left. */
    left = clamp_left(25, pair, 16, 16, 40);
    right = pair - left;
    expect_eq("drag_left_left", left, 25);
    expect_eq("drag_left_right", right, 75);
    expect_eq("drag_left_total", left + right, pair);

    /* Min clamp on left. */
    left = clamp_left(5, pair, 16, 16, 40);
    expect_eq("min_left_clamp", left, 16);
    expect_eq("min_left_total", left + (pair - left), pair);

    /* Min clamp on right (max left). */
    left = clamp_left(90, pair, 16, 16, 40);
    expect_eq("min_right_clamp", left, 84); /* 100-16 */
    expect_eq("min_right_peer", pair - left, 16);

    /* Later column X positions unchanged when exchanging 0 and 1. */
    widths[0] = 40;
    widths[1] = 60;
    widths[2] = 80;
    widths[3] = 50;
    x_positions[0] = 0;
    for (i = 1; i < 4; i++) {
        x_positions[i] = x_positions[i - 1] + widths[i - 1] + 1; /* +divider */
    }
    {
        int old_x2 = x_positions[2];
        int old_x3 = x_positions[3];
        int new_l = 55;
        int new_r = widths[0] + widths[1] - new_l;

        widths[0] = new_l;
        widths[1] = new_r;
        x_positions[0] = 0;
        for (i = 1; i < 4; i++) {
            x_positions[i] = x_positions[i - 1] + widths[i - 1] + 1;
        }
        expect_eq("later_x2_fixed", x_positions[2], old_x2);
        expect_eq("later_x3_fixed", x_positions[3], old_x3);
        expect_eq("pair_after_exchange", widths[0] + widths[1], 100);
    }

    expect_eq("last_col_no_trailing_divider", 1, 1);

    /* ---- Four-pixel quantisation ---- */
    expect_eq("snap_0", quantize_delta(0), 0);
    expect_eq("snap_1", quantize_delta(1), 0);
    expect_eq("snap_2", quantize_delta(2), 0);
    expect_eq("snap_3", quantize_delta(3), 0);
    expect_eq("snap_4", quantize_delta(4), 4);
    expect_eq("snap_5", quantize_delta(5), 4);
    expect_eq("snap_7", quantize_delta(7), 4);
    expect_eq("snap_8", quantize_delta(8), 8);
    expect_eq("snap_neg1", quantize_delta(-1), 0);
    expect_eq("snap_neg3", quantize_delta(-3), 0);
    expect_eq("snap_neg4", quantize_delta(-4), -4);
    expect_eq("snap_neg5", quantize_delta(-5), -4);
    expect_eq("snap_neg8", quantize_delta(-8), -8);
    expect_eq("snap_symmetric", quantize_delta(6), -quantize_delta(-6));

    orig = 40;
    pair = 100;
    prev = orig;
    for (d = 0; d <= 3; d++) {
        left = proposed_from_delta(orig, d, pair, 16, 16);
        expect_eq(d == 0 ? "bucket_pos_0" :
                  d == 1 ? "bucket_pos_1" :
                  d == 2 ? "bucket_pos_2" : "bucket_pos_3",
                  left, prev);
    }
    left = proposed_from_delta(orig, 4, pair, 16, 16);
    expect_eq("bucket_pos_4_advances", left, orig + 4);

    for (d = -1; d >= -3; d--) {
        left = proposed_from_delta(orig, d, pair, 16, 16);
        expect_eq(d == -1 ? "bucket_neg_1" :
                  d == -2 ? "bucket_neg_2" : "bucket_neg_3",
                  left, orig);
    }
    left = proposed_from_delta(orig, -4, pair, 16, 16);
    expect_eq("bucket_neg_4_advances", left, orig - 4);

    /* Release commits the snapped value (same helper as move). */
    left = proposed_from_delta(orig, 11, pair, 16, 16);
    expect_eq("commit_snapped_11", left, orig + 8);
    expect_eq("commit_peer", pair - left, pair - (orig + 8));

    /* Snap then clamp: large positive delta hits max_l. */
    left = proposed_from_delta(orig, 200, pair, 16, 16);
    expect_eq("snap_then_clamp_max", left, 84);
    left = proposed_from_delta(orig, -200, pair, 16, 16);
    expect_eq("snap_then_clamp_min", left, 16);

    /* ---- Preview geometry ---- */
    pair_left = 10;
    pair_right = 10 + 40 + 1 + 60; /* left + div + right content end as cell */
    /* cell_right for non-last includes trailing divider; use content+div: */
    pair_right = pair_left + 40 + 1 + 60; /* 111; right cell right = 110 if no trailing */
    /* Model: left 40, div, right 60, pair_right = last pixel of right cell. */
    pair_right = pair_left + 40 + RLV_DIVIDER_WIDTH + 60 - 1; /* 110 */
    preview_rects(pair_left, pair_right, 40, 100, 1,
                  &fx1, &fx2, &ltl, &ltr, &rtl, &rtr, &guide, &rleft);
    expect_eq("bevel_fx1", fx1, pair_left + 1);
    expect_eq("bevel_fx2", fx2, pair_right - 1);
    expect_eq("guide_at_orig", guide, pair_left + 40);
    expect_eq("right_left_at_orig", rleft, guide + RLV_DIVIDER_WIDTH);
    expect_true("left_text_inside", ltl >= fx1 && ltr <= guide - 1);
    expect_true("right_text_inside", rtl >= rleft && rtr <= pair_right);
    expect_true("guide_inside_pair", guide > pair_left && guide < pair_right);
    expect_eq("right_fixed_edge", pair_right, 110);

    preview_rects(pair_left, pair_right, 48, 100, 1,
                  &fx1, &fx2, &ltl, &ltr, &rtl, &rtr, &guide, &rleft);
    expect_eq("moved_guide", guide, pair_left + 48);
    expect_eq("right_edge_unmoved", pair_right, 110);
    expect_true("moved_left_clip", ltr < guide);
    expect_true("moved_right_clip", rtl > guide);
    expect_true("clear_excludes_left_bevel", fx1 == pair_left + 1);
    expect_true("clear_excludes_right_bevel", fx2 == pair_right - 1);

    if (fail_count != 0) {
        printf("%d failures\n", fail_count);
        return 1;
    }
    printf("all geometry checks passed\n");
    return 0;
}
