/**
 * Compile-only audit: public core + Amiga V36 backend headers together.
 * Does not require private headers or implementation source paths.
 */

#include "rich_listview/rich_listview.h"
#include "rich_listview/backends/rlv_backend_amiga_v36.h"

int main(void)
{
    RLV_Control *control;
    RLV_BackendV36 *backend;
    const RLV_DrawOps *ops;

    control = 0;
    backend = 0;
    ops = rlv_backend_v36_get_ops();

    if (control != 0 || backend != 0 || ops == 0) {
        return 1;
    }
    return 0;
}
