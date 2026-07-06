# KittenHATS Makefile
# Spiegelt Hekatos' Makefile-structuur voor BDK/LVGL compatibiliteit.
# Bouwt een standalone payload (kittenhats.bin) die als eerste laadt.

ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

include $(DEVKITARM)/base_rules

################################################################################

IPL_LOAD_ADDR := 0x40008000
IPL_MAGIC := 0x43544349 #"ICTC"

################################################################################

TARGET := kittenhats
BUILDDIR := build
BUILDTDIR := build/$(TARGET)
OUTPUTDIR := dist
SOURCEDIR := src
BDKDIR := vendor/hekate
BDKINC := -I./$(BDKDIR) -I./$(BDKDIR)/bdk -I./$(BDKDIR)/bootloader/gfx -I./$(SOURCEDIR)
VPATH = $(dir ./$(SOURCEDIR)/) $(dir $(wildcard ./$(SOURCEDIR)/*/)) $(dir $(wildcard ./$(SOURCEDIR)/*/*/))
VPATH += $(dir $(wildcard ./$(BDKDIR)/)) $(dir $(wildcard ./$(BDKDIR)/*/)) $(dir $(wildcard ./$(BDKDIR)/*/*/))
VPATH += $(dir $(wildcard ./$(BDKDIR)/*/*/*/)) $(dir $(wildcard ./$(BDKDIR)/*/*/*/*/))

# Splash conversion
SPLASH_SOURCE := ../assets/splash.png
SPLASH_OUTPUT_BIN := dist/splash.bin
SPLASH_CONVERTER := tools/convert_splash.py

# Track compiler flags
TRACK_CFLAGS = $(BUILDTDIR)/.cflags
TRACK_LDFLAGS = $(BUILDTDIR)/.ldflags

# KittenHATS objects — our own code
OBJS =  start main boot/chainload boot/bootcfg ui/splash \
		ui/ui_runtime ui/kh_ui_asset_loader ui/kh_ui_global_assets ui/kh_ui_theme_assets ui/screen_home ui/screen_pin ui/screen_settings ui/screen_lang \
		config/kh_config config/kh_pin_verify config/kh_ui_config

# BDK objects we need to link (hardware init, display, heap, utils, FatFs, input, LVGL)
# These are compiled from vendor/hekate/bdk/
BDK_OBJS = exception_handlers heap gfx
BDK_OBJS += bpmp ccplex clock di i2c irq timer \
		mc sdram minerva smmu \
		gpio pinmux pmc se tsec uart \
		fuse kfuse \
		sdmmc sdmmc_driver emmc sd emummc \
		bq24193 max17050 max7762x max77620-rtc \
		hw_init
BDK_OBJS += btn util config ini dirlist sprintf

BDK_OBJS += ff ffsystem ffunicode diskio

# Input drivers (touch, Joy-Con)
BDK_OBJS += touch joycon regulator_5v

# LVGL core
BDK_OBJS += lv_group lv_indev lv_obj lv_refr lv_style lv_vdb
BDK_OBJS += lv_draw lv_draw_rbasic lv_draw_vbasic lv_draw_arc lv_draw_img
BDK_OBJS += lv_draw_label lv_draw_line lv_draw_rect lv_draw_triangle
BDK_OBJS += lv_hal_disp lv_hal_indev lv_hal_tick
BDK_OBJS += lv_anim lv_area lv_circ lv_color lv_font lv_ll lv_math lv_mem lv_task lv_txt lv_gc
BDK_OBJS += lv_bar lv_btn lv_btnm lv_cb lv_cont lv_ddlist lv_img lv_label lv_line lv_list lv_lmeter lv_mbox
BDK_OBJS += lv_page lv_roller lv_slider lv_sw lv_tabview lv_ta lv_win lv_log lv_imgbtn
BDK_OBJS += lv_theme lv_theme_hekate

# LVGL fonts
BDK_OBJS += interui_20 interui_30 ubuntu_mono hekate_symbol_20 hekate_symbol_30 hekate_symbol_120 lv_font_builtin

OBJS := $(addsuffix .o, $(OBJS))
OBJS := $(addprefix $(BUILDTDIR)/, $(OBJS))
BDK_OBJS := $(addsuffix .o, $(BDK_OBJS))
BDK_OBJS := $(addprefix $(BUILDTDIR)/, $(BDK_OBJS))

# All objects
ALLOBJS := $(OBJS) $(BDK_OBJS)

################################################################################

CUSTOMDEFINES := -DIPL_LOAD_ADDR=$(IPL_LOAD_ADDR) -DBL_MAGIC=$(IPL_MAGIC)
CUSTOMDEFINES += -DBL_VER_MJ=0 -DBL_VER_MN=0 -DBL_VER_HF=1 -DBL_VER_RL=0
CUSTOMDEFINES += -DNYX_VER_MJ=0 -DNYX_VER_MN=0 -DNYX_VER_HF=0 -DNYX_VER_RL=0

# BDK defines.
CUSTOMDEFINES += -DBDK_MALLOC_NO_DEFRAG -DBDK_EMUMMC_ENABLE
CUSTOMDEFINES += -DBDK_WATCHDOG_FIQ_ENABLE -DBDK_RESTART_BL_ON_WDT
CUSTOMDEFINES += -DGFX_INC='"gfx.h"'

# Warnings
WARNINGS := -Wall -Wsign-compare -Wtype-limits -Wno-array-bounds -Wno-stringop-overread -Wno-stringop-overflow

ARCH := -march=armv4t -mtune=arm7tdmi -mthumb -mthumb-interwork $(WARNINGS)
CFLAGS = $(ARCH) -O2 -g -gdwarf-4 -nostdlib -ffunction-sections -fdata-sections -fomit-frame-pointer -fno-inline -std=gnu11 $(CUSTOMDEFINES)
LDFLAGS = $(ARCH) -nostartfiles -lgcc -Wl,--nmagic,--gc-sections -Xlinker --defsym=IPL_LOAD_ADDR=$(IPL_LOAD_ADDR)

################################################################################

.PHONY: all clean splash

all: splash $(TARGET).bin
	@echo "--------------------------------------"
	@echo "$(TARGET) size:"
	$(eval BIN_SIZE = $(shell wc -c < $(OUTPUTDIR)/$(TARGET).bin))
	@echo $(BIN_SIZE)" Bytes"
	@if [ ${BIN_SIZE} -gt 140288 ]; then echo "\e[1;33mSize exceeds limit!\e[0m"; fi
	@echo "--------------------------------------"
	@echo "Built: $(OUTPUTDIR)/$(TARGET).bin"

clean:
	@rm -rf $(BUILDDIR)
	@rm -rf $(OUTPUTDIR)

# Splash conversion: generate KHSP-format splash binary
splash: $(SPLASH_OUTPUT_BIN)

$(SPLASH_OUTPUT_BIN): $(SPLASH_SOURCE) $(SPLASH_CONVERTER)
	@mkdir -p $(OUTPUTDIR)
	@python $(SPLASH_CONVERTER) $(SPLASH_SOURCE) $(SPLASH_OUTPUT_BIN)

$(TARGET).bin: $(BUILDTDIR)/$(TARGET).elf
	@mkdir -p $(OUTPUTDIR)
	@$(OBJCOPY) -S -O binary $< $(OUTPUTDIR)/$@
	@echo "--------------------------------------"

$(BUILDTDIR)/$(TARGET).elf: $(ALLOBJS) $(TRACK_LDFLAGS)
	@echo -ne "\r[100%] Linking $(TARGET).elf\033[K"
	@$(CC) $(LDFLAGS) -T $(SOURCEDIR)/link.ld $(ALLOBJS) -o $@
	@printf "\n$(TARGET) was built with the following flags:\nCFLAGS:  $(CFLAGS)\nLDFLAGS: $(LDFLAGS)\n"

$(BUILDTDIR)/%.o: %.c $(TRACK_CFLAGS) | $(BUILDTDIR)
	@echo "  Building $@"
	@$(CC) $(CFLAGS) $(BDKINC) -MMD -MP -c $< -o $@

$(BUILDTDIR)/%.o: %.S $(TRACK_CFLAGS) | $(BUILDTDIR)
	@echo "  Building $@"
	@$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILDTDIR):
	@mkdir -p "$(BUILDDIR)"
	@mkdir -p "$(BUILDTDIR)"
	@mkdir -p "$(BUILDTDIR)/boot"
	@mkdir -p "$(BUILDTDIR)/ui"
	@mkdir -p "$(BUILDTDIR)/config"
	@mkdir -p "$(OUTPUTDIR)"

# Non objects change detectors.
$(TRACK_CFLAGS): $(BUILDTDIR)
	@echo '$(CFLAGS)' | cmp -s - $@ || echo '$(CFLAGS)' > $@
$(TRACK_LDFLAGS): $(BUILDTDIR)
	@echo '$(LDFLAGS)' | cmp -s - $@ || echo '$(LDFLAGS)' > $@
-include $(OBJS:.o=.d)
