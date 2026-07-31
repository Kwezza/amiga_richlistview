/**
 * Scroll accessors — Phase 4 binds these to the scroller gadget.
 */

#include "rich_listview/rlv_internal.h"
#include "rich_listview/rlv_log.h"

LONG rlv_get_scroll_y(const RLV_Control *c)
{
    if (c == 0) {
        return 0;
    }
    return c->scroll_y;
}

VOID rlv_set_scroll_y(RLV_Control *c, LONG scroll_y)
{
    LONG max_scroll;
    LONG vp_h;
    LONG requested;

    if (c == 0) {
        RLV_LOG("INVARIANT set_scroll_y control is NULL");
        return;
    }

    requested = scroll_y;
    if (scroll_y < 0) {
        scroll_y = 0;
    }

    vp_h = (LONG)c->viewport_bounds.MaxY - (LONG)c->viewport_bounds.MinY + 1;
    if (vp_h < 0) {
        vp_h = 0;
    }
    max_scroll = c->content_height - vp_h;
    if (max_scroll < 0) {
        max_scroll = 0;
    }
    if (scroll_y > max_scroll) {
        scroll_y = max_scroll;
    }

    if (requested < 0 || requested > max_scroll) {
        RLV_LOGF("INVARIANT scroll_y request=%ld clamped to %ld (max=%ld)",
                 (long)requested, (long)scroll_y, (long)max_scroll);
    }

    c->scroll_y = scroll_y;
}

LONG rlv_get_content_height(const RLV_Control *c)
{
    if (c == 0) {
        return 0;
    }
    return c->content_height;
}

LONG rlv_get_viewport_height(const RLV_Control *c)
{
    LONG vp_h;

    if (c == 0) {
        return 0;
    }
    vp_h = (LONG)c->viewport_bounds.MaxY - (LONG)c->viewport_bounds.MinY + 1;
    if (vp_h < 0) {
        vp_h = 0;
    }
    return vp_h;
}
