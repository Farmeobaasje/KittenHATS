/*
 * Copyright (c) 2025 KittenHATS Contributors
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * KittenHATS — Entry point: splash + home screen + chainload.
 *
 * Boot flow:
 *   1. Hardware init
 *   2. display_init_window_a_pitch() — configureer Window A pitch-mode
 *   3. kh_splash_show() — laad splash van SD naar framebuffer
 *   4. UPDATE/ACT_REQ cycle — publiceer framebuffer naar displaycontroller
 *   5. Splash ~5s zichtbaar (500ms intern + 4500ms extern)
 *   6. kh_home_screen() — LVGL home screen met 2 knoppen
 *      - Retourneert KH_UI_ACTION_KIDS of KH_UI_ACTION_CLEAN
 *   7. Vertaal actie naar boot mode
 *   8. chainload_hekatos(mode) — laad en start Hekatos/Nyx
 *   9. Bij fout: toon foutkleur en hang
 *
 * Foutkleuren (A8B8G8R8):
 *   Paars   = SD mount mislukt
 *   Geel    = splash.bin / hekatos.bin niet gevonden
 *   Cyaan   = ongeldige header/grootte/dimensies/stride
 *   Blauw   = header lezen mislukt
 *   Wit     = scanline lezen mislukt
 *   Magenta = malloc mislukt
 *   Oranje  = bootcfg error
 *   Rood    = onbekende fout / ongeldige grootte / ongeldige mode
 */

#include <string.h>
#include <stdlib.h>

#include <bdk.h>

#include "boot/chainload.h"
#include "ui/splash.h"
#include "ui/screen_home.h"

/* KittenHATS version info — same section as hekate's ipl_ver for compatibility */
#define KH_VER_MAJOR 0
#define KH_VER_MINOR 1
#define KH_VER_HOTFX 0

/* Magic: "ICTC" — same as hekate, so chainloaders recognise this as a valid payload */
#define KH_MAGIC 0x43544349

/* Diagnosekleuren in A8B8G8R8 (Tegra X1 native) */
#define KH_DIAG_GREEN   0xFF00FF00
#define KH_DIAG_PURPLE  0xFFFF00FF
#define KH_DIAG_YELLOW  0xFF00FFFF
#define KH_DIAG_CYAN    0xFFFFFF00
#define KH_DIAG_BLUE    0xFFFF0000
#define KH_DIAG_WHITE   0xFFFFFFFF
#define KH_DIAG_ORANGE  0xFF2288FF
#define KH_DIAG_RED     0xFF0000FF

/* Forward declarations */
static void _kh_init_hardware(void);
static void _kh_show_splash_error(kh_splash_result_t result);
static void _kh_show_chainload_error(kh_chainload_result_t result);

/* Boot config storage — placed in ._boot_cfg section for hekate compatibility */
boot_cfg_t __attribute__((section ("._boot_cfg"))) b_cfg;

/* IPL version metadata — placed in ._ipl_version section */
const volatile ipl_ver_meta_t __attribute__((section ("._ipl_version"))) ipl_ver = {
	.magic   = KH_MAGIC,
	.version = (KH_VER_MAJOR + '0') | ((KH_VER_MINOR + '0') << 8) | ((KH_VER_HOTFX + '0') << 16),
	.rcfg.rsvd_flags   = 0,
	.rcfg.bclk_t210    = BPMP_CLK_LOWER_BOOST,
	.rcfg.bclk_t210b01 = BPMP_CLK_DEFAULT_BOOST
};

void kh_main(void)
{
	kh_splash_result_t splash_result;
	kh_chainload_result_t chainload_result;
	kh_ui_action_t ui_action;
	kh_boot_mode_t boot_mode;

	/* 1. Hardware init: clocks, PMC, pinmux, DRAM, display, heap */
	_kh_init_hardware();

	/*
	 * 2. Configure Window A pitch-mode on IPL_FB_ADDRESS.
	 *    Geen display_color_screen() — dat zou one-color mode activeren
	 *    en Window A uitschakelen. Direct naar pitch-mode.
	 */
	display_init_window_a_pitch();

	/*
	 * 2b. Enable backlight.
	 *     display_init() initialiseert de paneelhardware maar schakelt
	 *     de backlight niet in. display_color_screen() deed dat eerder
	 *     impliciet, maar die functie wordt niet meer aangeroepen.
	 *     Voor OLED (Aula/Samsung) gebruiken we display_backlight_brightness(),
	 *     voor LCD de eenvoudige display_backlight(true).
	 */
	if (display_get_decoded_panel_id() != PANEL_SAM_AMS699VC01)
		display_backlight(true);
	else
		display_backlight_brightness(150, 0);

	/* 3. Show splash screen — load from SD */
	splash_result = kh_splash_show();

	/* 4. Handle splash result */
	if (splash_result != KH_SPLASH_OK)
	{
		/* Show error color and hang */
		_kh_show_splash_error(splash_result);
		while (1)
			;
	}

	/*
	 * 5. Splash was displayed successfully.
	 *    kh_splash_show() heeft pixels naar IPL_FB_ADDRESS geschreven,
	 *    maar de displaycontroller heeft de framebuffer nog niet
	 *    herladen sinds display_init_window_a_pitch() (die de buffer
	 *    naar 0 zette). We moeten een UPDATE/ACT_REQ cycle sturen
	 *    voor Window A zodat de controller de nieuwe pixels inleest.
	 */
	DISPLAY_A(DC_CMD_STATE_CONTROL) = GENERAL_UPDATE | WIN_A_UPDATE;
	DISPLAY_A(DC_CMD_STATE_CONTROL) = GENERAL_ACT_REQ | WIN_A_ACT_REQ;
	usleep(35000);  /* Wacht 2 frames zodat de update doorgevoerd wordt */

	/*
	 * 6. Wacht ~5s zodat de splash zichtbaar blijft.
	 *    kh_splash_show() bevat intern msleep(500).
	 *    Totaal: ~500ms intern + 4500ms extern = ~5.5s.
	 */
	msleep(4500);

	/*
	 * 7. Show home screen — LVGL UI with two buttons.
	 *    Returns KH_UI_ACTION_KIDS or KH_UI_ACTION_CLEAN.
	 *    On critical error (SD mount failure), returns KH_UI_ACTION_NONE.
	 */
	ui_action = kh_home_screen();

	/* 8. Translate UI action to boot mode */
	switch (ui_action)
	{
	case KH_UI_ACTION_KIDS:
		boot_mode = KH_BOOT_MODE_KIDS;
		break;
	case KH_UI_ACTION_CLEAN:
		boot_mode = KH_BOOT_MODE_CLEAN;
		break;
	default:
		/* Unknown action — show error and hang */
		display_color_screen(KH_DIAG_RED);
		while (1)
			;
	}

	/* 9. Chainload Hekatos with the selected boot mode */
	chainload_result = chainload_hekatos(boot_mode);

	/* Path C: Chainload failed — show error color and hang */
	_kh_show_chainload_error(chainload_result);
	while (1)
		;
}

/* Alias for exception_handlers.S which references ipl_main */
void __attribute__((alias("kh_main"))) ipl_main(void);

static void _kh_init_hardware(void)
{
	/* Full hardware init — same as hekate does at boot */
	hw_init();

	/* Initialize heap at IPL_HEAP_START (0x82000000, 256MB) */
	heap_init((void *)IPL_HEAP_START);

	/* Initialize display */
	display_init();

	/* Enable backlight at full brightness */
	display_backlight_brightness(100, 1000);
}

static void _kh_show_splash_error(kh_splash_result_t result)
{
	u32 color;

	switch (result)
	{
	case KH_SPLASH_SD_MOUNT_FAILED:
		/* Paars: SD mount mislukt */
		color = KH_DIAG_PURPLE;
		break;
	case KH_SPLASH_FILE_NOT_FOUND:
	case KH_SPLASH_OPEN_FAILED:
		/* Geel: splash.bin niet gevonden of niet te openen */
		color = KH_DIAG_YELLOW;
		break;
	case KH_SPLASH_INVALID_SIZE:
	case KH_SPLASH_INVALID_HEADER:
	case KH_SPLASH_INVALID_DIMENSIONS:
	case KH_SPLASH_INVALID_STRIDE:
		/* Cyaan: ongeldige header, grootte, dimensies of stride */
		color = KH_DIAG_CYAN;
		break;
	case KH_SPLASH_HEADER_READ_FAILED:
		/* Blauw: header lezen mislukt */
		color = KH_DIAG_BLUE;
		break;
	case KH_SPLASH_READ_FAILED:
		/* Wit: scanline lezen mislukt */
		color = KH_DIAG_WHITE;
		break;
	case KH_SPLASH_OUT_OF_MEMORY:
		/* Magenta: malloc mislukt */
		color = 0xFFFF0080;
		break;
	default:
		/* Rood: onbekende fout */
		color = KH_DIAG_RED;
		break;
	}

	display_color_screen(color);
}

static void _kh_show_chainload_error(kh_chainload_result_t result)
{
	u32 color;

	switch (result)
	{
	case KH_CHAINLOAD_SD_ERROR:
		color = KH_DIAG_PURPLE;
		break;
	case KH_CHAINLOAD_FILE_NOT_FOUND:
		color = KH_DIAG_YELLOW;
		break;
	case KH_CHAINLOAD_INVALID_SIZE:
	case KH_CHAINLOAD_INVALID_MODE:
		color = KH_DIAG_RED;
		break;
	case KH_CHAINLOAD_OUT_OF_MEMORY:
		color = 0xFFFF0080;
		break;
	case KH_CHAINLOAD_BOOTCFG_ERROR:
		color = KH_DIAG_ORANGE;
		break;
	default:
		color = KH_DIAG_RED;
		break;
	}

	display_color_screen(color);
}
