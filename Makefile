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

# Optional crash-safe PROGDIR logger for rich-listview-demo-log only.
RLV_ENABLE_LOGGING ?= 0

RLV_CFLAGS = $(CFLAGS) -DRLV_ENABLE_SMART_SCROLL=$(RLV_ENABLE_SMART_SCROLL) \
	-DRLV_ENABLE_EXPANDABLE_ROWS=$(RLV_ENABLE_EXPANDABLE_ROWS) \
	-DRLV_ENABLE_SORTING=$(RLV_ENABLE_SORTING)
RLV_LOG_CFLAGS = $(CFLAGS) -DRLV_ENABLE_LOGGING \
	-DRLV_ENABLE_SMART_SCROLL=$(RLV_ENABLE_SMART_SCROLL) \
	-DRLV_ENABLE_EXPANDABLE_ROWS=$(RLV_ENABLE_EXPANDABLE_ROWS) \
	-DRLV_ENABLE_SORTING=$(RLV_ENABLE_SORTING)
RLV_BENCH_CFLAGS = $(CFLAGS) -DRLV_ENABLE_BENCHMARKS \
	-DRLV_ENABLE_SMART_SCROLL=$(RLV_ENABLE_SMART_SCROLL) \
	-DRLV_ENABLE_EXPANDABLE_ROWS=$(RLV_ENABLE_EXPANDABLE_ROWS) \
	-DRLV_ENABLE_SORTING=$(RLV_ENABLE_SORTING)
RLV_NOSMART_CFLAGS = $(CFLAGS) -DRLV_ENABLE_SMART_SCROLL=0 \
	-DRLV_ENABLE_EXPANDABLE_ROWS=$(RLV_ENABLE_EXPANDABLE_ROWS) \
	-DRLV_ENABLE_SORTING=$(RLV_ENABLE_SORTING)
RLV_SORT_CFLAGS = $(CFLAGS) -DRLV_ENABLE_SMART_SCROLL=$(RLV_ENABLE_SMART_SCROLL) \
	-DRLV_ENABLE_EXPANDABLE_ROWS=$(RLV_ENABLE_EXPANDABLE_ROWS) \
	-DRLV_ENABLE_SORTING=1
RLV_SORT_LOG_CFLAGS = $(CFLAGS) -DRLV_ENABLE_LOGGING \
	-DRLV_ENABLE_SMART_SCROLL=$(RLV_ENABLE_SMART_SCROLL) \
	-DRLV_ENABLE_EXPANDABLE_ROWS=$(RLV_ENABLE_EXPANDABLE_ROWS) \
	-DRLV_ENABLE_SORTING=1

RLV_LOG_DIR = $(BUILD_DIR)/rich_listview_log
RLV_BENCH_DIR = $(BUILD_DIR)/rich_listview_bench
RLV_NOSMART_DIR = $(BUILD_DIR)/rich_listview_nosmart
RLV_SORT_DIR = $(BUILD_DIR)/rich_listview_sort
RLV_SORT_LOG_DIR = $(BUILD_DIR)/rich_listview_sort_log

# ---------------------------------------------------------------------------
# Explicit object lists (no wildcards)
# ---------------------------------------------------------------------------

RLV_PLATFORM_OBJS = $(BUILD_DIR)/rich_listview/rlv_platform.o

RLV_OBJS = \
	$(BUILD_DIR)/rich_listview/rlv.o \
	$(BUILD_DIR)/rich_listview/rlv_layout.o \
	$(BUILD_DIR)/rich_listview/rlv_wrap.o \
	$(BUILD_DIR)/rich_listview/rlv_render.o \
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

RLV_LIBS = \
	$(RLV_PLATFORM_OBJS) \
	$(RLV_OBJS)

RLV_LOG_OBJS = \
	$(RLV_LOG_DIR)/rlv.o \
	$(RLV_LOG_DIR)/rlv_layout.o \
	$(RLV_LOG_DIR)/rlv_wrap.o \
	$(RLV_LOG_DIR)/rlv_render.o \
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

RLV_LOG_LIBS = \
	$(RLV_PLATFORM_OBJS) \
	$(RLV_LOG_OBJS)

RLV_BENCH_OBJS = \
	$(RLV_BENCH_DIR)/rlv.o \
	$(RLV_BENCH_DIR)/rlv_layout.o \
	$(RLV_BENCH_DIR)/rlv_wrap.o \
	$(RLV_BENCH_DIR)/rlv_render.o \
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

RLV_BENCH_LIBS = \
	$(RLV_BENCH_OBJS)

RLV_NOSMART_PLATFORM_OBJS = $(RLV_NOSMART_DIR)/rlv_platform.o

RLV_NOSMART_OBJS = \
	$(RLV_NOSMART_DIR)/rlv.o \
	$(RLV_NOSMART_DIR)/rlv_layout.o \
	$(RLV_NOSMART_DIR)/rlv_wrap.o \
	$(RLV_NOSMART_DIR)/rlv_render.o \
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

RLV_SORT_LOG_LIBS = \
	$(RLV_SORT_LOG_PLATFORM_OBJS) \
	$(RLV_SORT_LOG_OBJS)

EXAMPLE_OBJ = $(BUILD_DIR)/examples/rich_listview_demo.o
EXAMPLE_LOG_OBJ = $(BUILD_DIR)/examples/rich_listview_demo_log.o
EXAMPLE_BENCH_OBJ = $(BUILD_DIR)/examples/rich_listview_demo_bench.o
EXAMPLE_NOSMART_OBJ = $(RLV_NOSMART_DIR)/examples/rich_listview_demo.o
EXAMPLE_SORT_OBJ = $(RLV_SORT_DIR)/examples/rich_listview_demo.o
EXAMPLE_SORT_LOG_OBJ = $(RLV_SORT_LOG_DIR)/examples/rich_listview_demo.o

# ---------------------------------------------------------------------------
# Targets
# ---------------------------------------------------------------------------

.PHONY: all dirs clean \
	rich-listview-demo rich-listview-demo-log \
	rich-listview-demo-bench rich-listview-demo-nosmart \
	rich-listview-demo-sort rich-listview-demo-sort-log \
	public-header-audit

all: rich-listview-demo

dirs:
	@powershell -Command "New-Item -ItemType Directory -Force -Path '$(BUILD_DIR)/examples','$(BUILD_DIR)/rich_listview/backends','$(RLV_LOG_DIR)/backends','$(RLV_BENCH_DIR)/backends','$(RLV_NOSMART_DIR)/backends','$(RLV_NOSMART_DIR)/examples','$(RLV_SORT_DIR)/backends','$(RLV_SORT_DIR)/examples','$(RLV_SORT_LOG_DIR)/backends','$(RLV_SORT_LOG_DIR)/examples','$(BUILD_DIR)/tests/public_headers','$(BIN_DIR)' | Out-Null"

# Compile-only: public headers must not need private includes.
public-header-audit: dirs \
	$(BUILD_DIR)/tests/public_headers/rlv_public_core.o \
	$(BUILD_DIR)/tests/public_headers/rlv_public_backend.o

$(BUILD_DIR)/tests/public_headers/%.o: tests/public_headers/%.c
	$(CC) $(RLV_CFLAGS) -c -o $@ $<


rich-listview-demo: dirs $(BIN_DIR)/rich-listview-demo
rich-listview-demo-log: dirs $(BIN_DIR)/rich-listview-demo-log
rich-listview-demo-bench: dirs $(BIN_DIR)/rich-listview-demo-bench
rich-listview-demo-nosmart: dirs $(BIN_DIR)/rich-listview-demo-nosmart
rich-listview-demo-sort: dirs $(BIN_DIR)/rich-listview-demo-sort
rich-listview-demo-sort-log: dirs $(BIN_DIR)/rich-listview-demo-sort-log

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
	src/rich_listview/rlv_platform.h \
	src/rich_listview/rlv_bench_internal.h

$(BUILD_DIR)/rich_listview/%.o: src/rich_listview/%.c $(RLV_CORE_HDRS)
	$(CC) $(RLV_CFLAGS) -c -o $@ $<

$(BUILD_DIR)/rich_listview/backends/%.o: src/rich_listview/backends/%.c $(RLV_CORE_HDRS) \
	src/rich_listview/backends/rlv_backend_amiga_v36.h
	$(CC) $(RLV_CFLAGS) -c -o $@ $<

$(RLV_LOG_DIR)/%.o: src/rich_listview/%.c $(RLV_CORE_HDRS)
	$(CC) $(RLV_LOG_CFLAGS) -c -o $@ $<

$(RLV_LOG_DIR)/backends/%.o: src/rich_listview/backends/%.c $(RLV_CORE_HDRS) \
	src/rich_listview/backends/rlv_backend_amiga_v36.h
	$(CC) $(RLV_LOG_CFLAGS) -c -o $@ $<

$(RLV_BENCH_DIR)/%.o: src/rich_listview/%.c $(RLV_CORE_HDRS)
	$(CC) $(RLV_BENCH_CFLAGS) -c -o $@ $<

$(RLV_BENCH_DIR)/backends/%.o: src/rich_listview/backends/%.c $(RLV_CORE_HDRS) \
	src/rich_listview/backends/rlv_backend_amiga_v36.h
	$(CC) $(RLV_BENCH_CFLAGS) -c -o $@ $<

$(RLV_NOSMART_DIR)/%.o: src/rich_listview/%.c $(RLV_CORE_HDRS)
	$(CC) $(RLV_NOSMART_CFLAGS) -c -o $@ $<

$(RLV_NOSMART_DIR)/backends/%.o: src/rich_listview/backends/%.c $(RLV_CORE_HDRS) \
	src/rich_listview/backends/rlv_backend_amiga_v36.h
	$(CC) $(RLV_NOSMART_CFLAGS) -c -o $@ $<

$(RLV_SORT_DIR)/%.o: src/rich_listview/%.c $(RLV_CORE_HDRS)
	$(CC) $(RLV_SORT_CFLAGS) -c -o $@ $<

$(RLV_SORT_DIR)/backends/%.o: src/rich_listview/backends/%.c $(RLV_CORE_HDRS) \
	src/rich_listview/backends/rlv_backend_amiga_v36.h
	$(CC) $(RLV_SORT_CFLAGS) -c -o $@ $<

$(RLV_SORT_LOG_DIR)/%.o: src/rich_listview/%.c $(RLV_CORE_HDRS)
	$(CC) $(RLV_SORT_LOG_CFLAGS) -c -o $@ $<

$(RLV_SORT_LOG_DIR)/backends/%.o: src/rich_listview/backends/%.c $(RLV_CORE_HDRS) \
	src/rich_listview/backends/rlv_backend_amiga_v36.h
	$(CC) $(RLV_SORT_LOG_CFLAGS) -c -o $@ $<

$(BUILD_DIR)/examples/rich_listview_demo.o: examples/rich_listview_demo/main.c \
	src/rich_listview/rich_listview.h \
	src/rich_listview/backends/rlv_backend_amiga_v36.h
	$(CC) $(RLV_CFLAGS) -c -o $@ $<

$(BUILD_DIR)/examples/rich_listview_demo_log.o: examples/rich_listview_demo/main.c \
	src/rich_listview/rich_listview.h \
	src/rich_listview/backends/rlv_backend_amiga_v36.h
	$(CC) $(RLV_LOG_CFLAGS) -c -o $@ $<

$(BUILD_DIR)/examples/rich_listview_demo_bench.o: examples/rich_listview_demo/main.c \
	src/rich_listview/rich_listview.h \
	src/rich_listview/backends/rlv_backend_amiga_v36.h
	$(CC) $(RLV_BENCH_CFLAGS) -c -o $@ $<

$(RLV_NOSMART_DIR)/examples/rich_listview_demo.o: examples/rich_listview_demo/main.c \
	src/rich_listview/rich_listview.h \
	src/rich_listview/backends/rlv_backend_amiga_v36.h
	$(CC) $(RLV_NOSMART_CFLAGS) -c -o $@ $<

$(RLV_SORT_DIR)/examples/rich_listview_demo.o: examples/rich_listview_demo/main.c \
	src/rich_listview/rich_listview.h \
	src/rich_listview/backends/rlv_backend_amiga_v36.h
	$(CC) $(RLV_SORT_CFLAGS) -c -o $@ $<

$(RLV_SORT_LOG_DIR)/examples/rich_listview_demo.o: examples/rich_listview_demo/main.c \
	src/rich_listview/rich_listview.h \
	src/rich_listview/backends/rlv_backend_amiga_v36.h
	$(CC) $(RLV_SORT_LOG_CFLAGS) -c -o $@ $<

clean:
	@powershell -Command "if (Test-Path '$(BUILD_DIR)') { Remove-Item -Recurse -Force '$(BUILD_DIR)' }; if (Test-Path '$(BIN_DIR)') { Remove-Item -Recurse -Force '$(BIN_DIR)' }"
