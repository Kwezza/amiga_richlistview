#!/usr/bin/env python3
"""Generate LISTVIEW_AUDIT_MACHINE_READABLE.csv — data for the architecture audit."""
from __future__ import annotations

import csv
from pathlib import Path

OUT = Path(__file__).with_name("LISTVIEW_AUDIT_MACHINE_READABLE.csv")
COLS = [
    "Path",
    "Classification",
    "Confidence",
    "SecondaryFlags",
    "BuiltBy",
    "KeyEvidence",
    "Dependencies",
    "ProposedExtractionAction",
    "Notes",
]

# Each row: path, class, conf, flags, built_by, evidence, deps, action, notes
ROWS: list[tuple[str, ...]] = []


def r(
    path: str,
    classification: str,
    confidence: str,
    flags: str,
    built_by: str,
    evidence: str,
    deps: str,
    action: str,
    notes: str = "",
) -> None:
    ROWS.append(
        (
            path,
            classification,
            confidence,
            flags,
            built_by,
            evidence,
            deps,
            action,
            notes,
        )
    )


# ---------------------------------------------------------------------------
# GENERIC / SHARED foundations
# ---------------------------------------------------------------------------
r(
    "src/custom_listview/clv_platform.h",
    "GENERIC_REUSABLE",
    "High",
    "PUBLIC_API;COPY_CANDIDATE;RICHLISTVIEW_DEPENDENCY",
    "all profiles; custom-control-demo",
    "CLV_PLATFORM_AMIGA assert; included by control via clv_control_platform.h",
    "none",
    "DUPLICATE_SMALL_GENERIC_MODULE",
    "Thin; control depends on it",
)
r(
    "src/custom_listview/clv_platform.c",
    "GENERIC_REUSABLE",
    "High",
    "PRIVATE_INTERNAL;COPY_CANDIDATE;RICHLISTVIEW_DEPENDENCY",
    "CLV_PLATFORM_OBJS all profiles+control",
    "clv_platform_malloc/free/strdup",
    "Amiga AllocMem",
    "DUPLICATE_SMALL_GENERIC_MODULE",
    "Only legacy .o linked into RichListview today",
)
r(
    "src/custom_listview/clv_platform_internal.h",
    "GENERIC_REUSABLE",
    "High",
    "PRIVATE_INTERNAL;COPY_CANDIDATE;RICHLISTVIEW_DEPENDENCY",
    "via clv_platform.o",
    "declares allocation shim; included by control .c files",
    "clv_platform.h",
    "DUPLICATE_SMALL_GENERIC_MODULE",
)
r(
    "src/custom_listview/clv_compiler.h",
    "GENERIC_REUSABLE",
    "High",
    "PUBLIC_API;COPY_CANDIDATE",
    "header-only",
    "CLV_COMPILER_* detection",
    "none",
    "DUPLICATE_SMALL_GENERIC_MODULE",
)
r(
    "src/custom_listview/clv_sdk_compat.h",
    "GENERIC_REUSABLE",
    "High",
    "PUBLIC_API;COPY_CANDIDATE",
    "header-only",
    "NewList and SDK fallbacks",
    "Amiga SDK",
    "DUPLICATE_SMALL_GENERIC_MODULE",
    "ASCII/list code needs it; control may not",
)
r(
    "src/custom_listview/clv_exec_list_compat.h",
    "GENERIC_REUSABLE",
    "High",
    "PUBLIC_API;COPY_CANDIDATE",
    "header-only",
    "thin NewList wrapper",
    "clv_sdk_compat.h",
    "DUPLICATE_SMALL_GENERIC_MODULE",
)
r(
    "src/custom_listview/clv_types.h",
    "GENERIC_REUSABLE",
    "Medium",
    "PUBLIC_API;SPLIT_REQUIRED;COPY_CANDIDATE;RICHLISTVIEW_DEPENDENCY;MISLEADING_NAME",
    "header; used by both arches",
    "CLV_CellAlign; CLV_PixelColumn; CLV_LISTVIEW_SCROLLBAR_BORDER; comments cite lvdm_Bounds",
    "clv_platform.h",
    "SPLIT_FILE_BEFORE_OR_DURING_EXTRACTION",
    "Medium: GadTools-oriented comments/constant shared into control; unclear if SCROLLBAR_BORDER belongs in Rich types",
)
r(
    "src/custom_listview/clv_config.h",
    "LEGACY_GADTOOLS",
    "High",
    "PUBLIC_API;LEAVE_BEHIND",
    "profile clients via generated CLV_HAS_*",
    "CLV_HAS_* defaults for v1 feature families",
    "none",
    "LEAVE_IN_LEGACY_REPOSITORY",
    "RichListview needs its own feature macros",
)
r(
    "src/custom_listview/clv_log.h",
    "GENERIC_REUSABLE",
    "Medium",
    "PRIVATE_INTERNAL;MISLEADING_NAME",
    "header-only macros in legacy .c",
    "clv_log_info/error no-op macros; name collides with control clv_log_*",
    "none",
    "REWRITE_FOR_RICHLISTVIEW",
    "Medium: symbol-family clash with clv_control_log.h functions",
)
r(
    "src/custom_listview/clv_bench.c",
    "GENERIC_REUSABLE",
    "High",
    "PRIVATE_INTERNAL;COPY_CANDIDATE;RICHLISTVIEW_DEPENDENCY",
    "custom-control-demo-bench",
    "timing harness used by control bench build",
    "clv_bench_internal.h",
    "COPY_TO_RICHLISTVIEW_THEN_CLEAN",
)
r(
    "src/custom_listview/clv_bench_internal.h",
    "GENERIC_REUSABLE",
    "High",
    "PRIVATE_INTERNAL;COPY_CANDIDATE;RICHLISTVIEW_DEPENDENCY",
    "control bench + backend include",
    "bench macros; included by clv_control_internal.h and v36 backend",
    "none",
    "COPY_TO_RICHLISTVIEW_THEN_CLEAN",
)

# ---------------------------------------------------------------------------
# ASCII / path / sort / tracking (legacy GadTools enhancement)
# ---------------------------------------------------------------------------
r(
    "src/custom_listview/custom_listview.h",
    "LEGACY_GADTOOLS",
    "High",
    "PUBLIC_API;LEAVE_BEHIND",
    "umbrella include",
    "includes v1 public headers only",
    "v1 public headers",
    "LEAVE_IN_LEGACY_REPOSITORY",
)
r(
    "src/custom_listview/PUBLIC_HEADERS.txt",
    "BUILD_OR_TOOLING",
    "High",
    "LEAVE_BEHIND",
    "header allowlist tooling",
    "lists v1 public basenames including clv_cellctl.h",
    "none",
    "LEAVE_IN_LEGACY_REPOSITORY",
)
r(
    "src/custom_listview/clv_ascii.h",
    "LEGACY_GADTOOLS",
    "High",
    "PUBLIC_API;SPLIT_REQUIRED;LEAVE_BEHIND",
    "ascii-* and draw-* (columns bridge)",
    "ASCII format API + pixel-column bridge for drawn prepare; lvdm_Bounds comments",
    "clv_types.h",
    "LEAVE_IN_LEGACY_REPOSITORY",
    "Bridge half is drawn-GadTools specific",
)
r(
    "src/custom_listview/clv_ascii_columns.c",
    "LEGACY_GADTOOLS",
    "High",
    "SPLIT_REQUIRED;LEAVE_BEHIND",
    "CLV_ASCII_COLUMNS_OBJS ascii+draw profiles",
    "format_header/row + clv_ascii_columns_calc_pixel_columns",
    "clv_log.h; path_core; types",
    "LEAVE_IN_LEGACY_REPOSITORY",
)
r(
    "src/custom_listview/clv_ascii_internal.h",
    "LEGACY_GADTOOLS",
    "High",
    "PRIVATE_INTERNAL;LEAVE_BEHIND",
    "ascii_columns/prepare internals",
    "content-viewport bridge internals",
    "ascii",
    "LEAVE_IN_LEGACY_REPOSITORY",
)
r(
    "src/custom_listview/clv_ascii_formatter.h",
    "LEGACY_GADTOOLS",
    "High",
    "PUBLIC_API;LEAVE_BEHIND",
    "ascii profiles",
    "justified struct List for GTLV_Labels; detach docs",
    "exec lists",
    "LEAVE_IN_LEGACY_REPOSITORY",
)
r(
    "src/custom_listview/clv_ascii_formatter.c",
    "LEGACY_GADTOOLS",
    "High",
    "LEAVE_BEHIND",
    "CLV_ASCII_FORMATTER_OBJS",
    "clv_ascii_create/free_justified_list",
    "platform; log; exec list",
    "LEAVE_IN_LEGACY_REPOSITORY",
)
r(
    "src/custom_listview/clv_columns.h",
    "LEGACY_GADTOOLS",
    "High",
    "PUBLIC_API;LEAVE_BEHIND",
    "ascii-sorted/wrapped/full",
    "computed ASCII column format API",
    "none",
    "LEAVE_IN_LEGACY_REPOSITORY",
    "Algorithmically generic text format but product is ASCII GTLV labels",
)
r(
    "src/custom_listview/clv_columns.c",
    "LEGACY_GADTOOLS",
    "High",
    "LEAVE_BEHIND",
    "CLV_COLUMNS_OBJS",
    "compute widths + format",
    "platform",
    "LEAVE_IN_LEGACY_REPOSITORY",
)
r(
    "src/custom_listview/clv_sort.h",
    "GENERIC_REUSABLE",
    "Medium",
    "PUBLIC_API;COPY_CANDIDATE",
    "ascii-sorted; full-smoke",
    "sort state over source cells; no GadTools calls",
    "none",
    "DUPLICATE_SMALL_GENERIC_MODULE",
    "Medium: unused by control today; may be useful later",
)
r(
    "src/custom_listview/clv_sort.c",
    "GENERIC_REUSABLE",
    "Medium",
    "COPY_CANDIDATE",
    "CLV_SORT_OBJS",
    "qsort-based text/numeric order",
    "platform",
    "DUPLICATE_SMALL_GENERIC_MODULE",
)
r(
    "src/custom_listview/clv_cell_tracking.h",
    "LEGACY_GADTOOLS",
    "High",
    "PUBLIC_API;LEAVE_BEHIND",
    "ascii-tracked; full",
    "column click hit-test for ListView X",
    "none",
    "LEAVE_IN_LEGACY_REPOSITORY",
    "Control has own hit_test",
)
r(
    "src/custom_listview/clv_cell_tracking.c",
    "LEGACY_GADTOOLS",
    "High",
    "LEAVE_BEHIND",
    "CLV_CELL_TRACKING_OBJS",
    "clv_cell_tracking_detect_*",
    "ascii geometry",
    "LEAVE_IN_LEGACY_REPOSITORY",
)
r(
    "src/custom_listview/clv_path.h",
    "GENERIC_REUSABLE",
    "High",
    "PUBLIC_API;COPY_CANDIDATE",
    "ascii-sorted/wrapped/full",
    "path truncate/shorten public API",
    "none",
    "DUPLICATE_SMALL_GENERIC_MODULE",
    "Not linked by control; control wrap has PATH mode via draw_ops",
)
r(
    "src/custom_listview/clv_path.c",
    "GENERIC_REUSABLE",
    "High",
    "COPY_CANDIDATE",
    "CLV_PATH_WRAPPER_OBJS",
    "public wrappers",
    "path_core",
    "DUPLICATE_SMALL_GENERIC_MODULE",
)
r(
    "src/custom_listview/clv_path_core.c",
    "GENERIC_REUSABLE",
    "High",
    "COPY_CANDIDATE",
    "CLV_PATH_CORE_OBJS ascii+draw",
    "canonical path shorten core",
    "platform",
    "DUPLICATE_SMALL_GENERIC_MODULE",
)
r(
    "src/custom_listview/clv_path_internal.h",
    "GENERIC_REUSABLE",
    "High",
    "PRIVATE_INTERNAL;COPY_CANDIDATE",
    "path modules",
    "internal path-core API",
    "none",
    "DUPLICATE_SMALL_GENERIC_MODULE",
)
r(
    "src/custom_listview/clv_char_wrap.h",
    "LEGACY_GADTOOLS",
    "High",
    "PUBLIC_API;LEAVE_BEHIND",
    "ascii-wrapped; full",
    "char wrap + display maps; GTLV_Labels ownership docs",
    "none",
    "LEAVE_IN_LEGACY_REPOSITORY",
)
r(
    "src/custom_listview/clv_char_wrap.c",
    "LEGACY_GADTOOLS",
    "High",
    "LEAVE_BEHIND",
    "CLV_CHAR_WRAP_OBJS",
    "builds wrapped physical Node lists",
    "ascii; path; platform; log",
    "LEAVE_IN_LEGACY_REPOSITORY",
)

# ---------------------------------------------------------------------------
# Drawn GadTools renderer stack
# ---------------------------------------------------------------------------
for p, ev, action in [
    (
        "src/custom_listview/clv_renderer.h",
        "PUBLIC_API: clv_renderer_create/get_hook; prepared lists; GTLV_CallBack docs",
        "LEAVE_IN_LEGACY_REPOSITORY",
    ),
    (
        "src/custom_listview/clv_renderer_core.c",
        "HookEntry; clv_renderer_dispatch(LVDrawMsg); LV_DRAW; prepared build",
        "LEAVE_IN_LEGACY_REPOSITORY",
    ),
    (
        "src/custom_listview/clv_renderer_columns.c",
        "divider reserve; line styles; Phase-2 decor",
        "LEAVE_IN_LEGACY_REPOSITORY",
    ),
    (
        "src/custom_listview/clv_renderer_ops.c",
        "g_clv_opt_fns ops table; single-fragment path",
        "LEAVE_IN_LEGACY_REPOSITORY",
    ),
    (
        "src/custom_listview/clv_renderer_internal.h",
        "private renderer types; bind install decls; includes gadtools",
        "LEAVE_IN_LEGACY_REPOSITORY",
    ),
    (
        "src/custom_listview/clv_prepared_display_map.c",
        "geometric growth for physical display maps (GadTools multi-node wrap)",
        "LEAVE_IN_LEGACY_REPOSITORY",
    ),
    (
        "src/custom_listview/clv_prepared_internal.h",
        "prepared-list map helpers",
        "LEAVE_IN_LEGACY_REPOSITORY",
    ),
]:
    flags = "PUBLIC_API;LEAVE_BEHIND" if p.endswith(".h") and "internal" not in p and "prepared_internal" not in p else "PRIVATE_INTERNAL;LEAVE_BEHIND"
    if p.endswith("clv_renderer.h"):
        flags = "PUBLIC_API;LEAVE_BEHIND"
    r(
        p,
        "LEGACY_GADTOOLS",
        "High",
        flags,
        "CLV_RENDERER_CORE_OBJS draw-*+full",
        ev,
        "gadtools LVDrawMsg; platform; ascii_columns; ops/bind",
        action,
    )

r(
    "src/custom_listview/clv_renderer_setup.h",
    "LEGACY_GADTOOLS",
    "Medium",
    "PRIVATE_INTERNAL;COPY_CANDIDATE",
    "renderer_core; host tests",
    "viewport/continuation-guide geometry helpers; host-testable",
    "types",
    "REWRITE_FOR_RICHLISTVIEW",
    "Medium: algorithmically useful but APIs assume drawn-GadTools presentation",
)
r(
    "src/custom_listview/clv_renderer_setup.c",
    "LEGACY_GADTOOLS",
    "Medium",
    "PRIVATE_INTERNAL;COPY_CANDIDATE",
    "CLV_RENDERER_CORE_OBJS",
    "pure cell-presentation / viewport geometry",
    "renderer_setup.h",
    "REWRITE_FOR_RICHLISTVIEW",
    "Control has own layout; do not copy blindly",
)

r(
    "src/custom_listview/clv_selection.h",
    "LEGACY_GADTOOLS",
    "High",
    "PUBLIC_API;LEAVE_BEHIND",
    "draw-selected+; cellctl; wrapped; styled; details; full",
    "clv_handle_selection; GTLV_Selected restore docs",
    "prepared maps",
    "LEAVE_IN_LEGACY_REPOSITORY",
)
r(
    "src/custom_listview/clv_selection.c",
    "LEGACY_GADTOOLS",
    "High",
    "LEAVE_BEHIND",
    "CLV_SELECTION_OBJS",
    "GT_SetGadgetAttrs(GTLV_Selected); reject special rows",
    "gadtools; prepared",
    "LEAVE_IN_LEGACY_REPOSITORY",
    "Control selection is logical-row state — different semantics",
)
r(
    "src/custom_listview/clv_pixel_wrap.c",
    "LEGACY_GADTOOLS",
    "High",
    "LEAVE_BEHIND;DUPLICATE_CANDIDATE",
    "CLV_PIXEL_WRAP_OBJS + bind_wrapped/details/full",
    "pixel wrap install into ops; algorithm source for control_wrap copy",
    "renderer_internal; platform",
    "LEAVE_IN_LEGACY_REPOSITORY",
    "Rich already forked algorithm in clv_control_wrap.c",
)
r(
    "src/custom_listview/clv_icons.c",
    "LEGACY_GADTOOLS",
    "High",
    "LEAVE_BEHIND",
    "CLV_ICON_OBJS",
    "clv_icons_install; status/sort indicators in LV_DRAW",
    "renderer ops",
    "REWRITE_FOR_RICHLISTVIEW",
)
r(
    "src/custom_listview/clv_styles.c",
    "LEGACY_GADTOOLS",
    "High",
    "LEAVE_BEHIND",
    "CLV_STYLE_OBJS",
    "clv_styles_install soft styles",
    "renderer ops",
    "REWRITE_FOR_RICHLISTVIEW",
)
r(
    "src/custom_listview/clv_details.h",
    "LEGACY_GADTOOLS",
    "High",
    "PUBLIC_API;LEAVE_BEHIND",
    "draw-details; full",
    "details builder + Hook* view API",
    "renderer",
    "LEAVE_IN_LEGACY_REPOSITORY",
)
r(
    "src/custom_listview/clv_details.c",
    "LEGACY_GADTOOLS",
    "High",
    "LEAVE_BEHIND",
    "CLV_DETAILS_OBJS",
    "semantic details builder",
    "details_prepare; renderer",
    "REWRITE_FOR_RICHLISTVIEW",
)
r(
    "src/custom_listview/clv_details_prepare.c",
    "LEGACY_GADTOOLS",
    "High",
    "LEAVE_BEHIND",
    "CLV_DETAILS_OBJS",
    "prepared-list construction for details",
    "renderer prepare; ops",
    "LEAVE_IN_LEGACY_REPOSITORY",
)

for bind, installs, prof in [
    ("clv_bind_none.c", "empty bind", "draw-basic; draw-selected"),
    ("clv_bind_wrapped.c", "pixel_wrap_install", "draw-wrapped"),
    ("clv_bind_styled.c", "icons+styles install", "draw-styled"),
    ("clv_bind_details.c", "wrap+icons+styles", "draw-details"),
    ("clv_bind_full.c", "wrap+icons+styles", "full-smoke"),
    ("clv_bind_cellctl.c", "cellctl_install", "draw-cellctl-checkbox"),
]:
    r(
        f"src/custom_listview/{bind}",
        "LEGACY_GADTOOLS",
        "High",
        "PRIVATE_INTERNAL;LEAVE_BEHIND",
        prof,
        f"clv_renderer_bind_optional installs: {installs}",
        "renderer_ops; optional modules",
        "LEAVE_IN_LEGACY_REPOSITORY",
        "Binder-only; easy to miss as unused",
    )

# Cellctl — legacy GadTools interactive cells
r(
    "src/custom_listview/clv_cellctl.h",
    "LEGACY_GADTOOLS",
    "High",
    "PUBLIC_API;MISLEADING_NAME;LEAVE_BEHIND",
    "draw-cellctl-checkbox",
    "GadTools control-cell API; parallel to clv_control checkbox",
    "renderer; selection",
    "LEAVE_IN_LEGACY_REPOSITORY",
    "Name suggests product cells; clv_control.h says non-authoritative",
)
r(
    "src/custom_listview/clv_cellctl_core.c",
    "LEGACY_GADTOOLS",
    "High",
    "LEAVE_BEHIND",
    "CLV_CELLCTL_OBJS",
    "mouse hit; GTLV geometry queries",
    "gadtools; renderer; cellctl_geom",
    "LEAVE_IN_LEGACY_REPOSITORY",
)
r(
    "src/custom_listview/clv_cellctl_checkbox.c",
    "LEGACY_GADTOOLS",
    "High",
    "LEAVE_BEHIND;MISLEADING_NAME",
    "CLV_CELLCTL_OBJS",
    "checkbox paint in LV_DRAW hook",
    "cellctl_internal; renderer",
    "LEAVE_IN_LEGACY_REPOSITORY",
)
r(
    "src/custom_listview/clv_cellctl_geom.c",
    "GENERIC_REUSABLE",
    "Medium",
    "COPY_CANDIDATE;DUPLICATE_CANDIDATE",
    "CLV_CELLCTL_OBJS; host cellctl_geom tests",
    "pure checkbox geometry/state; host-tested",
    "clv_cellctl_geom.h",
    "DUPLICATE_SMALL_GENERIC_MODULE",
    "Medium: control uses own clv_control_checkbox.c — duplication already exists",
)
r(
    "src/custom_listview/clv_cellctl_geom.h",
    "GENERIC_REUSABLE",
    "Medium",
    "PUBLIC_API;COPY_CANDIDATE",
    "cellctl + host tests",
    "geom API",
    "none",
    "DUPLICATE_SMALL_GENERIC_MODULE",
)
r(
    "src/custom_listview/clv_cellctl_internal.h",
    "LEGACY_GADTOOLS",
    "High",
    "PRIVATE_INTERNAL;LEAVE_BEHIND",
    "cellctl modules",
    "install/paint/hit internals",
    "renderer_internal",
    "LEAVE_IN_LEGACY_REPOSITORY",
)

# ---------------------------------------------------------------------------
# RICHLISTVIEW control package
# ---------------------------------------------------------------------------
CTRL_BUILT = "custom-control-demo; -log; -bench; -nosmart"

r(
    "src/custom_listview_control/clv_control.h",
    "RICHLISTVIEW",
    "High",
    "PUBLIC_API;MOVE_CANDIDATE;LEGACY_DEPENDENCY",
    CTRL_BUILT,
    "clv_control_create/destroy/render/handle_input; not LISTVIEW_KIND",
    "clv_control_draw.h; custom_listview/clv_types.h",
    "COPY_TO_RICHLISTVIEW_THEN_CLEAN",
    "Includes legacy clv_types.h",
)
r(
    "src/custom_listview_control/clv_control_draw.h",
    "RICHLISTVIEW",
    "High",
    "PUBLIC_API;MOVE_CANDIDATE",
    CTRL_BUILT,
    "CLV_DrawOps; CLV_Pens; no clv_renderer.h",
    "none architecture-specific",
    "MOVE_TO_RICHLISTVIEW",
)
r(
    "src/custom_listview_control/clv_control_platform.h",
    "RICHLISTVIEW",
    "High",
    "PUBLIC_API;MOVE_CANDIDATE;LEGACY_DEPENDENCY",
    CTRL_BUILT,
    "includes custom_listview/clv_platform.h",
    "clv_platform.h",
    "COPY_TO_RICHLISTVIEW_THEN_CLEAN",
)
r(
    "src/custom_listview_control/clv_control_internal.h",
    "RICHLISTVIEW",
    "High",
    "PRIVATE_INTERNAL;MOVE_CANDIDATE;LEGACY_DEPENDENCY",
    CTRL_BUILT,
    "struct CLV_Control; includes clv_bench_internal.h",
    "platform; bench; draw_ops",
    "COPY_TO_RICHLISTVIEW_THEN_CLEAN",
)
r(
    "src/custom_listview_control/clv_control.c",
    "RICHLISTVIEW",
    "High",
    "MOVE_CANDIDATE;LEGACY_DEPENDENCY",
    CTRL_BUILT,
    "create/destroy/setters/render entry",
    "clv_platform_malloc/free; layout; scroll",
    "COPY_TO_RICHLISTVIEW_THEN_CLEAN",
)
r(
    "src/custom_listview_control/clv_control_layout.c",
    "RICHLISTVIEW",
    "High",
    "MOVE_CANDIDATE;LEGACY_DEPENDENCY",
    CTRL_BUILT,
    "layout_rebuild; column geom; row heights",
    "platform; wrap_prepare; CLV_PixelColumn",
    "COPY_TO_RICHLISTVIEW_THEN_CLEAN",
)
r(
    "src/custom_listview_control/clv_control_wrap.c",
    "RICHLISTVIEW",
    "High",
    "MOVE_CANDIDATE;LEGACY_DEPENDENCY;DUPLICATE_CANDIDATE",
    CTRL_BUILT,
    "pixel wrap prepare; adapted from clv_pixel_wrap without linking",
    "platform; draw_ops text_width/fit",
    "COPY_TO_RICHLISTVIEW_THEN_CLEAN",
)
r(
    "src/custom_listview_control/clv_control_render.c",
    "RICHLISTVIEW",
    "High",
    "MOVE_CANDIDATE",
    CTRL_BUILT,
    "full/viewport/partial/smart-scroll paint; no LVDrawMsg",
    "draw_ops; checkbox paint",
    "MOVE_TO_RICHLISTVIEW",
)
r(
    "src/custom_listview_control/clv_control_checkbox.c",
    "RICHLISTVIEW",
    "High",
    "MOVE_CANDIDATE",
    CTRL_BUILT,
    "checkbox resolve/paint for control",
    "draw_ops; cell snapshot",
    "MOVE_TO_RICHLISTVIEW",
)
r(
    "src/custom_listview_control/clv_control_input.c",
    "RICHLISTVIEW",
    "High",
    "MOVE_CANDIDATE",
    CTRL_BUILT,
    "hit_test; selection; keyboard NAV; CELL_CONTROL events",
    "checkbox; scroll; layout",
    "MOVE_TO_RICHLISTVIEW",
)
r(
    "src/custom_listview_control/clv_control_scroll.c",
    "RICHLISTVIEW",
    "High",
    "MOVE_CANDIDATE",
    CTRL_BUILT,
    "scroll_y clamp; content/viewport height",
    "CLV_Control only",
    "MOVE_TO_RICHLISTVIEW",
)
r(
    "src/custom_listview_control/clv_control_log.h",
    "RICHLISTVIEW",
    "High",
    "PRIVATE_INTERNAL;MOVE_CANDIDATE;MISLEADING_NAME",
    "custom-control-demo-log",
    "clv_log_init/write macros; collides with legacy clv_log.h names",
    "none",
    "COPY_TO_RICHLISTVIEW_THEN_CLEAN",
    "Rename later to clv_ctrl_log_*",
)
r(
    "src/custom_listview_control/clv_control_log.c",
    "RICHLISTVIEW",
    "High",
    "PRIVATE_INTERNAL;MOVE_CANDIDATE",
    "custom-control-demo-log only",
    "PROGDIR logger implementation",
    "DOS",
    "MOVE_TO_RICHLISTVIEW",
)
r(
    "src/custom_listview_control/backends/clv_backend_amiga_v36.h",
    "RICHLISTVIEW",
    "High",
    "PUBLIC_API;MOVE_CANDIDATE",
    CTRL_BUILT,
    "clv_backend_v36_*; No LISTVIEW_KIND comment",
    "CLV_DrawOps",
    "MOVE_TO_RICHLISTVIEW",
)
r(
    "src/custom_listview_control/backends/clv_backend_amiga_v36.c",
    "RICHLISTVIEW",
    "High",
    "MOVE_CANDIDATE;LEGACY_DEPENDENCY",
    CTRL_BUILT,
    "implements DrawOps; ScrollRaster smart scroll",
    "clv_platform_*; clv_bench_internal.h; graphics",
    "COPY_TO_RICHLISTVIEW_THEN_CLEAN",
)

# ---------------------------------------------------------------------------
# Examples
# ---------------------------------------------------------------------------
LEGACY_EXAMPLES = [
    ("00_extraction_smoke_test", "full-smoke", "smoke of native modules"),
    ("01_ascii_minimal", "ascii-minimal", "CreateGadget LISTVIEW_KIND; justified labels"),
    ("02_ascii_tracked", "ascii-tracked", "cell tracking"),
    ("03_ascii_sorted", "ascii-sorted", "sort"),
    ("04_ascii_wrapped", "ascii-wrapped", "char wrap"),
    ("05_draw_basic", "draw-basic", "GTLV_CallBack + prepared labels"),
    ("05_draw_cellctl_checkbox", "draw-cellctl-checkbox", "legacy GadTools cellctl"),
    ("06_draw_wrapped", "draw-wrapped", "pixel wrap + selection"),
    ("07_draw_icons_styles", "draw-styled", "icons/styles"),
    ("08_draw_details", "draw-details", "details builder"),
]
for name, target, ev in LEGACY_EXAMPLES:
    for suffix, kind in [("main.c", "DEMO_OR_TEST"), ("README.md", "DOCUMENTATION")]:
        r(
            f"examples/{name}/{suffix}",
            kind,
            "High",
            "LEAVE_BEHIND" if name != "05_draw_cellctl_checkbox" else "LEAVE_BEHIND;MISLEADING_NAME",
            target,
            ev,
            "legacy CLV modules",
            "LEAVE_IN_LEGACY_REPOSITORY",
        )

r(
    "examples/custom_control_demo/main.c",
    "DEMO_OR_TEST",
    "High",
    "MOVE_CANDIDATE;COPY_CANDIDATE",
    "custom-control-demo*",
    "IDCMP→CLV_InputEvent; SCROLLER_KIND companion; paints via clv_control_render",
    "clv_control_*; backend_v36; platform",
    "COPY_TO_RICHLISTVIEW_THEN_CLEAN",
    "Working reference app — copy first",
)
r(
    "examples/custom_control_demo/README.md",
    "DOCUMENTATION",
    "High",
    "MOVE_CANDIDATE;COPY_CANDIDATE",
    "docs for control demo",
    "documents no LISTVIEW_KIND; control architecture",
    "none",
    "COPY_TO_RICHLISTVIEW_THEN_CLEAN",
)

for stub in [
    "examples/ascii_sort_stub.c",
    "examples/ascii_wrap_stub.c",
    "examples/draw_details_stub.c",
]:
    r(
        stub,
        "DEMO_OR_TEST",
        "High",
        "LEAVE_BEHIND;DEAD_OR_UNREFERENCED",
        "historical/stub",
        "stub example remnants",
        "legacy",
        "ARCHIVE_OR_REMOVE_LATER",
        "Verify Makefile references before removal",
    )

SIZE_FILES = [
    "size_shell.c",
    "size_shell.h",
    "size_ascii_minimal.c",
    "size_ascii_sorted.c",
    "size_ascii_tracked.c",
    "size_ascii_wrapped.c",
    "size_draw_basic.c",
    "size_draw_cellctl_checkbox.c",
    "size_draw_details.c",
    "size_draw_selected.c",
    "size_draw_styled.c",
    "size_draw_wrapped.c",
    "size_full.c",
    "README.md",
]
for f in SIZE_FILES:
    r(
        f"examples/size_compare/{f}",
        "DEMO_OR_TEST" if f.endswith((".c", ".h")) else "DOCUMENTATION",
        "High",
        "LEAVE_BEHIND",
        "make size-* / sizes",
        "like-for-like size harness for v1 profiles",
        "legacy profile libs",
        "LEAVE_IN_LEGACY_REPOSITORY",
    )

# ---------------------------------------------------------------------------
# Build / tools / templates / root
# ---------------------------------------------------------------------------
r(
    "Makefile",
    "BUILD_OR_TOOLING",
    "High",
    "SPLIT_REQUIRED;MISLEADING_NAME",
    "all Amiga targets",
    "hosts both CLV_* profile libs and CLV_CUSTOM_CONTROL_* targets",
    "all src",
    "SPLIT_FILE_BEFORE_OR_DURING_EXTRACTION",
    "Wildcard risk low (explicit object lists); but one Makefile for both arches",
)
r(
    "templates/Makefile",
    "BUILD_OR_TOOLING",
    "High",
    "LEAVE_BEHIND",
    "templates",
    "template window builds",
    "templates",
    "LEAVE_IN_LEGACY_REPOSITORY",
)
r(
    "tests/host/Makefile",
    "BUILD_OR_TOOLING",
    "High",
    "LEAVE_BEHIND",
    "host GCC tests",
    "links real src/*.c with stubs — v1 modules",
    "src/custom_listview",
    "LEAVE_IN_LEGACY_REPOSITORY",
)
r(
    "tests/header_audit/Makefile",
    "BUILD_OR_TOOLING",
    "High",
    "LEAVE_BEHIND",
    "header audit",
    "v1 public header compile matrix",
    "src/custom_listview",
    "LEAVE_IN_LEGACY_REPOSITORY",
)

TOOLS = [
    "tools/generate_profile_config.ps1",
    "tools/header_audit_inventory.py",
    "tools/size_report.ps1",
    "tools/validate_abi.ps1",
    "tools/validate_benchmark_report.ps1",
    "tools/validate_cellctl_omission.ps1",
    "tools/validate_layout.ps1",
    "tools/validate_no_host_branches.ps1",
    "tools/validate_no_wb3c_compat.ps1",
    "tools/validate_platform.ps1",
    "tools/validate_profile_config.ps1",
    "tools/write_header_audit_docs.py",
]
for t in TOOLS:
    r(
        t,
        "BUILD_OR_TOOLING",
        "High",
        "LEAVE_BEHIND",
        "validate-* / sizes / header audit",
        "v1 profile/ABI/size validation",
        "Makefile profiles",
        "LEAVE_IN_LEGACY_REPOSITORY",
    )

# Templates — GadTools window lifecycle
TMPL = [
    "templates/AI_AGENT_GETTING_STARTED.md",
    "templates/AI_AGENT_GUIDE.md",
    "templates/AI_AGENT_LAYOUT_GUIDE.md",
    "templates/AI_AGENT_QUICK_REFERENCE.md",
    "templates/amiga_window_template.c",
    "templates/amiga_window_template.h",
    "templates/amiga_window_menus_template.c",
    "templates/amiga_window_resize_template.c",
    "templates/amiga_window_resize_template.old.c",
    "templates/PROFESSIONAL_WINDOW_LAYOUT_TEMPLATE.c",
    "templates/test_window_template.c",
    "templates/COMPILATION_SUCCESS.md",
    "templates/CONSOLE_OUTPUT_FIX.md",
    "templates/GUI_Styling.md",
    "templates/LAYOUT_SYSTEM_OVERVIEW.md",
    "templates/PLACETEXT_ABOVE_FIX.md",
    "templates/QUICK_REFERENCE.md",
    "templates/README_RESIZE_TEMPLATE.md",
    "templates/README_TEMPLATE.md",
    "templates/WINDOW_BOTTOM_BORDER_FIX.md",
    "templates/amiga_gui_research_3x.md",
    "templates/iTidy_Advanced_Settings_Layout_Guide.md",
    "templates/Clearing Window Contents Before Gadget Re-Add in GadTools.txt",
    "templates/Resizable GadTools Forms on Amiga Workbench 3.0.txt",
]
for t in TMPL:
    kind = "DEMO_OR_TEST" if t.endswith((".c", ".h")) else "DOCUMENTATION"
    r(
        t,
        kind,
        "High",
        "LEAVE_BEHIND",
        "templates / agent guidance",
        "GadTools window/gadget lifecycle templates",
        "intuition/gadtools",
        "LEAVE_IN_LEGACY_REPOSITORY",
        "Still useful for apps embedding either LV; not RichListview control code",
    )

# Root meta
r(
    "README.md",
    "DOCUMENTATION",
    "High",
    "STALE_DOCUMENTATION;MISLEADING_NAME;SPLIT_REQUIRED",
    "repo front page",
    "Describes only GadTools LISTVIEW_KIND enhancement; omits custom_control_demo",
    "none",
    "REQUIRES_MANUAL_DECISION",
    "Should eventually document both arches or split repos",
)
r(
    "AGENTS.md",
    "DOCUMENTATION",
    "High",
    "STALE_DOCUMENTATION;MISLEADING_NAME",
    "agent instructions",
    "Primary guidance is GadTools ListView enhancer; barely mentions full custom control",
    "none",
    "REQUIRES_MANUAL_DECISION",
)
r(
    ".github/copilot-instructions.md",
    "DOCUMENTATION",
    "High",
    "STALE_DOCUMENTATION",
    "Copilot",
    "mirrors AGENTS legacy focus",
    "none",
    "REQUIRES_MANUAL_DECISION",
)
r(
    "DISPLAY_MAP_CAPACITY_REFACTOR_REPORT.md",
    "DOCUMENTATION",
    "High",
    "LEAVE_BEHIND;STALE_DOCUMENTATION",
    "historical",
    "v1 display map capacity",
    "none",
    "ARCHIVE_OR_REMOVE_LATER",
)

# ---------------------------------------------------------------------------
# Tests (summary rows for groups + key files)
# ---------------------------------------------------------------------------
# Host tests — mostly legacy
HOST_LEGACY = [
    "tests/host/main.c",
    "tests/host/ascii_format_regression.c",
    "tests/host/clv_host_test_helpers.c",
    "tests/host/clv_host_test_helpers.h",
    "tests/host/clv_platform_test.c",
    "tests/host/continuation_guide_tests.c",
    "tests/host/ownership_main.c",
    "tests/host/ownership_tests.c",
    "tests/host/path_shortening_characterise.c",
    "tests/host/path_shortening_harness.c",
    "tests/host/path_shortening_harness.h",
    "tests/host/path_shortening_main.c",
    "tests/host/path_shortening_tests.c",
    "tests/host/prepared_display_map_main.c",
    "tests/host/prepared_display_map_tests.c",
    "tests/host/renderer_setup_characterise.c",
    "tests/host/renderer_setup_harness.c",
    "tests/host/renderer_setup_harness.h",
    "tests/host/renderer_setup_main.c",
    "tests/host/renderer_setup_tests.c",
    "tests/host/renderer_viewport_characterise.c",
    "tests/host/renderer_viewport_harness.c",
    "tests/host/renderer_viewport_harness.h",
    "tests/host/renderer_viewport_main.c",
    "tests/host/renderer_viewport_tests.c",
    "tests/host/cellctl_geom_main.c",
    "tests/host/cellctl_geom_tests.c",
    "tests/host/stubs/exec/list_stubs.c",
    "tests/host/stubs/exec/lists.h",
    "tests/host/stubs/exec/nodes.h",
    "tests/host/stubs/exec/types.h",
    "tests/host/stubs/graphics/text.h",
    "tests/host/stubs/proto/exec.h",
    "tests/host/README.md",
    "tests/host/PATH_SHORTENING_TESTS.md",
    "tests/host/RENDERER_SETUP_TESTS.md",
    "tests/host/output/PATH_SHORTENING_CHARACTERISATION.md",
    "tests/host/output/RENDERER_SETUP_CHARACTERISATION.md",
    "tests/host/output/RENDERER_VIEWPORT_CLIPPING_CHARACTERISATION.md",
]
for p in HOST_LEGACY:
    kind = "DOCUMENTATION" if p.endswith(".md") else "DEMO_OR_TEST"
    r(
        p,
        kind,
        "High",
        "LEAVE_BEHIND",
        "tests/host make test",
        "host GCC regression of v1 modules (not control)",
        "src/custom_listview",
        "LEAVE_IN_LEGACY_REPOSITORY",
    )

# Amiga wrap selftests
for p in [
    "tests/clv_char_wrap_amiga_selftest.c",
    "tests/clv_char_wrap_amiga_selftest.h",
    "tests/clv_selftest_common.h",
    "tests/wrap_selftest_main.c",
    "tests/abi/clv_abi_probe.c",
    "tests/abi/clv_abi_baseline.json",
]:
    r(
        p,
        "DEMO_OR_TEST",
        "High",
        "LEAVE_BEHIND",
        "make tests / validate-abi",
        "v1 wrap/ABI tests",
        "legacy modules",
        "LEAVE_IN_LEGACY_REPOSITORY",
    )

# Header audit — enumerate via pattern note; list compile/client/neg files from inventory
HEADER_AUDIT = [
    "tests/header_audit/RESULTS.md",
    "tests/header_audit/client_ascii_api.c",
    "tests/header_audit/client_char_wrap_api.c",
    "tests/header_audit/client_details_api.c",
    "tests/header_audit/client_renderer_api.c",
    "tests/header_audit/compile_clv_ascii.c",
    "tests/header_audit/compile_clv_ascii_formatter.c",
    "tests/header_audit/compile_clv_cell_tracking.c",
    "tests/header_audit/compile_clv_char_wrap.c",
    "tests/header_audit/compile_clv_columns.c",
    "tests/header_audit/compile_clv_compiler.c",
    "tests/header_audit/compile_clv_config.c",
    "tests/header_audit/compile_clv_details.c",
    "tests/header_audit/compile_clv_exec_list_compat.c",
    "tests/header_audit/compile_clv_path.c",
    "tests/header_audit/compile_clv_platform.c",
    "tests/header_audit/compile_clv_renderer.c",
    "tests/header_audit/compile_clv_sdk_compat.c",
    "tests/header_audit/compile_clv_selection.c",
    "tests/header_audit/compile_clv_sort.c",
    "tests/header_audit/compile_clv_types.c",
    "tests/header_audit/compile_umbrella.c",
    "tests/header_audit/include_order_a.c",
    "tests/header_audit/include_order_b.c",
    "tests/header_audit/include_order_renderer_selection.c",
    "tests/header_audit/include_order_selection_renderer.c",
    "tests/header_audit/internal_renderer_types.c",
    "tests/header_audit/internal_viewport_api.c",
    "tests/header_audit/neg_public_divider.c",
    "tests/header_audit/neg_public_platform_free.c",
    "tests/header_audit/neg_public_platform_malloc.c",
    "tests/header_audit/neg_public_render_cell.c",
    "tests/header_audit/neg_public_render_fragment.c",
    "tests/header_audit/neg_public_render_max_internal.c",
    "tests/header_audit/neg_public_render_node.c",
    "tests/header_audit/neg_public_row_node_tags.c",
    "tests/header_audit/neg_public_viewport_ascii.c",
    "tests/header_audit/neg_public_viewport_renderer_clear.c",
    "tests/header_audit/neg_public_viewport_renderer_set.c",
]
for p in HEADER_AUDIT:
    kind = "DOCUMENTATION" if p.endswith(".md") else "DEMO_OR_TEST"
    r(
        p,
        kind,
        "High",
        "LEAVE_BEHIND",
        "header_audit Makefile",
        "v1 public/internal header surface gates",
        "src/custom_listview",
        "LEAVE_IN_LEGACY_REPOSITORY",
    )

HEADERS_HDR = [
    "tests/headers/hdr_ascii.c",
    "tests/headers/hdr_cellctl.c",
    "tests/headers/hdr_char_wrap.c",
    "tests/headers/hdr_config.c",
    "tests/headers/hdr_details.c",
    "tests/headers/hdr_exec_compat.c",
    "tests/headers/hdr_renderer.c",
    "tests/headers/hdr_selection.c",
    "tests/headers/hdr_types.c",
    "tests/headers/hdr_umbrella.c",
]
for p in HEADERS_HDR:
    r(
        p,
        "DEMO_OR_TEST",
        "High",
        "LEAVE_BEHIND",
        "validate-headers",
        "v1 header compile probes",
        "src/custom_listview",
        "LEAVE_IN_LEGACY_REPOSITORY",
    )

# ---------------------------------------------------------------------------
# Documentation — project docs (not AutoDocs)
# ---------------------------------------------------------------------------
DOC_RICH = [
    ("docs/CLV_CUSTOM_CONTROL_DESIGN_AND_IMPLEMENTATION_PLAN.md", "RichListview design/implementation plan"),
    ("docs/CLV_CUSTOM_CONTROL_KEYBOARD_NAVIGATION_PLAN.md", "control keyboard NAV plan"),
    ("docs/CLV_CUSTOM_CONTROL_PHASE1_AUDIT.md", "Phase1 custom control audit"),
    ("docs/CLV_CELL_CONTROL_EVENT_NOTIFICATION_PLAN.md", "control cell events plan"),
    ("docs/CLV_CONTROL_CELLS_DEVELOPER_LOG.md", "developer log for control cells"),
    ("docs/CLV_INTERACTIVE_CONTROL_CELLS_MASTER_PLAN.md", "master plan; may mention both paths"),
    ("docs/CLV_BENCHMARK_HANDOFF.md", "control bench handoff"),
    ("docs/CLV_BENCHMARK_IMPLEMENTATION_REPORT.md", "control bench implementation"),
    ("docs/AI_AGENT_PROMPT_AUDIT_AND_CLASSIFY_LISTVIEW_FILES.md", "this audit prompt"),
    ("docs/CLV_FUTURE_IMPROVEMENTS_WISHLIST.md", "wishlist spanning future work"),
]
for p, ev in DOC_RICH:
    flags = "MOVE_CANDIDATE;COPY_CANDIDATE"
    if "MASTER_PLAN" in p or "WISHLIST" in p or "INTERACTIVE" in p:
        flags += ";SPLIT_REQUIRED;STALE_DOCUMENTATION"
    r(
        p,
        "DOCUMENTATION",
        "High" if "MASTER" not in p else "Medium",
        flags,
        "docs only",
        ev,
        "none",
        "COPY_TO_RICHLISTVIEW_THEN_CLEAN"
        if "PROMPT" not in p
        else "LEAVE_IN_LEGACY_REPOSITORY",
        "Medium confidence on MASTER_PLAN: spans both cellctl and control",
    )

DOC_LEGACY = [
    "docs/CUSTOM_LISTVIEW_INTEGRATION_GUIDE.md",
    "docs/CUSTOM_LISTVIEW_INTEGRATION_GUIDE_REPORT.md",
    "docs/CLV_BUILD_PROFILES.md",
    "docs/CLV_SIZE_REPORT.md",
    "docs/CLV_MEMORY_MODEL.md",
    "docs/CLV_MODULE_ARCHITECTURE.md",
    "docs/CLV_MODULE_AUDIT.md",
    "docs/WB3C_TO_CLV_MIGRATION.md",
    "docs/LISTVIEW_HELPERS.md",
    "docs/LISTVIEW_DETAILS_HELPER.md",
    "docs/WRAPPED_CONTINUATION_GUIDE_REPORT.md",
    "docs/WRAPPED_CONTINUATION_GUIDE_REFINEMENT_REPORT.md",
    "docs/CLV_68000_OPTIMIZATION_ANALYSIS.md",
    "docs/DevLog.md",
]
for p in DOC_LEGACY:
    flags = "LEAVE_BEHIND"
    if p.endswith("CLV_MODULE_ARCHITECTURE.md"):
        flags = "LEAVE_BEHIND;STALE_DOCUMENTATION;MISLEADING_NAME"
    r(
        p,
        "DOCUMENTATION",
        "High",
        flags,
        "docs",
        "primarily v1 GadTools CLV architecture/profiles",
        "none",
        "LEAVE_IN_LEGACY_REPOSITORY",
        "MODULE_ARCHITECTURE layout paths outdated (src/ flat); mentions prefer custom-control-demo for cellctl",
    )

# OldDocs + implementation — archive
import subprocess

old = subprocess.check_output(
    ["git", "ls-files", "docs/OldDocs/**", "docs/implementation/**", "docs/samples/**"],
    text=True,
    cwd=str(Path(__file__).resolve().parents[2]),
).splitlines()
for p in old:
    r(
        p,
        "DOCUMENTATION",
        "High",
        "LEAVE_BEHIND;STALE_DOCUMENTATION",
        "historical",
        "historical extraction/modularisation/header reports for v1",
        "none",
        "ARCHIVE_OR_REMOVE_LATER",
    )

r(
    "docs/audit/LISTVIEW_ARCHITECTURE_FILE_AUDIT.md",
    "DOCUMENTATION",
    "High",
    "COPY_CANDIDATE",
    "this audit",
    "main audit report",
    "none",
    "COPY_TO_RICHLISTVIEW_THEN_CLEAN",
)
r(
    "docs/audit/LISTVIEW_DEPENDENCY_MAP.md",
    "DOCUMENTATION",
    "High",
    "COPY_CANDIDATE",
    "this audit",
    "dependency map",
    "none",
    "COPY_TO_RICHLISTVIEW_THEN_CLEAN",
)
r(
    "docs/audit/RICHLISTVIEW_EXTRACTION_MANIFEST.md",
    "DOCUMENTATION",
    "High",
    "COPY_CANDIDATE",
    "this audit",
    "extraction plan",
    "none",
    "COPY_TO_RICHLISTVIEW_THEN_CLEAN",
)
r(
    "docs/audit/LISTVIEW_AUDIT_MACHINE_READABLE.csv",
    "DOCUMENTATION",
    "High",
    "COPY_CANDIDATE",
    "this audit",
    "machine-readable classifications",
    "none",
    "COPY_TO_RICHLISTVIEW_THEN_CLEAN",
)
r(
    "docs/audit/_generate_audit_csv.py",
    "BUILD_OR_TOOLING",
    "High",
    "LEAVE_BEHIND",
    "audit regen",
    "generator for CSV",
    "none",
    "ARCHIVE_OR_REMOVE_LATER",
)

# Exclusion note row
r(
    "docs/AutoDocs/*",
    "DOCUMENTATION",
    "High",
    "LEAVE_BEHIND;DEAD_OR_UNREFERENCED",
    "reference only",
    "Third-party Amiga AutoDocs corpus — excluded from row-level source audit",
    "none",
    "LEAVE_IN_LEGACY_REPOSITORY",
    "Keep as Amiga API reference for both arches",
)


def main() -> None:
    # de-dupe by path keeping first
    seen = set()
    uniq = []
    for row in ROWS:
        if row[0] in seen:
            continue
        seen.add(row[0])
        uniq.append(row)
    with OUT.open("w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(COLS)
        w.writerows(uniq)
    # summary
    from collections import Counter

    c = Counter(r[1] for r in uniq)
    print(f"Wrote {len(uniq)} rows to {OUT}")
    for k, v in sorted(c.items()):
        print(f"  {k}: {v}")
    mixed = sum(1 for r in uniq if "SPLIT_REQUIRED" in r[3])
    print(f"  SPLIT_REQUIRED flags: {mixed}")


if __name__ == "__main__":
    main()
