# RichListview — full custom ListView control for classic AmigaOS
#
# Extracted from amiga_custom_listview (custom_listview_control package).
# This Makefile builds ONLY the custom control + demo. It does not include
# the legacy GadTools LISTVIEW_KIND enhancer, ASCII formatter, binders,
# selection adapter, or clv_cellctl implementation.
#
# VBCC +aos68k does NOT predefine __AMIGA__; -DCLV_PLATFORM_AMIGA=1 drives
# the central platform assert. Public API still uses CLV_* / clv_control_*
# names until a later rename phase.

CC := vc
CFLAGS := +aos68k -c99 -cpu=68000 -O2 -size -Isrc -DCLV_PLATFORM_AMIGA=1
LDFLAGS := +aos68k -cpu=68000 -O2 -size -final -lamiga -lauto

BUILD_DIR := build
BIN_DIR := bin

# Smart vertical scrolling (pixel shift + exposed-band regional paint).
# 0 = always full-viewport scroll paint.
CLV_ENABLE_SMART_SCROLL ?= 1

# Optional crash-safe PROGDIR logger for custom-control-demo-log only.
CLV_ENABLE_LOGGING ?= 0

CLV_CTRL_CFLAGS = $(CFLAGS) -DCLV_ENABLE_SMART_SCROLL=$(CLV_ENABLE_SMART_SCROLL)
CLV_CTRL_LOG_CFLAGS = $(CFLAGS) -DCLV_ENABLE_LOGGING -DCLV_ENABLE_SMART_SCROLL=$(CLV_ENABLE_SMART_SCROLL)
CLV_CTRL_BENCH_CFLAGS = $(CFLAGS) -DCLV_ENABLE_BENCHMARKS -DCLV_ENABLE_SMART_SCROLL=$(CLV_ENABLE_SMART_SCROLL)

CLV_CTRL_LOG_DIR = $(BUILD_DIR)/rich_listview_log
CLV_CTRL_BENCH_DIR = $(BUILD_DIR)/rich_listview_bench

# ---------------------------------------------------------------------------
# Explicit object lists (no wildcards)
# ---------------------------------------------------------------------------

CLV_PLATFORM_OBJS = $(BUILD_DIR)/rich_listview/clv_platform.o

CLV_CUSTOM_CONTROL_OBJS = \
	$(BUILD_DIR)/rich_listview/clv_control.o \
	$(BUILD_DIR)/rich_listview/clv_control_layout.o \
	$(BUILD_DIR)/rich_listview/clv_control_wrap.o \
	$(BUILD_DIR)/rich_listview/clv_control_render.o \
	$(BUILD_DIR)/rich_listview/clv_control_checkbox.o \
	$(BUILD_DIR)/rich_listview/clv_control_input.o \
	$(BUILD_DIR)/rich_listview/clv_control_scroll.o \
	$(BUILD_DIR)/rich_listview/backends/clv_backend_amiga_v36.o

CLV_CUSTOM_CONTROL_LIBS = \
	$(CLV_PLATFORM_OBJS) \
	$(CLV_CUSTOM_CONTROL_OBJS)

CLV_CUSTOM_CONTROL_LOG_OBJS = \
	$(CLV_CTRL_LOG_DIR)/clv_control.o \
	$(CLV_CTRL_LOG_DIR)/clv_control_layout.o \
	$(CLV_CTRL_LOG_DIR)/clv_control_wrap.o \
	$(CLV_CTRL_LOG_DIR)/clv_control_render.o \
	$(CLV_CTRL_LOG_DIR)/clv_control_checkbox.o \
	$(CLV_CTRL_LOG_DIR)/clv_control_input.o \
	$(CLV_CTRL_LOG_DIR)/clv_control_scroll.o \
	$(CLV_CTRL_LOG_DIR)/backends/clv_backend_amiga_v36.o \
	$(CLV_CTRL_LOG_DIR)/clv_control_log.o

CLV_CUSTOM_CONTROL_LOG_LIBS = \
	$(CLV_PLATFORM_OBJS) \
	$(CLV_CUSTOM_CONTROL_LOG_OBJS)

CLV_CUSTOM_CONTROL_BENCH_OBJS = \
	$(CLV_CTRL_BENCH_DIR)/clv_control.o \
	$(CLV_CTRL_BENCH_DIR)/clv_control_layout.o \
	$(CLV_CTRL_BENCH_DIR)/clv_control_wrap.o \
	$(CLV_CTRL_BENCH_DIR)/clv_control_render.o \
	$(CLV_CTRL_BENCH_DIR)/clv_control_checkbox.o \
	$(CLV_CTRL_BENCH_DIR)/clv_control_input.o \
	$(CLV_CTRL_BENCH_DIR)/clv_control_scroll.o \
	$(CLV_CTRL_BENCH_DIR)/backends/clv_backend_amiga_v36.o \
	$(CLV_CTRL_BENCH_DIR)/clv_platform.o \
	$(CLV_CTRL_BENCH_DIR)/clv_bench.o

CLV_CUSTOM_CONTROL_BENCH_LIBS = \
	$(CLV_CUSTOM_CONTROL_BENCH_OBJS)

EXAMPLE_CUSTOM_CONTROL_OBJ = $(BUILD_DIR)/examples/custom_control_demo.o
EXAMPLE_CUSTOM_CONTROL_LOG_OBJ = $(BUILD_DIR)/examples/custom_control_demo_log.o
EXAMPLE_CUSTOM_CONTROL_BENCH_OBJ = $(BUILD_DIR)/examples/custom_control_demo_bench.o

# ---------------------------------------------------------------------------
# Targets
# ---------------------------------------------------------------------------

.PHONY: all dirs clean \
	custom-control-demo custom-control-demo-log \
	custom-control-demo-bench custom-control-demo-nosmart

all: custom-control-demo

dirs:
	@powershell -Command "New-Item -ItemType Directory -Force -Path '$(BUILD_DIR)/examples','$(BUILD_DIR)/rich_listview/backends','$(CLV_CTRL_LOG_DIR)/backends','$(CLV_CTRL_BENCH_DIR)/backends','$(BIN_DIR)' | Out-Null"

custom-control-demo: dirs $(BIN_DIR)/custom-control-demo
custom-control-demo-log: dirs $(BIN_DIR)/custom-control-demo-log
custom-control-demo-bench: dirs $(BIN_DIR)/custom-control-demo-bench

$(BIN_DIR)/custom-control-demo: $(CLV_CUSTOM_CONTROL_LIBS) $(EXAMPLE_CUSTOM_CONTROL_OBJ)
	$(CC) $(LDFLAGS) -o $@ $(CLV_CUSTOM_CONTROL_LIBS) $(EXAMPLE_CUSTOM_CONTROL_OBJ)

$(BIN_DIR)/custom-control-demo-log: $(CLV_CUSTOM_CONTROL_LOG_LIBS) $(EXAMPLE_CUSTOM_CONTROL_LOG_OBJ)
	$(CC) $(LDFLAGS) -o $@ $(CLV_CUSTOM_CONTROL_LOG_LIBS) $(EXAMPLE_CUSTOM_CONTROL_LOG_OBJ)

$(BIN_DIR)/custom-control-demo-bench: $(CLV_CUSTOM_CONTROL_BENCH_LIBS) $(EXAMPLE_CUSTOM_CONTROL_BENCH_OBJ)
	$(CC) $(LDFLAGS) -o $@ $(CLV_CUSTOM_CONTROL_BENCH_LIBS) $(EXAMPLE_CUSTOM_CONTROL_BENCH_OBJ)

# Rebuild baseline with smart scroll disabled (reuses same output name).
custom-control-demo-nosmart:
	$(MAKE) custom-control-demo CLV_ENABLE_SMART_SCROLL=0

# ---------------------------------------------------------------------------
# Compile rules
# ---------------------------------------------------------------------------

$(BUILD_DIR)/rich_listview/%.o: src/rich_listview/%.c
	$(CC) $(CLV_CTRL_CFLAGS) -c -o $@ $<

$(BUILD_DIR)/rich_listview/backends/%.o: src/rich_listview/backends/%.c
	$(CC) $(CLV_CTRL_CFLAGS) -c -o $@ $<

$(CLV_CTRL_LOG_DIR)/%.o: src/rich_listview/%.c
	$(CC) $(CLV_CTRL_LOG_CFLAGS) -c -o $@ $<

$(CLV_CTRL_LOG_DIR)/backends/%.o: src/rich_listview/backends/%.c
	$(CC) $(CLV_CTRL_LOG_CFLAGS) -c -o $@ $<

$(CLV_CTRL_BENCH_DIR)/%.o: src/rich_listview/%.c
	$(CC) $(CLV_CTRL_BENCH_CFLAGS) -c -o $@ $<

$(CLV_CTRL_BENCH_DIR)/backends/%.o: src/rich_listview/backends/%.c
	$(CC) $(CLV_CTRL_BENCH_CFLAGS) -c -o $@ $<

$(BUILD_DIR)/examples/custom_control_demo.o: examples/custom_control_demo/main.c
	$(CC) $(CLV_CTRL_CFLAGS) -c -o $@ $<

$(BUILD_DIR)/examples/custom_control_demo_log.o: examples/custom_control_demo/main.c
	$(CC) $(CLV_CTRL_LOG_CFLAGS) -c -o $@ $<

$(BUILD_DIR)/examples/custom_control_demo_bench.o: examples/custom_control_demo/main.c
	$(CC) $(CLV_CTRL_BENCH_CFLAGS) -c -o $@ $<

clean:
	@powershell -Command "if (Test-Path '$(BUILD_DIR)') { Remove-Item -Recurse -Force '$(BUILD_DIR)' }; if (Test-Path '$(BIN_DIR)') { Remove-Item -Recurse -Force '$(BIN_DIR)' }"
