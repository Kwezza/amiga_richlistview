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
        UWORD rowdisp;
        UWORD longword;
        UWORD ellip;

        policy = (UWORD)RLV_CONTROL_ACTIVATE_SELECT_ROW;
        visual = (UWORD)RLV_CURRENT_ROW_VISUAL_FULL;
        repaint = (UWORD)RLV_CELL_REPAINT_OK;
        rowdisp = (UWORD)RLV_ROWS_COLLAPSIBLE;
        longword = (UWORD)RLV_LONG_WORD_CLIP;
        ellip = (UWORD)RLV_ELLIPSIS_NONE;
        /* Opaque row tag field must remain public without private headers. */
        row.user_data = (APTR)0;
        ev.row_user_data = row.user_data;
        if (policy != 0 || visual != 0 || repaint != 0
            || rowdisp != 0 || longword != 0 || ellip != 0
            || row.user_data != 0 || ev.row_user_data != 0) {
            return 1;
        }
        policy = (UWORD)RLV_CONTROL_ACTIVATE_KEEP_CURRENT;
        visual = (UWORD)RLV_CURRENT_ROW_VISUAL_MARKER;
        rowdisp = (UWORD)RLV_ROWS_ALWAYS_EXPANDED;
        longword = (UWORD)RLV_LONG_WORD_WRAP;
        ellip = (UWORD)(RLV_ELLIPSIS_COLLAPSED_CONTENT
                        | RLV_ELLIPSIS_HORIZONTAL_CLIP);
        if (policy == 0 || visual == 0 || rowdisp == 0
            || longword == 0 || ellip == 0) {
            return 1;
        }
        visual = (UWORD)RLV_CURRENT_ROW_VISUAL_NONE;
        rowdisp = (UWORD)RLV_ROWS_SINGLE_LINE;
        if (visual == 0 || rowdisp == 0) {
            return 1;
        }
        {
            UWORD initexp;
            RLV_Config cfg2;
            UWORD sortkind;
            UWORD sortdir;

            initexp = (UWORD)RLV_INITIAL_EXPAND_ALL_OPEN;
            if (initexp != 0) {
                return 1;
            }
            initexp = (UWORD)RLV_INITIAL_EXPAND_ALL_COLLAPSED;
            if (initexp == 0) {
                return 1;
            }
            memset(&cfg2, 0, sizeof(cfg2));
            if (cfg2.initial_expand != 0) {
                return 1;
            }
            sortkind = (UWORD)RLV_SORT_TEXT_NOCASE;
            sortdir = (UWORD)RLV_SORT_ASC;
            if (sortkind == 0 || sortdir != 0) {
                return 1;
            }
            event_type = (UWORD)RLV_EVENT_SORT_CHANGED;
            if (event_type == 0) {
                return 1;
            }
            event_type = (UWORD)RLV_EVENT_COLUMN_RESIZED;
            if (event_type == 0) {
                return 1;
            }
            {
                UWORD no_resize;

                no_resize = (UWORD)RLV_COL_F_NO_RESIZE;
                if (no_resize == 0) {
                    return 1;
                }
                input_type = (UWORD)RLV_INPUT_CANCEL;
                if (input_type == 0) {
                    return 1;
                }
                ev.resize_left = 0;
                ev.resize_right = 1;
                ev.old_left_width = 10;
                ev.old_right_width = 20;
                ev.new_left_width = 12;
                ev.new_right_width = 18;
                if (ev.old_left_width + ev.old_right_width
                    != ev.new_left_width + ev.new_right_width) {
                    return 1;
                }
            }
            event_type = (UWORD)RLV_EVENT_NONE;
            input_type = (UWORD)RLV_INPUT_SELECT_DOWN;
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
