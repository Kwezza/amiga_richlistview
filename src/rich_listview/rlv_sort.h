#ifndef RLV_SORT_H
#define RLV_SORT_H

/**
 * Optional column sorting for RichListview (RLV_ENABLE_SORTING).
 *
 * Include after rich_listview.h, or rely on declarations in the umbrella
 * header when sorting is compiled in. Non-sorting builds provide API stubs
 * that return FALSE / identity indices.
 *
 * Sorting applies only to the currently attached row array. It does not
 * globally sort a paged master dataset. Borrowed RLV_Row memory is never
 * reordered; a control-owned view-order map provides display order.
 */

#include "rich_listview/rich_listview.h"

#endif /* RLV_SORT_H */
