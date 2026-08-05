/**
 * Amiga V36+ draw backend for the experimental custom ListView control.
 */

#include "rich_listview/backends/rlv_backend_amiga_v36.h"
#include "rich_listview/rlv_log.h"
#include "rich_listview/rlv_bench_internal.h"
#include "rich_listview/rlv_platform_internal.h"

#include <graphics/clip.h>
#include <graphics/gfxbase.h>
#include <graphics/gfxmacros.h>
#include <graphics/layers.h>
#include <intuition/intuition.h>
#include <proto/graphics.h>
#include <proto/layers.h>
#include <proto/intuition.h>

#include <string.h>

extern struct GfxBase *GfxBase;

typedef struct RLV_V36ClipState
{
    struct Layer *layer;
    struct Region *old_region;
    struct Region *new_region;
    struct Rectangle soft; /* software clip; used during LAYERUPDATING */
    BOOL active;
    BOOL no_layer;
    BOOL soft_active;
    /* TRUE when soft clip is the only guard (LAYERUPDATING; no HW clip). */
    BOOL soft_only;
} RLV_V36ClipState;

struct RLV_BackendV36
{
    struct RastPort *rp;
    struct TextFont *font; /* borrowed */
    RLV_V36ClipState clip;
};

#ifndef LAYERS_NOBACKFILL
#define LAYERS_NOBACKFILL  ((struct Hook *)1L)
#endif

#ifndef LAYERREFRESH
#define LAYERREFRESH  0x0002U
#endif

#ifndef LAYERUPDATING
#define LAYERUPDATING  0x0010U
#endif

static VOID rlv_v36_set_pens(APTR ctx, UWORD front, UWORD back);
static VOID rlv_v36_fill_rect(APTR ctx, WORD x1, WORD y1, WORD x2, WORD y2);
static VOID rlv_v36_draw_line(APTR ctx, WORD x1, WORD y1, WORD x2, WORD y2);
static VOID rlv_v36_draw_dotted_hline(APTR ctx, WORD x1, WORD x2, WORD y);
static VOID rlv_v36_draw_xor_vline(APTR ctx, WORD x, WORD y1, WORD y2);
static VOID rlv_v36_draw_text(APTR ctx, WORD x, WORD baseline,
                              CONST_STRPTR text, UWORD length);
static UWORD rlv_v36_text_width(APTR ctx, CONST_STRPTR text, UWORD length);
static UWORD rlv_v36_text_fit(APTR ctx, CONST_STRPTR text, UWORD length,
                              UWORD max_width);
static BOOL rlv_v36_push_clip(APTR ctx, const struct Rectangle *rect);
static VOID rlv_v36_pop_clip(APTR ctx);
static UWORD rlv_v36_line_height(APTR ctx);
static UWORD rlv_v36_baseline(APTR ctx);
#if defined(RLV_ENABLE_SMART_SCROLL) && (RLV_ENABLE_SMART_SCROLL != 0)
static UWORD rlv_v36_move_viewport_pixels(APTR ctx,
                                          const struct Rectangle *viewport,
                                          WORD vertical_delta);
static VOID rlv_v36_finish_viewport_move(APTR ctx);
#endif

static const RLV_DrawOps g_rlv_v36_ops =
{
    rlv_v36_set_pens,
    rlv_v36_fill_rect,
    rlv_v36_draw_line,
    rlv_v36_draw_text,
    rlv_v36_text_width,
    rlv_v36_text_fit,
    rlv_v36_push_clip,
    rlv_v36_pop_clip,
    rlv_v36_line_height,
    rlv_v36_baseline,
#if defined(RLV_ENABLE_SMART_SCROLL) && (RLV_ENABLE_SMART_SCROLL != 0)
    rlv_v36_move_viewport_pixels,
    rlv_v36_finish_viewport_move,
#else
    0,
    0,
#endif
    rlv_v36_draw_dotted_hline,
    rlv_v36_draw_xor_vline
};

static struct RastPort *rlv_v36_rp(RLV_BackendV36 *b)
{
    if (b == 0) {
        return 0;
    }
    return b->rp;
}

RLV_BackendV36 *rlv_backend_v36_create(struct RastPort *rp,
                                       struct TextFont *font)
{
    RLV_BackendV36 *b;

    if (rp == 0) {
        return 0;
    }

    b = (RLV_BackendV36 *)rlv_platform_malloc(sizeof(*b));
    if (b == 0) {
        return 0;
    }
    memset(b, 0, sizeof(*b));
    b->rp = rp;
    b->font = font;
    if (font != 0) {
        SetFont(rp, font);
    }
    return b;
}

VOID rlv_backend_v36_destroy(RLV_BackendV36 *backend)
{
    if (backend == 0) {
        return;
    }
    rlv_v36_pop_clip(backend);
    rlv_platform_free(backend);
}

VOID rlv_backend_v36_set_rastport(RLV_BackendV36 *backend,
                                  struct RastPort *rp)
{
    if (backend == 0) {
        return;
    }
    backend->rp = rp;
    if (rp != 0 && backend->font != 0) {
        SetFont(rp, backend->font);
    }
}

VOID rlv_backend_v36_set_font(RLV_BackendV36 *backend,
                              struct TextFont *font)
{
    if (backend == 0) {
        return;
    }
    backend->font = font;
    if (backend->rp != 0 && font != 0) {
        SetFont(backend->rp, font);
    }
}

const RLV_DrawOps *rlv_backend_v36_get_ops(void)
{
    return &g_rlv_v36_ops;
}

APTR rlv_backend_v36_get_context(RLV_BackendV36 *backend)
{
    return (APTR)backend;
}

VOID rlv_backend_v36_pens_from_drawinfo(const struct DrawInfo *dri,
                                        RLV_Pens *out_pens)
{
    UWORD *pens;

    if (out_pens == 0) {
        return;
    }
    memset(out_pens, 0, sizeof(*out_pens));
    if (dri == 0 || dri->dri_Pens == 0) {
        return;
    }

    pens = dri->dri_Pens;
    out_pens->text = pens[TEXTPEN];
    out_pens->background = pens[BACKGROUNDPEN];
    out_pens->selected_text = pens[FILLTEXTPEN];
    out_pens->selected_background = pens[FILLPEN];
    out_pens->shine = pens[SHINEPEN];
    out_pens->shadow = pens[SHADOWPEN];
    out_pens->separator = pens[SHADOWPEN];
}

static VOID rlv_v36_set_pens(APTR ctx, UWORD front, UWORD back)
{
    struct RastPort *rp = rlv_v36_rp((RLV_BackendV36 *)ctx);

    if (rp == 0) {
        return;
    }
    SetAPen(rp, front);
    SetBPen(rp, back);
    SetDrMd(rp, JAM2);
}

/* Activate soft-only clip (no InstallClipRegion). Clip slot must be clear. */
static VOID rlv_v36_activate_soft_only(RLV_BackendV36 *b,
                                       const struct Rectangle *rect)
{
    if (b == 0 || rect == 0) {
        return;
    }
    b->clip.soft = *rect;
    b->clip.soft_active = TRUE;
    b->clip.soft_only = TRUE;
    b->clip.active = TRUE;
    b->clip.layer = 0;
    b->clip.old_region = 0;
    b->clip.new_region = 0;
}

/* Intersect [x1..x2],[y1..y2] with soft clip; return FALSE if empty. */
static BOOL rlv_v36_soft_intersect(RLV_BackendV36 *b,
                                   WORD *x1, WORD *y1, WORD *x2, WORD *y2)
{
    if (b == 0 || !b->clip.soft_active) {
        return TRUE;
    }
    if (*x1 < b->clip.soft.MinX) {
        *x1 = b->clip.soft.MinX;
    }
    if (*y1 < b->clip.soft.MinY) {
        *y1 = b->clip.soft.MinY;
    }
    if (*x2 > b->clip.soft.MaxX) {
        *x2 = b->clip.soft.MaxX;
    }
    if (*y2 > b->clip.soft.MaxY) {
        *y2 = b->clip.soft.MaxY;
    }
    return (*x2 >= *x1 && *y2 >= *y1) ? TRUE : FALSE;
}

static VOID rlv_v36_fill_rect(APTR ctx, WORD x1, WORD y1, WORD x2, WORD y2)
{
    RLV_BackendV36 *b = (RLV_BackendV36 *)ctx;
    struct RastPort *rp = rlv_v36_rp(b);

    if (rp == 0 || x2 < x1 || y2 < y1) {
        return;
    }
    if (!rlv_v36_soft_intersect(b, &x1, &y1, &x2, &y2)) {
        return;
    }
    /* RectFill uses APen only. */
    RLV_BENCH_COUNT(RLV_BENCH_COUNTER_BACKGROUND_FILLS);
    RectFill(rp, x1, y1, x2, y2);
}

/* Cohen–Sutherland outcodes; Amiga Y grows downward (MinY = top). */
#define RLV_V36_OUT_LEFT   1
#define RLV_V36_OUT_RIGHT  2
#define RLV_V36_OUT_TOP    4
#define RLV_V36_OUT_BOTTOM 8

static UBYTE rlv_v36_outcode(WORD x, WORD y, const struct Rectangle *r)
{
    UBYTE code;

    code = 0;
    if (x < r->MinX) {
        code = (UBYTE)(code | RLV_V36_OUT_LEFT);
    } else if (x > r->MaxX) {
        code = (UBYTE)(code | RLV_V36_OUT_RIGHT);
    }
    if (y < r->MinY) {
        code = (UBYTE)(code | RLV_V36_OUT_TOP);
    } else if (y > r->MaxY) {
        code = (UBYTE)(code | RLV_V36_OUT_BOTTOM);
    }
    return code;
}

/*
 * Clip a diagonal segment to soft rect (soft_only path). Returns FALSE if
 * nothing remains. Hardware-clipped paths must not use this — InstallClipRegion
 * plus Draw() already trims straddling strokes.
 */
static BOOL rlv_v36_clip_diag_to_soft(RLV_BackendV36 *b,
                                      WORD *x1, WORD *y1,
                                      WORD *x2, WORD *y2)
{
    UBYTE c1;
    UBYTE c2;
    const struct Rectangle *r;
    LONG x;
    LONG y;
    LONG dx;
    LONG dy;

    if (b == 0 || x1 == 0 || y1 == 0 || x2 == 0 || y2 == 0) {
        return FALSE;
    }
    r = &b->clip.soft;

    for (;;) {
        c1 = rlv_v36_outcode(*x1, *y1, r);
        c2 = rlv_v36_outcode(*x2, *y2, r);
        if ((c1 | c2) == 0) {
            return TRUE;
        }
        if ((c1 & c2) != 0) {
            return FALSE;
        }

        dx = (LONG)(*x2) - (LONG)(*x1);
        dy = (LONG)(*y2) - (LONG)(*y1);
        if (c1 != 0) {
            if ((c1 & RLV_V36_OUT_LEFT) != 0) {
                y = (LONG)(*y1) + (dy * ((LONG)r->MinX - (LONG)(*x1))) / dx;
                x = (LONG)r->MinX;
            } else if ((c1 & RLV_V36_OUT_RIGHT) != 0) {
                y = (LONG)(*y1) + (dy * ((LONG)r->MaxX - (LONG)(*x1))) / dx;
                x = (LONG)r->MaxX;
            } else if ((c1 & RLV_V36_OUT_TOP) != 0) {
                x = (LONG)(*x1) + (dx * ((LONG)r->MinY - (LONG)(*y1))) / dy;
                y = (LONG)r->MinY;
            } else {
                x = (LONG)(*x1) + (dx * ((LONG)r->MaxY - (LONG)(*y1))) / dy;
                y = (LONG)r->MaxY;
            }
            *x1 = (WORD)x;
            *y1 = (WORD)y;
        } else {
            if ((c2 & RLV_V36_OUT_LEFT) != 0) {
                y = (LONG)(*y1) + (dy * ((LONG)r->MinX - (LONG)(*x1))) / dx;
                x = (LONG)r->MinX;
            } else if ((c2 & RLV_V36_OUT_RIGHT) != 0) {
                y = (LONG)(*y1) + (dy * ((LONG)r->MaxX - (LONG)(*x1))) / dx;
                x = (LONG)r->MaxX;
            } else if ((c2 & RLV_V36_OUT_TOP) != 0) {
                x = (LONG)(*x1) + (dx * ((LONG)r->MinY - (LONG)(*y1))) / dy;
                y = (LONG)r->MinY;
            } else {
                x = (LONG)(*x1) + (dx * ((LONG)r->MaxY - (LONG)(*y1))) / dy;
                y = (LONG)r->MaxY;
            }
            *x2 = (WORD)x;
            *y2 = (WORD)y;
        }
    }
}

static VOID rlv_v36_draw_line(APTR ctx, WORD x1, WORD y1, WORD x2, WORD y2)
{
    RLV_BackendV36 *b = (RLV_BackendV36 *)ctx;
    struct RastPort *rp = rlv_v36_rp(b);
    WORD xa;
    WORD ya;
    WORD xb;
    WORD yb;

    if (rp == 0) {
        return;
    }

    /* Soft-clip axis-aligned lines (bevels/dividers); skip if empty. */
    if (b != 0 && b->clip.soft_active) {
        xa = x1;
        ya = y1;
        xb = x2;
        yb = y2;
        if (xa > xb) {
            WORD t = xa;
            xa = xb;
            xb = t;
        }
        if (ya > yb) {
            WORD t = ya;
            ya = yb;
            yb = t;
        }
        if (!rlv_v36_soft_intersect(b, &xa, &ya, &xb, &yb)) {
            return;
        }
        /* Restore orientation for horizontal/vertical segments. */
        if (y1 == y2) {
            x1 = xa;
            x2 = xb;
            y1 = ya;
            y2 = ya;
        } else if (x1 == x2) {
            x1 = xa;
            x2 = xa;
            y1 = ya;
            y2 = yb;
        } else if (b->clip.soft_only) {
            /*
             * soft_only (no InstallClipRegion): clip the segment. The old
             * all-or-nothing endpoint reject erased checkbox ticks under
             * smart-scroll / refresh bands — fill_rect soft-intersects and
             * wipes the in-band interior, then both diagonal strokes were
             * skipped if either endpoint sat one pixel outside the band.
             */
            if (!rlv_v36_clip_diag_to_soft(b, &x1, &y1, &x2, &y2)) {
                return;
            }
        }
        /*
         * soft + hardware: leave endpoints alone. InstallClipRegion trims
         * Draw(); rejecting straddling diagonals here left half-ticks after
         * regional smart-scroll paints.
         */
    }

    if (x1 == x2) {
        RLV_BENCH_COUNT(RLV_BENCH_COUNTER_VERTICAL_LINES);
    } else if (y1 == y2) {
        RLV_BENCH_COUNT(RLV_BENCH_COUNTER_HORIZONTAL_LINES);
    }
    SetDrMd(rp, JAM1);
    Move(rp, x1, y1);
    Draw(rp, x2, y2);
}

static VOID rlv_v36_draw_dotted_hline(APTR ctx, WORD x1, WORD x2, WORD y)
{
    RLV_BackendV36 *b;
    struct RastPort *rp;
    UWORD old_line_pattern;
    UBYTE old_pattern_count;
    WORD y2;
    WORD swap;

    b = (RLV_BackendV36 *)ctx;
    rp = rlv_v36_rp(b);
    if (rp == 0) {
        return;
    }
    if (x2 < x1) {
        swap = x1;
        x1 = x2;
        x2 = swap;
    }
    y2 = y;
    if (!rlv_v36_soft_intersect(b, &x1, &y, &x2, &y2)) {
        return;
    }

    old_line_pattern = rp->LinePtrn;
    old_pattern_count = rp->linpatcnt;
    SetDrMd(rp, JAM1);
    SetDrPt(rp, 0xAAAAU);
    Move(rp, x1, y);
    Draw(rp, x2, y);
    SetDrPt(rp, old_line_pattern);
    rp->linpatcnt = old_pattern_count;
    RLV_BENCH_COUNT(RLV_BENCH_COUNTER_HORIZONTAL_LINES);
}

/*
 * Reversible vertical guide for column-resize preview. COMPLEMENT toggles
 * all bitplanes; drawing the same segment again restores prior pixels.
 * Saves and restores DrawMode and APen.
 */
static VOID rlv_v36_draw_xor_vline(APTR ctx, WORD x, WORD y1, WORD y2)
{
    RLV_BackendV36 *b;
    struct RastPort *rp;
    UBYTE old_mode;
    UBYTE old_fg;
    WORD swap;

    b = (RLV_BackendV36 *)ctx;
    rp = rlv_v36_rp(b);
    if (rp == 0) {
        return;
    }
    if (y2 < y1) {
        swap = y1;
        y1 = y2;
        y2 = swap;
    }
    if (!rlv_v36_soft_intersect(b, &x, &y1, &x, &y2)) {
        return;
    }

    old_mode = rp->DrawMode;
    old_fg = rp->FgPen;
    SetAPen(rp, ~0);
    SetDrMd(rp, COMPLEMENT);
    Move(rp, x, y1);
    Draw(rp, x, y2);
    SetAPen(rp, old_fg);
    SetDrMd(rp, old_mode);
    RLV_BENCH_COUNT(RLV_BENCH_COUNTER_VERTICAL_LINES);
}

static VOID rlv_v36_draw_text(APTR ctx, WORD x, WORD baseline,
                              CONST_STRPTR text, UWORD length)
{
    RLV_BackendV36 *b = (RLV_BackendV36 *)ctx;
    struct RastPort *rp = rlv_v36_rp(b);
    struct TextFont *font;
    WORD glyph_top;
    WORD glyph_bottom;

    if (rp == 0 || text == 0 || length == 0) {
        return;
    }

    /*
     * Software clip for text. Text() is all-or-nothing (including JAM2
     * cell backgrounds), so partial glyphs that straddle the soft top/bottom
     * would otherwise paint into damaged header pixels during LAYERUPDATING.
     *
     * soft_only (refresh): reject any glyph that extends outside the soft
     * rect — no hardware clip is present to trim the overhang.
     * soft + hardware: reject only glyphs wholly outside; InstallClipRegion
     * trims edge lines so partially visible rows still draw.
     */
    if (b != 0 && b->clip.soft_active) {
        font = b->font;
        if (font == 0 && rp->Font != 0) {
            font = rp->Font;
        }
        if (font != 0) {
            glyph_top = (WORD)(baseline - (WORD)font->tf_Baseline);
            glyph_bottom = (WORD)(glyph_top + (WORD)font->tf_YSize - 1);
        } else {
            glyph_top = baseline;
            glyph_bottom = baseline;
        }
        if (x > b->clip.soft.MaxX) {
            return;
        }
        if (b->clip.soft_only) {
            if (glyph_top < b->clip.soft.MinY
                || glyph_bottom > b->clip.soft.MaxY) {
                return;
            }
        } else if (glyph_bottom < b->clip.soft.MinY
                   || glyph_top > b->clip.soft.MaxY) {
            return;
        }
    }

    RLV_BENCH_COUNT(RLV_BENCH_COUNTER_CELLS_DRAWN);
    Move(rp, x, baseline);
    Text(rp, (STRPTR)text, length);
}

static UWORD rlv_v36_text_width(APTR ctx, CONST_STRPTR text, UWORD length)
{
    struct RastPort *rp = rlv_v36_rp((RLV_BackendV36 *)ctx);

    if (rp == 0 || text == 0 || length == 0) {
        return 0;
    }
    RLV_BENCH_COUNT(RLV_BENCH_COUNTER_TEXT_LENGTH_CALLS);
    return (UWORD)TextLength(rp, (STRPTR)text, length);
}

static UWORD rlv_v36_text_fit(APTR ctx, CONST_STRPTR text, UWORD length,
                              UWORD max_width)
{
    struct RastPort *rp = rlv_v36_rp((RLV_BackendV36 *)ctx);
    struct TextExtent extent;

    if (rp == 0 || text == 0 || length == 0 || max_width == 0) {
        return 0;
    }
    RLV_BENCH_COUNT(RLV_BENCH_COUNTER_TEXT_FIT_CALLS);
    memset(&extent, 0, sizeof(extent));
    return (UWORD)TextFit(rp, (STRPTR)text, (ULONG)length, &extent,
                          NULL, 1, (ULONG)max_width, 32767UL);
}

static BOOL rlv_v36_push_clip(APTR ctx, const struct Rectangle *rect)
{
    RLV_BackendV36 *b = (RLV_BackendV36 *)ctx;
    struct RastPort *rp;
    struct Region *new_region;
    struct Region *old_region;
    struct Region *install_result;

    RLV_LOG("v36 push_clip begin");
    if (b == 0 || rect == 0) {
        RLV_LOG("INVARIANT v36 push_clip null backend/rect");
        RLV_LOG("v36 push_clip result=0");
        return FALSE;
    }

    /* Nested push not supported in Phase 2; pop any prior clip first. */
    rlv_v36_pop_clip(b);
    memset(&b->clip, 0, sizeof(b->clip));

    rp = b->rp;
    if (rp == 0) {
        RLV_LOG("INVARIANT v36 push_clip null RastPort");
        RLV_LOG("v36 push_clip result=0");
        return FALSE;
    }
    if (rect->MaxX < rect->MinX || rect->MaxY < rect->MinY) {
        RLV_LOGF("INVARIANT v36 push_clip invalid rect %d,%d-%d,%d",
                 (int)rect->MinX, (int)rect->MinY,
                 (int)rect->MaxX, (int)rect->MaxY);
        RLV_LOG("v36 push_clip result=0");
        return FALSE;
    }

    if (rp->Layer == 0) {
        /* No layer: still activate soft_only so draws stay in rect. */
        b->clip.no_layer = TRUE;
        rlv_v36_activate_soft_only(b, rect);
        RLV_LOG("v36 push_clip no_layer soft_only fallback result=1");
        return TRUE;
    }

    /*
     * During BeginRefresh/BeginUpdate (LAYERUPDATING), do not call
     * InstallClipRegion at all — it fights the damage ClipRects and
     * recreates blank uncover bands. Use software clipping instead so
     * scrolled text cannot paint into damaged pixels outside the viewport.
     */
#ifdef LAYERUPDATING
    if ((rp->Layer->Flags & LAYERUPDATING) != 0)
#else
    if ((rp->Layer->Flags & 0x10U) != 0) /* LAYERUPDATING */
#endif
    {
        rlv_v36_activate_soft_only(b, rect);
        RLV_LOG("v36 push_clip result=1 soft (LAYERUPDATING+viewport)");
        return TRUE;
    }

    new_region = NewRegion();
    if (new_region == 0) {
        /* Soft-only: never report success with clip inactive (Phase 5 audit). */
        rlv_v36_activate_soft_only(b, rect);
        RLV_LOG("v36 push_clip NewRegion failed soft_only fallback result=1");
        return TRUE;
    }

    if (!OrRectRegion(new_region, (struct Rectangle *)rect)) {
        DisposeRegion(new_region);
        rlv_v36_activate_soft_only(b, rect);
        RLV_LOG("v36 push_clip OrRectRegion failed soft_only fallback result=1");
        return TRUE;
    }

    old_region = InstallClipRegion(rp->Layer, NULL);
    if (old_region != 0) {
        if (!AndRegionRegion(old_region, new_region)) {
            InstallClipRegion(rp->Layer, old_region);
            DisposeRegion(new_region);
            rlv_v36_activate_soft_only(b, rect);
            RLV_LOG("v36 push_clip AndRegionRegion failed soft_only fallback result=1");
            return TRUE;
        }
    }

    install_result = InstallClipRegion(rp->Layer, new_region);
    if (install_result == new_region) {
        InstallClipRegion(rp->Layer, old_region);
        DisposeRegion(new_region);
        rlv_v36_activate_soft_only(b, rect);
        RLV_LOG("INVARIANT v36 InstallClipRegion rejected new region");
        RLV_LOG("v36 push_clip result=1 soft_only fallback");
        return TRUE;
    }

    b->clip.layer = rp->Layer;
    b->clip.old_region = old_region;
    b->clip.new_region = new_region;
    b->clip.soft = *rect;
    b->clip.soft_active = TRUE; /* belt-and-suspenders with hardware clip */
    b->clip.soft_only = FALSE;
    b->clip.active = TRUE;
    RLV_LOG("v36 push_clip result=1 active");
    return TRUE;
}

static VOID rlv_v36_pop_clip(APTR ctx)
{
    RLV_BackendV36 *b = (RLV_BackendV36 *)ctx;

    RLV_LOG("v36 pop_clip begin");
    if (b == 0 || !b->clip.active) {
        RLV_LOG("v36 pop_clip end (inactive)");
        return;
    }

    if (b->clip.new_region != 0 && b->clip.layer != 0) {
        InstallClipRegion(b->clip.layer, b->clip.old_region);
        DisposeRegion(b->clip.new_region);
    }

    b->clip.active = FALSE;
    b->clip.soft_active = FALSE;
    b->clip.soft_only = FALSE;
    b->clip.new_region = 0;
    b->clip.old_region = 0;
    b->clip.layer = 0;
    RLV_LOG("v36 pop_clip end");
}

#if defined(RLV_ENABLE_SMART_SCROLL) && (RLV_ENABLE_SMART_SCROLL != 0)

/*
 * Shift viewport pixels only. Does not paint.
 *
 * vertical_delta = new_scroll_y - old_scroll_y (content space).
 * Positive delta → ScrollRaster dy > 0 → existing bits move toward Y=0 (up).
 *
 * Uses ScrollRasterBF + LAYERS_NOBACKFILL on graphics V39+; ScrollRaster
 * on V36/V37 so Workbench 2.x remains supported. Caller paints the exposed
 * band or falls back to a full viewport repaint.
 */
static UWORD rlv_v36_move_viewport_pixels(APTR ctx,
                                          const struct Rectangle *viewport,
                                          WORD vertical_delta)
{
    RLV_BackendV36 *b = (RLV_BackendV36 *)ctx;
    struct RastPort *rp;
    struct Layer *layer;
    struct Layer_Info *li;
    struct Hook *prior_hook;
    ULONG flags_before;
    ULONG flags_after;
    BOOL locked;
    BOOL hook_changed;
    WORD abs_delta;
    UWORD gfx_ver;

    RLV_LOG("SMART_SCROLL backend begin");
    RLV_BENCH_COUNT(RLV_BENCH_COUNTER_SCROLL_COPY_ATTEMPTS);

    if (b == 0 || viewport == 0) {
        RLV_LOG("SMART_SCROLL rejected reason=null_ctx_or_viewport");
        return (UWORD)RLV_VIEWPORT_MOVE_REPAINT;
    }
    if (vertical_delta == 0) {
        RLV_LOG("SMART_SCROLL rejected reason=delta_zero");
        return (UWORD)RLV_VIEWPORT_MOVE_UNUSED;
    }
    if (viewport->MaxX < viewport->MinX || viewport->MaxY < viewport->MinY) {
        RLV_LOG("SMART_SCROLL rejected reason=invalid_viewport");
        return (UWORD)RLV_VIEWPORT_MOVE_REPAINT;
    }

    abs_delta = vertical_delta;
    if (abs_delta < 0) {
        abs_delta = (WORD)(-abs_delta);
    }
    if (abs_delta >= (WORD)(viewport->MaxY - viewport->MinY + 1)) {
        RLV_LOG("SMART_SCROLL rejected reason=delta_ge_viewport");
        return (UWORD)RLV_VIEWPORT_MOVE_REPAINT;
    }

    if (b->clip.active) {
        RLV_LOG("SMART_SCROLL rejected reason=clip_active");
        return (UWORD)RLV_VIEWPORT_MOVE_REPAINT;
    }

    rp = b->rp;
    if (rp == 0) {
        RLV_LOG("SMART_SCROLL rejected reason=null_rastport");
        return (UWORD)RLV_VIEWPORT_MOVE_REPAINT;
    }
    layer = rp->Layer;
    if (layer == 0) {
        RLV_LOG("SMART_SCROLL rejected reason=null_layer");
        return (UWORD)RLV_VIEWPORT_MOVE_REPAINT;
    }

    flags_before = layer->Flags;
    if ((flags_before & LAYERUPDATING) != 0) {
        RLV_LOG("SMART_SCROLL rejected reason=layerupdating");
        return (UWORD)RLV_VIEWPORT_MOVE_REPAINT;
    }
    if ((flags_before & LAYERREFRESH) != 0) {
        RLV_LOG("SMART_SCROLL rejected reason=layer_damaged");
        return (UWORD)RLV_VIEWPORT_MOVE_REPAINT;
    }
    RLV_LOGF("SMART_SCROLL damage before=%lu", (unsigned long)flags_before);

    li = layer->LayerInfo;
    locked = FALSE;
    hook_changed = FALSE;
    prior_hook = 0;
    gfx_ver = 0;
    if (GfxBase != 0) {
        gfx_ver = GfxBase->LibNode.lib_Version;
    }

    if (li != 0) {
        RLV_LOG("SMART_SCROLL layer lock");
        LockLayerInfo(li);
        locked = TRUE;
    }

    if (gfx_ver >= 39) {
        RLV_LOG("SMART_SCROLL clip suspended (nobackfill hook)");
        prior_hook = InstallLayerHook(layer, LAYERS_NOBACKFILL);
        hook_changed = TRUE;
        ScrollRasterBF(rp, 0, vertical_delta,
                       viewport->MinX, viewport->MinY,
                       viewport->MaxX, viewport->MaxY);
    } else {
        /* V36/V37: ScrollRasterBF / LAYERS_NOBACKFILL unavailable. */
        ScrollRaster(rp, 0, vertical_delta,
                     viewport->MinX, viewport->MinY,
                     viewport->MaxX, viewport->MaxY);
    }

    flags_after = layer->Flags;
    RLV_LOGF("SMART_SCROLL damage after=%lu", (unsigned long)flags_after);
    RLV_LOG("SMART_SCROLL pixel move complete");

    if (hook_changed) {
        InstallLayerHook(layer, prior_hook);
        RLV_LOG("SMART_SCROLL clip restored (hook)");
    }
    if (locked) {
        UnlockLayerInfo(li);
        RLV_LOG("SMART_SCROLL layer unlock");
    }

    if ((flags_after & LAYERUPDATING) != 0) {
        RLV_LOG("SMART_SCROLL fallback full viewport (layerupdating)");
        return (UWORD)RLV_VIEWPORT_MOVE_REPAINT;
    }

    /*
     * V39+ ScrollRasterBF + NOBACKFILL: vacated strip is not damage.
     * LAYERREFRESH after the move means real overlap/obscure damage —
     * refuse and let the caller full-repaint / refresh path handle it.
     *
     * V36/V37 ScrollRaster: vacated strip is added to the damage list.
     * That is expected; return DONE so the caller paints the exposed band,
     * then finish_viewport_move acknowledges the damage.
     */
    if (gfx_ver >= 39 && (flags_after & LAYERREFRESH) != 0) {
        RLV_LOG("SMART_SCROLL fallback full viewport (damage)");
        return (UWORD)RLV_VIEWPORT_MOVE_REPAINT;
    }

    RLV_LOG("SMART_SCROLL backend end done");
    RLV_BENCH_COUNT(RLV_BENCH_COUNTER_SCROLL_COPY_SUCCESSES);
    return (UWORD)RLV_VIEWPORT_MOVE_DONE;
}

static VOID rlv_v36_finish_viewport_move(APTR ctx)
{
    RLV_BackendV36 *b = (RLV_BackendV36 *)ctx;
    struct RastPort *rp;
    struct Layer *layer;

    if (b == 0) {
        return;
    }
    rp = b->rp;
    if (rp == 0 || rp->Layer == 0) {
        return;
    }
    layer = rp->Layer;
    if ((layer->Flags & LAYERREFRESH) == 0) {
        return;
    }
    if ((layer->Flags & LAYERUPDATING) != 0) {
        return;
    }

    /* Acknowledge simple-refresh damage after the exposed band was painted. */
    if (BeginUpdate(layer)) {
        EndUpdate(layer, TRUE);
        RLV_LOG("SMART_SCROLL layer damage acknowledged");
    }
}

#endif /* RLV_ENABLE_SMART_SCROLL */

static UWORD rlv_v36_line_height(APTR ctx)
{
    RLV_BackendV36 *b = (RLV_BackendV36 *)ctx;
    struct RastPort *rp;
    struct TextFont *font;

    if (b == 0) {
        return 8;
    }
    font = b->font;
    rp = b->rp;
    if (font != 0) {
        return (UWORD)font->tf_YSize;
    }
    if (rp != 0 && rp->Font != 0) {
        return (UWORD)rp->Font->tf_YSize;
    }
    return 8;
}

static UWORD rlv_v36_baseline(APTR ctx)
{
    RLV_BackendV36 *b = (RLV_BackendV36 *)ctx;
    struct RastPort *rp;
    struct TextFont *font;

    if (b == 0) {
        return 6;
    }
    font = b->font;
    rp = b->rp;
    if (font != 0) {
        return (UWORD)font->tf_Baseline;
    }
    if (rp != 0 && rp->Font != 0) {
        return (UWORD)rp->Font->tf_Baseline;
    }
    return 6;
}
