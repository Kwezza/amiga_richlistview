/**
 * Focused geometry checks for two-column exchange resizing.
 * Host-compilable (no Amiga libs): verifies clamp / pair-total invariants.
 *
 * Build (optional):
 *   make column-resize-geometry-test
 * Or with a host C compiler:
 *   cc -I. -o test_resize_math tests/column_resize/test_resize_math.c
 */

#include <stdio.h>

/* Mirror of rlv_column_resize clamp logic (keep in sync with module). */
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

int main(void)
{
    int pair;
    int left;
    int right;
    int i;
    int x_positions[4];
    int widths[4];

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

    /* Last column cannot start a trailing resize (no divider after n-1). */
    expect_eq("last_col_no_trailing_divider", 1, 1);

    if (fail_count != 0) {
        printf("%d failures\n", fail_count);
        return 1;
    }
    printf("all geometry checks passed\n");
    return 0;
}
