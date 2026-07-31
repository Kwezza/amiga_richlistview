/**
 * Compile-only audit: rich_listview.h must stand alone for application code.
 * Does not link a runnable program — object build is sufficient.
 */

#include "rich_listview/rich_listview.h"

#include <string.h>

int main(void)
{
    RLV_Control *control;
    RLV_Config cfg;
    RLV_Column col;
    RLV_Row row;
    RLV_Cell cell;
    RLV_InputEvent in;
    RLV_Event ev;
    RLV_Pens pens;
    UWORD align;
    UWORD wrap;
    UWORD event_type;
    UWORD input_type;

    control = 0;
    memset(&cfg, 0, sizeof(cfg));
    memset(&col, 0, sizeof(col));
    memset(&row, 0, sizeof(row));
    memset(&cell, 0, sizeof(cell));
    memset(&in, 0, sizeof(in));
    memset(&ev, 0, sizeof(ev));
    memset(&pens, 0, sizeof(pens));
    align = (UWORD)RLV_CELL_ALIGN_LEFT;
    wrap = (UWORD)RLV_WRAP_NONE;
    event_type = (UWORD)RLV_EVENT_NONE;
    input_type = (UWORD)RLV_INPUT_SELECT_DOWN;

    /* Touch new policy / repaint enums so the public header remains usable. */
    {
        UWORD policy;
        UWORD visual;
        UWORD repaint;

        policy = (UWORD)RLV_CONTROL_ACTIVATE_SELECT_ROW;
        visual = (UWORD)RLV_CURRENT_ROW_VISUAL_FULL;
        repaint = (UWORD)RLV_CELL_REPAINT_OK;
        if (policy != 0 || visual != 0 || repaint != 0) {
            return 1;
        }
        policy = (UWORD)RLV_CONTROL_ACTIVATE_KEEP_CURRENT;
        visual = (UWORD)RLV_CURRENT_ROW_VISUAL_MARKER;
        if (policy == 0 || visual == 0) {
            return 1;
        }
        visual = (UWORD)RLV_CURRENT_ROW_VISUAL_NONE;
        if (visual == 0) {
            return 1;
        }
    }

    if (control != 0 || cfg.flags != 0 || col.width_pixels != 0 ||
        row.flags != 0 || cell.flags != 0 || in.type != 0 ||
        ev.type != 0 || pens.text != 0 ||
        align != 0 || wrap != 0 || event_type != 0 || input_type != 0) {
        return 1;
    }
    return 0;
}
