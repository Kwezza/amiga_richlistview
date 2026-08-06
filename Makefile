# RichListview — full custom ListView control for classic AmigaOS
#
# Builds ONLY the custom control + demo. Does not include the legacy
# GadTools LISTVIEW_KIND enhancer, ASCII formatter, binders, selection
# adapter, or clv_cellctl implementation.
#
# VBCC +aos68k does NOT predefine __AMIGA__; -DRLV_PLATFORM_AMIGA=1 drives
# the central platform assert. Public API uses RLV_* / rlv_*.

CC := vc
CFLAGS := +aos68k -c99 -cpu=68000 -O2 -size -Isrc -DRLV_PLATFORM_AMIGA=1
LDFLAGS := +aos68k -cpu=68000 -O2 -size -final -lamiga -lauto

BUILD_DIR := build
BIN_DIR := bin

# Smart vertical scrolling (pixel shift + exposed-band regional paint).
# 0 = always full-viewport scroll paint.
RLV_ENABLE_SMART_SCROLL ?= 1

# Optional expandable / collapsible rows (+/- disclosure column).
# 0 = omit rlv_expand.o / rlv_disclosure.o; API stubs return FALSE.
RLV_ENABLE_EXPANDABLE_ROWS ?= 1

# Optional column sorting (view-order map). Default off — ordinary demos
# omit rlv_sort.o. Use rich-listview-demo-sort to exercise sorting.
RLV_ENABLE_SORTING ?= 0

# Optional interactive column resizing (two-column exchange). Default off —
# omit rlv_column_resize.o. Use rich-listview-demo-colresize or
# rich-listview-demo-sort-resize.
RLV_ENABLE_COLUMN_RESIZE ?= 0

# Optional crash-safe PROGDIR logger for rich-listview-demo-log only.
RLV_ENABLE_LOGGING ?= 0

# Optional alternating body-row backdrops (logical-row parity).
# 0 = omit rlv_alternate_rows.o and all hot-path branches.
RLV_ENABLE_ALTERNATE_ROWS ?= 0

# Shared Amiga V39+ adaptive-colour engine (ObtainBestPen / RGB helpers).
# Auto-enabled when any adaptive feature below is 1.
RLV_ENABLE_ADAPTIVE_COLOURS ?= 0

# Optional Amiga V39+ adaptive darker-row pen (requires ALTERNATE_ROWS + engine).
RLV_ENABLE_ADAPTIVE_ROW_PEN ?= 0

# Optional Amiga V39+ adaptive title-bar blend pen (requires engine).
RLV_ENABLE_ADAPTIVE_TITLE_PEN ?= 0

# Optional Amiga V39+ adaptive selection-fill blend pen (requires engine).
RLV_ENABLE_ADAPTIVE_SELECTION_PEN ?= 0

# Optional Amiga V39+ adaptive body-row divider pen (requires engine).
RLV_ENABLE_ADAPTIVE_DIVIDERS ?= 0

ifeq ($(RLV_ENABLE_ADAPTIVE_ROW_PEN),1)
RLV_ENABLE_ADAPTIVE_COLOURS := 1
endif
ifeq ($(RLV_ENABLE_ADAPTIVE_TITLE_PEN),1)
RLV_ENABLE_ADAPTIVE_COLOURS := 1
endif
ifeq ($(RLV_ENABLE_ADAPTIVE_SELECTION_PEN),1)
RLV_ENABLE_ADAPTIVE_COLOURS := 1
endif
ifeq ($(RLV_ENABLE_ADAPTIVE_DIVIDERS),1)
RLV_ENABLE_ADAPTIVE_COLOURS := 1
endif

RLV_ADAPTIVE_CFLAGS = -DRLV_ENABLE_ALTERNATE_ROWS=$(RLV_ENABLE_ALTERNATE_ROWS) \
	-DRLV_ENABLE_ADAPTIVE_COLOURS=$(RLV_ENABLE_ADAPTIVE_COLOURS) \
	-DRLV_ENABLE_ADAPTIVE_ROW_PEN=$(RLV_ENABLE_ADAPTIVE_ROW_PEN) \
	-DRLV_ENABLE_ADAPTIVE_TITLE_PEN=$(RLV_ENABLE_ADAPTIVE_TITLE_PEN) \
	-DRLV_ENABLE_ADAPTIVE_SELECTION_PEN=$(RLV_ENABLE_ADAPTIVE_SELECTION_PEN) \
	-DRLV_ENABLE_ADAPTIVE_DIVIDERS=$(RLV_ENABLE_ADAPTIVE_DIVIDERS)
RLV_ALTROW_CFLAGS = $(RLV_ADAPTIVE_CFLAGS)

RLV_CFLAGS = $(CFLAGS) -DRLV_ENABLE_SMART_SCROLL=$(RLV_ENABLE_SMART_SCROLL) \
	-DRLV_ENABLE_EXPANDABLE_ROWS=$(RLV_ENABLE_EXPANDABLE_ROWS) \
	-DRLV_ENABLE_SORTING=$(RLV_ENABLE_SORTING) \
	-DRLV_ENABLE_COLUMN_RESIZE=$(RLV_ENABLE_COLUMN_RESIZE) \
	$(RLV_ADAPTIVE_CFLAGS)
RLV_LOG_CFLAGS = $(CFLAGS) -DRLV_ENABLE_LOGGING \
	-DRLV_ENABLE_SMART_SCROLL=$(RLV_ENABLE_SMART_SCROLL) \
	-DRLV_ENABLE_EXPANDABLE_ROWS=$(RLV_ENABLE_EXPANDABLE_ROWS) \
	-DRLV_ENABLE_SORTING=$(RLV_ENABLE_SORTING) \
	-DRLV_ENABLE_COLUMN_RESIZE=$(RLV_ENABLE_COLUMN_RESIZE) \
	$(RLV_ALTROW_CFLAGS)
RLV_BENCH_CFLAGS = $(CFLAGS) -DRLV_ENABLE_BENCHMARKS \
	-DRLV_ENABLE_SMART_SCROLL=$(RLV_ENABLE_SMART_SCROLL) \
	-DRLV_ENABLE_EXPANDABLE_ROWS=$(RLV_ENABLE_EXPANDABLE_ROWS) \
	-DRLV_ENABLE_SORTING=$(RLV_ENABLE_SORTING) \
	-DRLV_ENABLE_COLUMN_RESIZE=$(RLV_ENABLE_COLUMN_RESIZE) \
	$(RLV_ALTROW_CFLAGS)
RLV_NOSMART_CFLAGS = $(CFLAGS) -DRLV_ENABLE_SMART_SCROLL=0 \
	-DRLV_ENABLE_EXPANDABLE_ROWS=$(RLV_ENABLE_EXPANDABLE_ROWS) \
	-DRLV_ENABLE_SORTING=$(RLV_ENABLE_SORTING) \
	-DRLV_ENABLE_COLUMN_RESIZE=$(RLV_ENABLE_COLUMN_RESIZE) \
	$(RLV_ALTROW_CFLAGS)
RLV_SORT_CFLAGS = $(CFLAGS) -DRLV_ENABLE_SMART_SCROLL=$(RLV_ENABLE_SMART_SCROLL) \
	-DRLV_ENABLE_EXPANDABLE_ROWS=$(RLV_ENABLE_EXPANDABLE_ROWS) \
	-DRLV_ENABLE_SORTING=1 \
	-DRLV_ENABLE_COLUMN_RESIZE=0 \
	$(RLV_ALTROW_CFLAGS)
RLV_SORT_LOG_CFLAGS = $(CFLAGS) -DRLV_ENABLE_LOGGING \
	-DRLV_ENABLE_SMART_SCROLL=$(RLV_ENABLE_SMART_SCROLL) \
	-DRLV_ENABLE_EXPANDABLE_ROWS=$(RLV_ENABLE_EXPANDABLE_ROWS) \
	-DRLV_ENABLE_SORTING=1 \
	-DRLV_ENABLE_COLUMN_RESIZE=0 \
	$(RLV_ALTROW_CFLAGS)
RLV_COLRESIZE_CFLAGS = $(CFLAGS) -DRLV_ENABLE_SMART_SCROLL=$(RLV_ENABLE_SMART_SCROLL) \
	-DRLV_ENABLE_EXPANDABLE_ROWS=$(RLV_ENABLE_EXPANDABLE_ROWS) \
	-DRLV_ENABLE_SORTING=0 \
	-DRLV_ENABLE_COLUMN_RESIZE=1 \
	$(RLV_ALTROW_CFLAGS)
RLV_SORT_RESIZE_CFLAGS = $(CFLAGS) -DRLV_ENABLE_SMART_SCROLL=$(RLV_ENABLE_SMART_SCROLL) \
	-DRLV_ENABLE_EXPANDABLE_ROWS=$(RLV_ENABLE_EXPANDABLE_ROWS) \
	-DRLV_ENABLE_SORTING=1 \
	-DRLV_ENABLE_COLUMN_RESIZE=1 \
	$(RLV_ALTROW_CFLAGS)
RLV_SORT_RESIZE_LOG_CFLAGS = $(CFLAGS) -DRLV_ENABLE_LOGGING \
	-DRLV_ENABLE_SMART_SCROLL=$(RLV_ENABLE_SMART_SCROLL) \
	-DRLV_ENABLE_EXPANDABLE_ROWS=$(RLV_ENABLE_EXPANDABLE_ROWS) \
	-DRLV_ENABLE_SORTING=1 \
	-DRLV_ENABLE_COLUMN_RESIZE=1 \
	$(RLV_ALTROW_CFLAGS)

RLV_LOG_DIR = $(BUILD_DIR)/rich_listview_log
RLV_BENCH_DIR = $(BUILD_DIR)/rich_listview_bench
RLV_NOSMART_DIR = $(BUILD_DIR)/rich_listview_nosmart
RLV_SORT_DIR = $(BUILD_DIR)/rich_listview_sort
RLV_SORT_LOG_DIR = $(BUILD_DIR)/rich_listview_sort_log
RLV_COLRESIZE_DIR = $(BUILD_DIR)/rich_listview_colresize
RLV_SORT_RESIZE_DIR = $(BUILD_DIR)/rich_listview_sort_resize
RLV_SORT_RESIZE_LOG_DIR = $(BUILD_DIR)/rich_listview_sort_resize_log
RLV_ADAPTIVE_DIR = $(BUILD_DIR)/rich_listview_adaptive

# Full-adaptive demo CFLAGS (isolated tree; does not mutate global RLV_ENABLE_*).
RLV_ADAPTIVE_DEMO_CFLAGS = $(CFLAGS) -DRLV_ENABLE_SMART_SCROLL=$(RLV_ENABLE_SMART_SCROLL) \
	-DRLV_ENABLE_EXPANDABLE_ROWS=$(RLV_ENABLE_EXPANDABLE_ROWS) \
	-DRLV_ENABLE_SORTING=$(RLV_ENABLE_SORTING) \
	-DRLV_ENABLE_COLUMN_RESIZE=$(RLV_ENABLE_COLUMN_RESIZE) \
	-DRLV_ENABLE_ALTERNATE_ROWS=1 \
	-DRLV_ENABLE_ADAPTIVE_COLOURS=1 \
	-DRLV_ENABLE_ADAPTIVE_ROW_PEN=1 \
	-DRLV_ENABLE_ADAPTIVE_TITLE_PEN=1 \
	-DRLV_ENABLE_ADAPTIVE_SELECTION_PEN=1 \
	-DRLV_ENABLE_ADAPTIVE_DIVIDERS=1

# ---------------------------------------------------------------------------
# Explicit object lists (no wildcards)
# ---------------------------------------------------------------------------

RLV_PLATFORM_OBJS = $(BUILD_DIR)/rich_listview/rlv_platform.o

RLV_OBJS = \
	$(BUILD_DIR)/rich_listview/rlv.o \
	$(BUILD_DIR)/rich_listview/rlv_layout.o \
	$(BUILD_DIR)/rich_listview/rlv_wrap.o \
	$(BUILD_DIR)/rich_listview/rlv_render.o \
	$(BUILD_DIR)/rich_listview/rlv_title_fill.o \
	$(BUILD_DIR)/rich_listview/rlv_selection_fill.o \
	$(BUILD_DIR)/rich_listview/rlv_adaptive_divider.o \
	$(BUILD_DIR)/rich_listview/rlv_checkbox.o \
	$(BUILD_DIR)/rich_listview/rlv_input.o \
	$(BUILD_DIR)/rich_listview/rlv_scroll.o \
	$(BUILD_DIR)/rich_listview/backends/rlv_backend_amiga_v36.o

ifeq ($(RLV_ENABLE_EXPANDABLE_ROWS),1)
RLV_OBJS += \
	$(BUILD_DIR)/rich_listview/rlv_expand.o \
	$(BUILD_DIR)/rich_listview/rlv_disclosure.o
endif

ifeq ($(RLV_ENABLE_SORTING),1)
RLV_OBJS += \
	$(BUILD_DIR)/rich_listview/rlv_sort.o
endif

ifeq ($(RLV_ENABLE_COLUMN_RESIZE),1)
RLV_OBJS += \
	$(BUILD_DIR)/rich_listview/rlv_column_resize.o
endif

ifeq ($(RLV_ENABLE_ALTERNATE_ROWS),1)
RLV_OBJS += \
	$(BUILD_DIR)/rich_listview/rlv_alternate_rows.o
endif

ifeq ($(RLV_ENABLE_ADAPTIVE_COLOURS),1)
RLV_OBJS += \
	$(BUILD_DIR)/rich_listview/backends/rlv_adaptive_colour.o
endif

RLV_LIBS = \
	$(RLV_PLATFORM_OBJS) \
	$(RLV_OBJS)

RLV_LOG_OBJS = \
	$(RLV_LOG_DIR)/rlv.o \
	$(RLV_LOG_DIR)/rlv_layout.o \
	$(RLV_LOG_DIR)/rlv_wrap.o \
	$(RLV_LOG_DIR)/rlv_render.o \
	$(RLV_LOG_DIR)/rlv_title_fill.o \
	$(RLV_LOG_DIR)/rlv_selection_fill.o \
	$(RLV_LOG_DIR)/rlv_adaptive_divider.o \
	$(RLV_LOG_DIR)/rlv_checkbox.o \
	$(RLV_LOG_DIR)/rlv_input.o \
	$(RLV_LOG_DIR)/rlv_scroll.o \
	$(RLV_LOG_DIR)/backends/rlv_backend_amiga_v36.o \
	$(RLV_LOG_DIR)/rlv_log.o

ifeq ($(RLV_ENABLE_EXPANDABLE_ROWS),1)
RLV_LOG_OBJS += \
	$(RLV_LOG_DIR)/rlv_expand.o \
	$(RLV_LOG_DIR)/rlv_disclosure.o
endif

ifeq ($(RLV_ENABLE_SORTING),1)
RLV_LOG_OBJS += \
	$(RLV_LOG_DIR)/rlv_sort.o
endif

ifeq ($(RLV_ENABLE_COLUMN_RESIZE),1)
RLV_LOG_OBJS += \
	$(RLV_LOG_DIR)/rlv_column_resize.o
endif

ifeq ($(RLV_ENABLE_ALTERNATE_ROWS),1)
RLV_LOG_OBJS += \
	$(RLV_LOG_DIR)/rlv_alternate_rows.o
endif

ifeq ($(RLV_ENABLE_ADAPTIVE_COLOURS),1)
RLV_LOG_OBJS += \
	$(RLV_LOG_DIR)/backends/rlv_adaptive_colour.o
endif

RLV_LOG_LIBS = \
	$(RLV_PLATFORM_OBJS) \
	$(RLV_LOG_OBJS)

RLV_BENCH_OBJS = \
	$(RLV_BENCH_DIR)/rlv.o \
	$(RLV_BENCH_DIR)/rlv_layout.o \
	$(RLV_BENCH_DIR)/rlv_wrap.o \
	$(RLV_BENCH_DIR)/rlv_render.o \
	$(RLV_BENCH_DIR)/rlv_title_fill.o \
	$(RLV_BENCH_DIR)/rlv_selection_fill.o \
	$(RLV_BENCH_DIR)/rlv_adaptive_divider.o \
	$(RLV_BENCH_DIR)/rlv_checkbox.o \
	$(RLV_BENCH_DIR)/rlv_input.o \
	$(RLV_BENCH_DIR)/rlv_scroll.o \
	$(RLV_BENCH_DIR)/backends/rlv_backend_amiga_v36.o \
	$(RLV_BENCH_DIR)/rlv_platform.o \
	$(RLV_BENCH_DIR)/rlv_bench.o

ifeq ($(RLV_ENABLE_EXPANDABLE_ROWS),1)
RLV_BENCH_OBJS += \
	$(RLV_BENCH_DIR)/rlv_expand.o \
	$(RLV_BENCH_DIR)/rlv_disclosure.o
endif

ifeq ($(RLV_ENABLE_SORTING),1)
RLV_BENCH_OBJS += \
	$(RLV_BENCH_DIR)/rlv_sort.o
endif

ifeq ($(RLV_ENABLE_COLUMN_RESIZE),1)
RLV_BENCH_OBJS += \
	$(RLV_BENCH_DIR)/rlv_column_resize.o
endif

ifeq ($(RLV_ENABLE_ALTERNATE_ROWS),1)
RLV_BENCH_OBJS += \
	$(RLV_BENCH_DIR)/rlv_alternate_rows.o
endif

ifeq ($(RLV_ENABLE_ADAPTIVE_COLOURS),1)
RLV_BENCH_OBJS += \
	$(RLV_BENCH_DIR)/backends/rlv_adaptive_colour.o
endif

RLV_BENCH_LIBS = \
	$(RLV_BENCH_OBJS)

RLV_NOSMART_PLATFORM_OBJS = $(RLV_NOSMART_DIR)/rlv_platform.o

RLV_NOSMART_OBJS = \
	$(RLV_NOSMART_DIR)/rlv.o \
	$(RLV_NOSMART_DIR)/rlv_layout.o \
	$(RLV_NOSMART_DIR)/rlv_wrap.o \
	$(RLV_NOSMART_DIR)/rlv_render.o \
	$(RLV_NOSMART_DIR)/rlv_title_fill.o \
	$(RLV_NOSMART_DIR)/rlv_selection_fill.o \
	$(RLV_NOSMART_DIR)/rlv_adaptive_divider.o \
	$(RLV_NOSMART_DIR)/rlv_checkbox.o \
	$(RLV_NOSMART_DIR)/rlv_input.o \
	$(RLV_NOSMART_DIR)/rlv_scroll.o \
	$(RLV_NOSMART_DIR)/backends/rlv_backend_amiga_v36.o

ifeq ($(RLV_ENABLE_EXPANDABLE_ROWS),1)
RLV_NOSMART_OBJS += \
	$(RLV_NOSMART_DIR)/rlv_expand.o \
	$(RLV_NOSMART_DIR)/rlv_disclosure.o
endif

ifeq ($(RLV_ENABLE_SORTING),1)
RLV_NOSMART_OBJS += \
	$(RLV_NOSMART_DIR)/rlv_sort.o
endif

ifeq ($(RLV_ENABLE_COLUMN_RESIZE),1)
RLV_NOSMART_OBJS += \
	$(RLV_NOSMART_DIR)/rlv_column_resize.o
endif

ifeq ($(RLV_ENABLE_ALTERNATE_ROWS),1)
RLV_NOSMART_OBJS += \
	$(RLV_NOSMART_DIR)/rlv_alternate_rows.o
endif

ifeq ($(RLV_ENABLE_ADAPTIVE_COLOURS),1)
RLV_NOSMART_OBJS += \
	$(RLV_NOSMART_DIR)/backends/rlv_adaptive_colour.o
endif

RLV_NOSMART_LIBS = \
	$(RLV_NOSMART_PLATFORM_OBJS) \
	$(RLV_NOSMART_OBJS)

# Isolated sorting-enabled tree (ordinary demos keep RLV_ENABLE_SORTING=0).
RLV_SORT_PLATFORM_OBJS = $(RLV_SORT_DIR)/rlv_platform.o

RLV_SORT_OBJS = \
	$(RLV_SORT_DIR)/rlv.o \
	$(RLV_SORT_DIR)/rlv_layout.o \
	$(RLV_SORT_DIR)/rlv_wrap.o \
	$(RLV_SORT_DIR)/rlv_render.o \
	$(RLV_SORT_DIR)/rlv_title_fill.o \
	$(RLV_SORT_DIR)/rlv_selection_fill.o \
	$(RLV_SORT_DIR)/rlv_adaptive_divider.o \
	$(RLV_SORT_DIR)/rlv_checkbox.o \
	$(RLV_SORT_DIR)/rlv_input.o \
	$(RLV_SORT_DIR)/rlv_scroll.o \
	$(RLV_SORT_DIR)/backends/rlv_backend_amiga_v36.o \
	$(RLV_SORT_DIR)/rlv_sort.o

ifeq ($(RLV_ENABLE_EXPANDABLE_ROWS),1)
RLV_SORT_OBJS += \
	$(RLV_SORT_DIR)/rlv_expand.o \
	$(RLV_SORT_DIR)/rlv_disclosure.o
endif

ifeq ($(RLV_ENABLE_ALTERNATE_ROWS),1)
RLV_SORT_OBJS += \
	$(RLV_SORT_DIR)/rlv_alternate_rows.o
endif

ifeq ($(RLV_ENABLE_ADAPTIVE_COLOURS),1)
RLV_SORT_OBJS += \
	$(RLV_SORT_DIR)/backends/rlv_adaptive_colour.o
endif

RLV_SORT_LIBS = \
	$(RLV_SORT_PLATFORM_OBJS) \
	$(RLV_SORT_OBJS)

# Sorting + logging (diagnose view map / sort decisions → PROGDIR:rlv.log).
RLV_SORT_LOG_PLATFORM_OBJS = $(RLV_SORT_LOG_DIR)/rlv_platform.o

RLV_SORT_LOG_OBJS = \
	$(RLV_SORT_LOG_DIR)/rlv.o \
	$(RLV_SORT_LOG_DIR)/rlv_layout.o \
	$(RLV_SORT_LOG_DIR)/rlv_wrap.o \
	$(RLV_SORT_LOG_DIR)/rlv_render.o \
	$(RLV_SORT_LOG_DIR)/rlv_title_fill.o \
	$(RLV_SORT_LOG_DIR)/rlv_selection_fill.o \
	$(RLV_SORT_LOG_DIR)/rlv_adaptive_divider.o \
	$(RLV_SORT_LOG_DIR)/rlv_checkbox.o \
	$(RLV_SORT_LOG_DIR)/rlv_input.o \
	$(RLV_SORT_LOG_DIR)/rlv_scroll.o \
	$(RLV_SORT_LOG_DIR)/backends/rlv_backend_amiga_v36.o \
	$(RLV_SORT_LOG_DIR)/rlv_sort.o \
	$(RLV_SORT_LOG_DIR)/rlv_log.o

ifeq ($(RLV_ENABLE_EXPANDABLE_ROWS),1)
RLV_SORT_LOG_OBJS += \
	$(RLV_SORT_LOG_DIR)/rlv_expand.o \
	$(RLV_SORT_LOG_DIR)/rlv_disclosure.o
endif

ifeq ($(RLV_ENABLE_ALTERNATE_ROWS),1)
RLV_SORT_LOG_OBJS += \
	$(RLV_SORT_LOG_DIR)/rlv_alternate_rows.o
endif

ifeq ($(RLV_ENABLE_ADAPTIVE_COLOURS),1)
RLV_SORT_LOG_OBJS += \
	$(RLV_SORT_LOG_DIR)/backends/rlv_adaptive_colour.o
endif

RLV_SORT_LOG_LIBS = \
	$(RLV_SORT_LOG_PLATFORM_OBJS) \
	$(RLV_SORT_LOG_OBJS)

# Isolated column-resize tree (no sorting).
RLV_COLRESIZE_PLATFORM_OBJS = $(RLV_COLRESIZE_DIR)/rlv_platform.o

RLV_COLRESIZE_OBJS = \
	$(RLV_COLRESIZE_DIR)/rlv.o \
	$(RLV_COLRESIZE_DIR)/rlv_layout.o \
	$(RLV_COLRESIZE_DIR)/rlv_wrap.o \
	$(RLV_COLRESIZE_DIR)/rlv_render.o \
	$(RLV_COLRESIZE_DIR)/rlv_title_fill.o \
	$(RLV_COLRESIZE_DIR)/rlv_selection_fill.o \
	$(RLV_COLRESIZE_DIR)/rlv_adaptive_divider.o \
	$(RLV_COLRESIZE_DIR)/rlv_checkbox.o \
	$(RLV_COLRESIZE_DIR)/rlv_input.o \
	$(RLV_COLRESIZE_DIR)/rlv_scroll.o \
	$(RLV_COLRESIZE_DIR)/backends/rlv_backend_amiga_v36.o \
	$(RLV_COLRESIZE_DIR)/rlv_column_resize.o

ifeq ($(RLV_ENABLE_EXPANDABLE_ROWS),1)
RLV_COLRESIZE_OBJS += \
	$(RLV_COLRESIZE_DIR)/rlv_expand.o \
	$(RLV_COLRESIZE_DIR)/rlv_disclosure.o
endif

ifeq ($(RLV_ENABLE_ALTERNATE_ROWS),1)
RLV_COLRESIZE_OBJS += \
	$(RLV_COLRESIZE_DIR)/rlv_alternate_rows.o
endif

ifeq ($(RLV_ENABLE_ADAPTIVE_COLOURS),1)
RLV_COLRESIZE_OBJS += \
	$(RLV_COLRESIZE_DIR)/backends/rlv_adaptive_colour.o
endif

RLV_COLRESIZE_LIBS = \
	$(RLV_COLRESIZE_PLATFORM_OBJS) \
	$(RLV_COLRESIZE_OBJS)

# Sorting + column resize (primary interactive demo for both features).
RLV_SORT_RESIZE_PLATFORM_OBJS = $(RLV_SORT_RESIZE_DIR)/rlv_platform.o

RLV_SORT_RESIZE_OBJS = \
	$(RLV_SORT_RESIZE_DIR)/rlv.o \
	$(RLV_SORT_RESIZE_DIR)/rlv_layout.o \
	$(RLV_SORT_RESIZE_DIR)/rlv_wrap.o \
	$(RLV_SORT_RESIZE_DIR)/rlv_render.o \
	$(RLV_SORT_RESIZE_DIR)/rlv_title_fill.o \
	$(RLV_SORT_RESIZE_DIR)/rlv_selection_fill.o \
	$(RLV_SORT_RESIZE_DIR)/rlv_adaptive_divider.o \
	$(RLV_SORT_RESIZE_DIR)/rlv_checkbox.o \
	$(RLV_SORT_RESIZE_DIR)/rlv_input.o \
	$(RLV_SORT_RESIZE_DIR)/rlv_scroll.o \
	$(RLV_SORT_RESIZE_DIR)/backends/rlv_backend_amiga_v36.o \
	$(RLV_SORT_RESIZE_DIR)/rlv_sort.o \
	$(RLV_SORT_RESIZE_DIR)/rlv_column_resize.o

ifeq ($(RLV_ENABLE_EXPANDABLE_ROWS),1)
RLV_SORT_RESIZE_OBJS += \
	$(RLV_SORT_RESIZE_DIR)/rlv_expand.o \
	$(RLV_SORT_RESIZE_DIR)/rlv_disclosure.o
endif

ifeq ($(RLV_ENABLE_ALTERNATE_ROWS),1)
RLV_SORT_RESIZE_OBJS += \
	$(RLV_SORT_RESIZE_DIR)/rlv_alternate_rows.o
endif

ifeq ($(RLV_ENABLE_ADAPTIVE_COLOURS),1)
RLV_SORT_RESIZE_OBJS += \
	$(RLV_SORT_RESIZE_DIR)/backends/rlv_adaptive_colour.o
endif

RLV_SORT_RESIZE_LIBS = \
	$(RLV_SORT_RESIZE_PLATFORM_OBJS) \
	$(RLV_SORT_RESIZE_OBJS)

RLV_SORT_RESIZE_LOG_PLATFORM_OBJS = $(RLV_SORT_RESIZE_LOG_DIR)/rlv_platform.o

RLV_SORT_RESIZE_LOG_OBJS = \
	$(RLV_SORT_RESIZE_LOG_DIR)/rlv.o \
	$(RLV_SORT_RESIZE_LOG_DIR)/rlv_layout.o \
	$(RLV_SORT_RESIZE_LOG_DIR)/rlv_wrap.o \
	$(RLV_SORT_RESIZE_LOG_DIR)/rlv_render.o \
	$(RLV_SORT_RESIZE_LOG_DIR)/rlv_title_fill.o \
	$(RLV_SORT_RESIZE_LOG_DIR)/rlv_selection_fill.o \
	$(RLV_SORT_RESIZE_LOG_DIR)/rlv_adaptive_divider.o \
	$(RLV_SORT_RESIZE_LOG_DIR)/rlv_checkbox.o \
	$(RLV_SORT_RESIZE_LOG_DIR)/rlv_input.o \
	$(RLV_SORT_RESIZE_LOG_DIR)/rlv_scroll.o \
	$(RLV_SORT_RESIZE_LOG_DIR)/backends/rlv_backend_amiga_v36.o \
	$(RLV_SORT_RESIZE_LOG_DIR)/rlv_sort.o \
	$(RLV_SORT_RESIZE_LOG_DIR)/rlv_column_resize.o \
	$(RLV_SORT_RESIZE_LOG_DIR)/rlv_log.o

ifeq ($(RLV_ENABLE_EXPANDABLE_ROWS),1)
RLV_SORT_RESIZE_LOG_OBJS += \
	$(RLV_SORT_RESIZE_LOG_DIR)/rlv_expand.o \
	$(RLV_SORT_RESIZE_LOG_DIR)/rlv_disclosure.o
endif

ifeq ($(RLV_ENABLE_ALTERNATE_ROWS),1)
RLV_SORT_RESIZE_LOG_OBJS += \
	$(RLV_SORT_RESIZE_LOG_DIR)/rlv_alternate_rows.o
endif

ifeq ($(RLV_ENABLE_ADAPTIVE_COLOURS),1)
RLV_SORT_RESIZE_LOG_OBJS += \
	$(RLV_SORT_RESIZE_LOG_DIR)/backends/rlv_adaptive_colour.o
endif

RLV_SORT_RESIZE_LOG_LIBS = \
	$(RLV_SORT_RESIZE_LOG_PLATFORM_OBJS) \
	$(RLV_SORT_RESIZE_LOG_OBJS)

# Isolated full-adaptive colours tree.
RLV_ADAPTIVE_PLATFORM_OBJS = $(RLV_ADAPTIVE_DIR)/rlv_platform.o

RLV_ADAPTIVE_OBJS = \
	$(RLV_ADAPTIVE_DIR)/rlv.o \
	$(RLV_ADAPTIVE_DIR)/rlv_layout.o \
	$(RLV_ADAPTIVE_DIR)/rlv_wrap.o \
	$(RLV_ADAPTIVE_DIR)/rlv_render.o \
	$(RLV_ADAPTIVE_DIR)/rlv_title_fill.o \
	$(RLV_ADAPTIVE_DIR)/rlv_selection_fill.o \
	$(RLV_ADAPTIVE_DIR)/rlv_adaptive_divider.o \
	$(RLV_ADAPTIVE_DIR)/rlv_checkbox.o \
	$(RLV_ADAPTIVE_DIR)/rlv_input.o \
	$(RLV_ADAPTIVE_DIR)/rlv_scroll.o \
	$(RLV_ADAPTIVE_DIR)/backends/rlv_backend_amiga_v36.o \
	$(RLV_ADAPTIVE_DIR)/rlv_alternate_rows.o \
	$(RLV_ADAPTIVE_DIR)/backends/rlv_adaptive_colour.o

ifeq ($(RLV_ENABLE_EXPANDABLE_ROWS),1)
RLV_ADAPTIVE_OBJS += \
	$(RLV_ADAPTIVE_DIR)/rlv_expand.o \
	$(RLV_ADAPTIVE_DIR)/rlv_disclosure.o
endif

ifeq ($(RLV_ENABLE_SORTING),1)
RLV_ADAPTIVE_OBJS += \
	$(RLV_ADAPTIVE_DIR)/rlv_sort.o
endif

ifeq ($(RLV_ENABLE_COLUMN_RESIZE),1)
RLV_ADAPTIVE_OBJS += \
	$(RLV_ADAPTIVE_DIR)/rlv_column_resize.o
endif

RLV_ADAPTIVE_LIBS = \
	$(RLV_ADAPTIVE_PLATFORM_OBJS) \
	$(RLV_ADAPTIVE_OBJS)

EXAMPLE_OBJ = $(BUILD_DIR)/examples/rich_listview_demo.o
EXAMPLE_LOG_OBJ = $(BUILD_DIR)/examples/rich_listview_demo_log.o
EXAMPLE_BENCH_OBJ = $(BUILD_DIR)/examples/rich_listview_demo_bench.o
EXAMPLE_CONSOLE_OBJ = $(BUILD_DIR)/examples/rich_listview_demo_console.o
EXAMPLE_NOSMART_OBJ = $(RLV_NOSMART_DIR)/examples/rich_listview_demo.o
EXAMPLE_SORT_OBJ = $(RLV_SORT_DIR)/examples/rich_listview_demo.o
EXAMPLE_SORT_LOG_OBJ = $(RLV_SORT_LOG_DIR)/examples/rich_listview_demo.o
EXAMPLE_COLRESIZE_OBJ = $(RLV_COLRESIZE_DIR)/examples/rich_listview_demo.o
EXAMPLE_SORT_RESIZE_OBJ = $(RLV_SORT_RESIZE_DIR)/examples/rich_listview_demo.o
EXAMPLE_SORT_RESIZE_LOG_OBJ = $(RLV_SORT_RESIZE_LOG_DIR)/examples/rich_listview_demo.o
EXAMPLE_ADAPTIVE_OBJ = $(RLV_ADAPTIVE_DIR)/examples/rich_listview_demo.o

# ---------------------------------------------------------------------------
# Targets
# ---------------------------------------------------------------------------

.PHONY: all clean \
	rich-listview-demo rich-listview-demo-log \
	rich-listview-demo-bench rich-listview-demo-nosmart \
	rich-listview-demo-sort rich-listview-demo-sort-log \
	rich-listview-demo-colresize \
	rich-listview-demo-sort-resize rich-listview-demo-sort-resize-log \
	rich-listview-demo-adaptive \
	rich-listview-demo-console \
	help \
	public-header-audit \
	column-resize-geometry-test

all: rich-listview-demo

help:
	@echo ================================================================================
	@echo RichListview - Build Targets
	@echo ================================================================================
	@echo.
	@echo Available targets:
	@echo   all                              Default: rich-listview-demo
	@echo   rich-listview-demo               Demo (no console stdout traces by default)
	@echo   rich-listview-demo-console      Demo with DEMO_ENABLE_CONSOLE (printf/fflush to stdout)
	@echo   rich-listview-demo-log           Demo with RLV_ENABLE_LOGGING (writes PROGDIR:rlv.log)
	@echo   rich-listview-demo-bench         Demo with RLV_ENABLE_BENCHMARKS
	@echo   rich-listview-demo-nosmart       Smart scroll disabled (RLV_ENABLE_SMART_SCROLL=0)
	@echo   rich-listview-demo-sort          Demo with sorting enabled (RLV_ENABLE_SORTING=1)
	@echo   rich-listview-demo-sort-log     Sorting + logging (RLV_ENABLE_SORTING=1, RLV_ENABLE_LOGGING=1)
	@echo   rich-listview-demo-colresize    Column resize only (RLV_ENABLE_COLUMN_RESIZE=1)
	@echo   rich-listview-demo-sort-resize  Sorting + column resize
	@echo   rich-listview-demo-sort-resize-log  Sorting + resize + logging
	@echo   rich-listview-demo-adaptive     All adaptive colour features enabled
	@echo   public-header-audit             Compile-only audit for public headers
	@echo   column-resize-geometry-test     Host clamp/snap/preview rect checks
	@echo   clean                            Remove build/ and bin/
	@echo.

# Real directory targets (avoid PowerShell startup + New-Item overhead).
# Make will only run these rules if the directory target doesn't exist.
DIRS = \
	$(BUILD_DIR) \
	$(BUILD_DIR)/examples \
	$(BUILD_DIR)/rich_listview \
	$(BUILD_DIR)/rich_listview/backends \
	$(RLV_LOG_DIR) \
	$(RLV_LOG_DIR)/backends \
	$(RLV_BENCH_DIR) \
	$(RLV_BENCH_DIR)/backends \
	$(RLV_NOSMART_DIR) \
	$(RLV_NOSMART_DIR)/backends \
	$(RLV_NOSMART_DIR)/examples \
	$(RLV_SORT_DIR) \
	$(RLV_SORT_DIR)/backends \
	$(RLV_SORT_DIR)/examples \
	$(RLV_SORT_LOG_DIR) \
	$(RLV_SORT_LOG_DIR)/backends \
	$(RLV_SORT_LOG_DIR)/examples \
	$(RLV_COLRESIZE_DIR) \
	$(RLV_COLRESIZE_DIR)/backends \
	$(RLV_COLRESIZE_DIR)/examples \
	$(RLV_SORT_RESIZE_DIR) \
	$(RLV_SORT_RESIZE_DIR)/backends \
	$(RLV_SORT_RESIZE_DIR)/examples \
	$(RLV_SORT_RESIZE_LOG_DIR) \
	$(RLV_SORT_RESIZE_LOG_DIR)/backends \
	$(RLV_SORT_RESIZE_LOG_DIR)/examples \
	$(RLV_ADAPTIVE_DIR) \
	$(RLV_ADAPTIVE_DIR)/backends \
	$(RLV_ADAPTIVE_DIR)/examples \
	$(BUILD_DIR)/tests \
	$(BUILD_DIR)/tests/public_headers \
	$(BIN_DIR)

$(DIRS):
	@:

DIRS_STAMP = $(BUILD_DIR)/.dirs_stamp

$(DIRS): $(DIRS_STAMP)

$(DIRS_STAMP):
	@powershell -NoProfile -Command "New-Item -ItemType Directory -Force -Path '$(BUILD_DIR)/examples','$(BUILD_DIR)/rich_listview/backends','$(RLV_LOG_DIR)/backends','$(RLV_BENCH_DIR)/backends','$(RLV_NOSMART_DIR)/backends','$(RLV_NOSMART_DIR)/examples','$(RLV_SORT_DIR)/backends','$(RLV_SORT_DIR)/examples','$(RLV_SORT_LOG_DIR)/backends','$(RLV_SORT_LOG_DIR)/examples','$(RLV_COLRESIZE_DIR)/backends','$(RLV_COLRESIZE_DIR)/examples','$(RLV_SORT_RESIZE_DIR)/backends','$(RLV_SORT_RESIZE_DIR)/examples','$(RLV_SORT_RESIZE_LOG_DIR)/backends','$(RLV_SORT_RESIZE_LOG_DIR)/examples','$(RLV_ADAPTIVE_DIR)/backends','$(RLV_ADAPTIVE_DIR)/examples','$(BUILD_DIR)/tests/public_headers','$(BIN_DIR)' | Out-Null; '' | Out-File -FilePath '$(DIRS_STAMP)' -Encoding ASCII"

# cmd.exe mkdir does not create intermediate parents, so model directory
# hierarchy explicitly to ensure parent dirs exist first.
$(BUILD_DIR)/examples: $(BUILD_DIR)
$(BUILD_DIR)/rich_listview: $(BUILD_DIR)
$(BUILD_DIR)/rich_listview/backends: $(BUILD_DIR)/rich_listview

$(RLV_LOG_DIR): $(BUILD_DIR)
$(RLV_LOG_DIR)/backends: $(RLV_LOG_DIR)

$(RLV_BENCH_DIR): $(BUILD_DIR)
$(RLV_BENCH_DIR)/backends: $(RLV_BENCH_DIR)

$(RLV_NOSMART_DIR): $(BUILD_DIR)
$(RLV_NOSMART_DIR)/backends: $(RLV_NOSMART_DIR)
$(RLV_NOSMART_DIR)/examples: $(RLV_NOSMART_DIR)

$(RLV_SORT_DIR): $(BUILD_DIR)
$(RLV_SORT_DIR)/backends: $(RLV_SORT_DIR)
$(RLV_SORT_DIR)/examples: $(RLV_SORT_DIR)

$(RLV_SORT_LOG_DIR): $(BUILD_DIR)
$(RLV_SORT_LOG_DIR)/backends: $(RLV_SORT_LOG_DIR)
$(RLV_SORT_LOG_DIR)/examples: $(RLV_SORT_LOG_DIR)

$(RLV_COLRESIZE_DIR): $(BUILD_DIR)
$(RLV_COLRESIZE_DIR)/backends: $(RLV_COLRESIZE_DIR)
$(RLV_COLRESIZE_DIR)/examples: $(RLV_COLRESIZE_DIR)

$(RLV_SORT_RESIZE_DIR): $(BUILD_DIR)
$(RLV_SORT_RESIZE_DIR)/backends: $(RLV_SORT_RESIZE_DIR)
$(RLV_SORT_RESIZE_DIR)/examples: $(RLV_SORT_RESIZE_DIR)

$(RLV_SORT_RESIZE_LOG_DIR): $(BUILD_DIR)
$(RLV_SORT_RESIZE_LOG_DIR)/backends: $(RLV_SORT_RESIZE_LOG_DIR)
$(RLV_SORT_RESIZE_LOG_DIR)/examples: $(RLV_SORT_RESIZE_LOG_DIR)

$(RLV_ADAPTIVE_DIR): $(BUILD_DIR)
$(RLV_ADAPTIVE_DIR)/backends: $(RLV_ADAPTIVE_DIR)
$(RLV_ADAPTIVE_DIR)/examples: $(RLV_ADAPTIVE_DIR)

$(BUILD_DIR)/tests: $(BUILD_DIR)
$(BUILD_DIR)/tests/public_headers: $(BUILD_DIR)/tests

# Compile-only: public headers must not need private includes.
public-header-audit: $(DIRS) \
	$(BUILD_DIR)/tests/public_headers/rlv_public_core.o \
	$(BUILD_DIR)/tests/public_headers/rlv_public_backend.o

$(BUILD_DIR)/tests/public_headers/%.o: tests/public_headers/%.c | $(BUILD_DIR)/tests/public_headers
	$(CC) $(RLV_CFLAGS) -c -o $@ $<

# Host-side math checks (no Amiga libs). Prefer gcc/cc; cl needs VS env.
column-resize-geometry-test: $(DIRS)
	@powershell -NoProfile -Command "\
		$$out = '$(BIN_DIR)/test_resize_math.exe'; \
		$$src = 'tests/column_resize/test_resize_math.c'; \
		if (Get-Command gcc -ErrorAction SilentlyContinue) { \
			& gcc -Wall -Wextra -I. -o $$out $$src; if ($$LASTEXITCODE -ne 0) { exit $$LASTEXITCODE } \
		} elseif (Get-Command cc -ErrorAction SilentlyContinue) { \
			& cc -Wall -Wextra -I. -o $$out $$src; if ($$LASTEXITCODE -ne 0) { exit $$LASTEXITCODE } \
		} elseif (Get-Command cl -ErrorAction SilentlyContinue) { \
			& cl /nologo /W3 /I. $$src /Fe:$$out; if ($$LASTEXITCODE -ne 0) { exit $$LASTEXITCODE } \
		} else { Write-Error 'Need gcc, cc, or cl for host test'; exit 1 }; \
		& $$out; exit $$LASTEXITCODE"

rich-listview-demo: $(DIRS) $(BIN_DIR)/rich-listview-demo
rich-listview-demo-log: $(DIRS) $(BIN_DIR)/rich-listview-demo-log
rich-listview-demo-bench: $(DIRS) $(BIN_DIR)/rich-listview-demo-bench
rich-listview-demo-nosmart: $(DIRS) $(BIN_DIR)/rich-listview-demo-nosmart
rich-listview-demo-sort: $(DIRS) $(BIN_DIR)/rich-listview-demo-sort
rich-listview-demo-sort-log: $(DIRS) $(BIN_DIR)/rich-listview-demo-sort-log
rich-listview-demo-colresize: $(DIRS) $(BIN_DIR)/rich-listview-demo-colresize
rich-listview-demo-sort-resize: $(DIRS) $(BIN_DIR)/rich-listview-demo-sort-resize
rich-listview-demo-sort-resize-log: $(DIRS) $(BIN_DIR)/rich-listview-demo-sort-resize-log
rich-listview-demo-adaptive: $(DIRS) $(BIN_DIR)/rich-listview-demo-adaptive
rich-listview-demo-console: $(DIRS) $(BIN_DIR)/rich-listview-demo-console

$(BIN_DIR)/rich-listview-demo: $(RLV_LIBS) $(EXAMPLE_OBJ)
	$(CC) $(LDFLAGS) -o $@ $(RLV_LIBS) $(EXAMPLE_OBJ)

$(BIN_DIR)/rich-listview-demo-log: $(RLV_LOG_LIBS) $(EXAMPLE_LOG_OBJ)
	$(CC) $(LDFLAGS) -o $@ $(RLV_LOG_LIBS) $(EXAMPLE_LOG_OBJ)

$(BIN_DIR)/rich-listview-demo-bench: $(RLV_BENCH_LIBS) $(EXAMPLE_BENCH_OBJ)
	$(CC) $(LDFLAGS) -o $@ $(RLV_BENCH_LIBS) $(EXAMPLE_BENCH_OBJ)

$(BIN_DIR)/rich-listview-demo-nosmart: $(RLV_NOSMART_LIBS) $(EXAMPLE_NOSMART_OBJ)
	$(CC) $(LDFLAGS) -o $@ $(RLV_NOSMART_LIBS) $(EXAMPLE_NOSMART_OBJ)

$(BIN_DIR)/rich-listview-demo-sort: $(RLV_SORT_LIBS) $(EXAMPLE_SORT_OBJ)
	$(CC) $(LDFLAGS) -o $@ $(RLV_SORT_LIBS) $(EXAMPLE_SORT_OBJ)

$(BIN_DIR)/rich-listview-demo-sort-log: $(RLV_SORT_LOG_LIBS) $(EXAMPLE_SORT_LOG_OBJ)
	$(CC) $(LDFLAGS) -o $@ $(RLV_SORT_LOG_LIBS) $(EXAMPLE_SORT_LOG_OBJ)

$(BIN_DIR)/rich-listview-demo-colresize: $(RLV_COLRESIZE_LIBS) $(EXAMPLE_COLRESIZE_OBJ)
	$(CC) $(LDFLAGS) -o $@ $(RLV_COLRESIZE_LIBS) $(EXAMPLE_COLRESIZE_OBJ)

$(BIN_DIR)/rich-listview-demo-sort-resize: $(RLV_SORT_RESIZE_LIBS) $(EXAMPLE_SORT_RESIZE_OBJ)
	$(CC) $(LDFLAGS) -o $@ $(RLV_SORT_RESIZE_LIBS) $(EXAMPLE_SORT_RESIZE_OBJ)

$(BIN_DIR)/rich-listview-demo-sort-resize-log: $(RLV_SORT_RESIZE_LOG_LIBS) $(EXAMPLE_SORT_RESIZE_LOG_OBJ)
	$(CC) $(LDFLAGS) -o $@ $(RLV_SORT_RESIZE_LOG_LIBS) $(EXAMPLE_SORT_RESIZE_LOG_OBJ)

$(BIN_DIR)/rich-listview-demo-adaptive: $(RLV_ADAPTIVE_LIBS) $(EXAMPLE_ADAPTIVE_OBJ)
	$(CC) $(LDFLAGS) -o $@ $(RLV_ADAPTIVE_LIBS) $(EXAMPLE_ADAPTIVE_OBJ)

$(BIN_DIR)/rich-listview-demo-console: $(RLV_LIBS) $(EXAMPLE_CONSOLE_OBJ)
	$(CC) $(LDFLAGS) -o $@ $(RLV_LIBS) $(EXAMPLE_CONSOLE_OBJ)

# ---------------------------------------------------------------------------
# Compile rules
# ---------------------------------------------------------------------------

# Private control layout lives in rlv_internal.h. Without these deps, a
# mid/end-struct field addition can leave stale .o files with wrong offsets
# (pointer corruption / Software Failure during layout or paint).
RLV_CORE_HDRS = \
	src/rich_listview/rich_listview.h \
	src/rich_listview/rlv_internal.h \
	src/rich_listview/rlv_draw.h \
	src/rich_listview/rlv_features.h \
	src/rich_listview/rlv_alternate_rows.h \
	src/rich_listview/rlv_platform.h \
	src/rich_listview/rlv_bench_internal.h

$(BUILD_DIR)/rich_listview/%.o: src/rich_listview/%.c $(RLV_CORE_HDRS) | $(BUILD_DIR)/rich_listview
	$(CC) $(RLV_CFLAGS) -c -o $@ $<

$(BUILD_DIR)/rich_listview/backends/%.o: src/rich_listview/backends/%.c $(RLV_CORE_HDRS) \
	src/rich_listview/backends/rlv_backend_amiga_v36.h | $(BUILD_DIR)/rich_listview/backends
	$(CC) $(RLV_CFLAGS) -c -o $@ $<

$(RLV_LOG_DIR)/%.o: src/rich_listview/%.c $(RLV_CORE_HDRS) | $(RLV_LOG_DIR)
	$(CC) $(RLV_LOG_CFLAGS) -c -o $@ $<

$(RLV_LOG_DIR)/backends/%.o: src/rich_listview/backends/%.c $(RLV_CORE_HDRS) \
	src/rich_listview/backends/rlv_backend_amiga_v36.h | $(RLV_LOG_DIR)/backends
	$(CC) $(RLV_LOG_CFLAGS) -c -o $@ $<

$(RLV_BENCH_DIR)/%.o: src/rich_listview/%.c $(RLV_CORE_HDRS) | $(RLV_BENCH_DIR)
	$(CC) $(RLV_BENCH_CFLAGS) -c -o $@ $<

$(RLV_BENCH_DIR)/backends/%.o: src/rich_listview/backends/%.c $(RLV_CORE_HDRS) \
	src/rich_listview/backends/rlv_backend_amiga_v36.h | $(RLV_BENCH_DIR)/backends
	$(CC) $(RLV_BENCH_CFLAGS) -c -o $@ $<

$(RLV_NOSMART_DIR)/%.o: src/rich_listview/%.c $(RLV_CORE_HDRS) | $(RLV_NOSMART_DIR)
	$(CC) $(RLV_NOSMART_CFLAGS) -c -o $@ $<

$(RLV_NOSMART_DIR)/backends/%.o: src/rich_listview/backends/%.c $(RLV_CORE_HDRS) \
	src/rich_listview/backends/rlv_backend_amiga_v36.h | $(RLV_NOSMART_DIR)/backends
	$(CC) $(RLV_NOSMART_CFLAGS) -c -o $@ $<

$(RLV_SORT_DIR)/%.o: src/rich_listview/%.c $(RLV_CORE_HDRS) | $(RLV_SORT_DIR)
	$(CC) $(RLV_SORT_CFLAGS) -c -o $@ $<

$(RLV_SORT_DIR)/backends/%.o: src/rich_listview/backends/%.c $(RLV_CORE_HDRS) \
	src/rich_listview/backends/rlv_backend_amiga_v36.h | $(RLV_SORT_DIR)/backends
	$(CC) $(RLV_SORT_CFLAGS) -c -o $@ $<

$(RLV_SORT_LOG_DIR)/%.o: src/rich_listview/%.c $(RLV_CORE_HDRS) | $(RLV_SORT_LOG_DIR)
	$(CC) $(RLV_SORT_LOG_CFLAGS) -c -o $@ $<

$(RLV_SORT_LOG_DIR)/backends/%.o: src/rich_listview/backends/%.c $(RLV_CORE_HDRS) \
	src/rich_listview/backends/rlv_backend_amiga_v36.h | $(RLV_SORT_LOG_DIR)/backends
	$(CC) $(RLV_SORT_LOG_CFLAGS) -c -o $@ $<

$(RLV_COLRESIZE_DIR)/%.o: src/rich_listview/%.c $(RLV_CORE_HDRS) | $(RLV_COLRESIZE_DIR)
	$(CC) $(RLV_COLRESIZE_CFLAGS) -c -o $@ $<

$(RLV_COLRESIZE_DIR)/backends/%.o: src/rich_listview/backends/%.c $(RLV_CORE_HDRS) \
	src/rich_listview/backends/rlv_backend_amiga_v36.h | $(RLV_COLRESIZE_DIR)/backends
	$(CC) $(RLV_COLRESIZE_CFLAGS) -c -o $@ $<

$(RLV_SORT_RESIZE_DIR)/%.o: src/rich_listview/%.c $(RLV_CORE_HDRS) | $(RLV_SORT_RESIZE_DIR)
	$(CC) $(RLV_SORT_RESIZE_CFLAGS) -c -o $@ $<

$(RLV_SORT_RESIZE_DIR)/backends/%.o: src/rich_listview/backends/%.c $(RLV_CORE_HDRS) \
	src/rich_listview/backends/rlv_backend_amiga_v36.h | $(RLV_SORT_RESIZE_DIR)/backends
	$(CC) $(RLV_SORT_RESIZE_CFLAGS) -c -o $@ $<

$(RLV_SORT_RESIZE_LOG_DIR)/%.o: src/rich_listview/%.c $(RLV_CORE_HDRS) | $(RLV_SORT_RESIZE_LOG_DIR)
	$(CC) $(RLV_SORT_RESIZE_LOG_CFLAGS) -c -o $@ $<

$(RLV_SORT_RESIZE_LOG_DIR)/backends/%.o: src/rich_listview/backends/%.c $(RLV_CORE_HDRS) \
	src/rich_listview/backends/rlv_backend_amiga_v36.h | $(RLV_SORT_RESIZE_LOG_DIR)/backends
	$(CC) $(RLV_SORT_RESIZE_LOG_CFLAGS) -c -o $@ $<

$(RLV_ADAPTIVE_DIR)/%.o: src/rich_listview/%.c $(RLV_CORE_HDRS) | $(RLV_ADAPTIVE_DIR)
	$(CC) $(RLV_ADAPTIVE_DEMO_CFLAGS) -c -o $@ $<

$(RLV_ADAPTIVE_DIR)/backends/%.o: src/rich_listview/backends/%.c $(RLV_CORE_HDRS) \
	src/rich_listview/backends/rlv_backend_amiga_v36.h \
	src/rich_listview/backends/rlv_adaptive_colour.h | $(RLV_ADAPTIVE_DIR)/backends
	$(CC) $(RLV_ADAPTIVE_DEMO_CFLAGS) -c -o $@ $<

$(BUILD_DIR)/examples/rich_listview_demo.o: examples/rich_listview_demo/main.c \
	src/rich_listview/rich_listview.h \
	src/rich_listview/backends/rlv_backend_amiga_v36.h | $(BUILD_DIR)/examples
	$(CC) $(RLV_CFLAGS) -c -o $@ $<

$(BUILD_DIR)/examples/rich_listview_demo_log.o: examples/rich_listview_demo/main.c \
	src/rich_listview/rich_listview.h \
	src/rich_listview/backends/rlv_backend_amiga_v36.h | $(BUILD_DIR)/examples
	$(CC) $(RLV_LOG_CFLAGS) -c -o $@ $<

$(BUILD_DIR)/examples/rich_listview_demo_bench.o: examples/rich_listview_demo/main.c \
	src/rich_listview/rich_listview.h \
	src/rich_listview/backends/rlv_backend_amiga_v36.h | $(BUILD_DIR)/examples
	$(CC) $(RLV_BENCH_CFLAGS) -c -o $@ $<

$(BUILD_DIR)/examples/rich_listview_demo_console.o: examples/rich_listview_demo/main.c \
	src/rich_listview/rich_listview.h \
	src/rich_listview/backends/rlv_backend_amiga_v36.h | $(BUILD_DIR)/examples
	$(CC) $(RLV_CFLAGS) -DDEMO_ENABLE_CONSOLE -c -o $@ $<

$(RLV_NOSMART_DIR)/examples/rich_listview_demo.o: examples/rich_listview_demo/main.c \
	src/rich_listview/rich_listview.h \
	src/rich_listview/backends/rlv_backend_amiga_v36.h | $(RLV_NOSMART_DIR)/examples
	$(CC) $(RLV_NOSMART_CFLAGS) -c -o $@ $<

$(RLV_SORT_DIR)/examples/rich_listview_demo.o: examples/rich_listview_demo/main.c \
	src/rich_listview/rich_listview.h \
	src/rich_listview/backends/rlv_backend_amiga_v36.h | $(RLV_SORT_DIR)/examples
	$(CC) $(RLV_SORT_CFLAGS) -c -o $@ $<

$(RLV_SORT_LOG_DIR)/examples/rich_listview_demo.o: examples/rich_listview_demo/main.c \
	src/rich_listview/rich_listview.h \
	src/rich_listview/backends/rlv_backend_amiga_v36.h | $(RLV_SORT_LOG_DIR)/examples
	$(CC) $(RLV_SORT_LOG_CFLAGS) -c -o $@ $<

$(RLV_COLRESIZE_DIR)/examples/rich_listview_demo.o: examples/rich_listview_demo/main.c \
	src/rich_listview/rich_listview.h \
	src/rich_listview/backends/rlv_backend_amiga_v36.h | $(RLV_COLRESIZE_DIR)/examples
	$(CC) $(RLV_COLRESIZE_CFLAGS) -c -o $@ $<

$(RLV_SORT_RESIZE_DIR)/examples/rich_listview_demo.o: examples/rich_listview_demo/main.c \
	src/rich_listview/rich_listview.h \
	src/rich_listview/backends/rlv_backend_amiga_v36.h | $(RLV_SORT_RESIZE_DIR)/examples
	$(CC) $(RLV_SORT_RESIZE_CFLAGS) -c -o $@ $<

$(RLV_SORT_RESIZE_LOG_DIR)/examples/rich_listview_demo.o: examples/rich_listview_demo/main.c \
	src/rich_listview/rich_listview.h \
	src/rich_listview/backends/rlv_backend_amiga_v36.h | $(RLV_SORT_RESIZE_LOG_DIR)/examples
	$(CC) $(RLV_SORT_RESIZE_LOG_CFLAGS) -c -o $@ $<

$(RLV_ADAPTIVE_DIR)/examples/rich_listview_demo.o: examples/rich_listview_demo/main.c \
	src/rich_listview/rich_listview.h \
	src/rich_listview/backends/rlv_backend_amiga_v36.h | $(RLV_ADAPTIVE_DIR)/examples
	$(CC) $(RLV_ADAPTIVE_DEMO_CFLAGS) -c -o $@ $<

clean:
	@powershell -Command "if (Test-Path '$(BUILD_DIR)') { Remove-Item -Recurse -Force '$(BUILD_DIR)' }; if (Test-Path '$(BIN_DIR)') { Remove-Item -Recurse -Force '$(BIN_DIR)' }"
