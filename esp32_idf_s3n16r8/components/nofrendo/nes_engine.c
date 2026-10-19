/* Minibox adapter for the licensed Nofrendo core; see COPYING and COPYING.LIB. */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "nes_engine_internal.h"
#include "nes.h"
#include "nesinput.h"
#include "gui.h"
#include "esp_log.h"

static nes_t *s_machine;
static nesinput_t s_pad = {.type = INP_JOYPAD0};
static uint16_t s_palette[256];
static char s_error[128];
rgb_t gui_pal[GUI_TOTALCOLORS];

extern mapintf_t map0_intf, map1_intf, map2_intf, map3_intf;
extern mapintf_t map4_intf, map7_intf, map66_intf;
mapintf_t *mappers[] = {
    &map0_intf, &map1_intf, &map2_intf, &map3_intf,
    &map4_intf, &map7_intf, &map66_intf, NULL,
};

void nes_engine_set_error(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vsnprintf(s_error, sizeof(s_error), format, args);
    va_end(args);
    ESP_LOGE("nes_core", "%s", s_error);
}

const char *nes_engine_error(void) { return s_error; }

int log_printf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    esp_log_writev(ESP_LOG_DEBUG, "nes_core", format, args);
    va_end(args);
    return 0;
}

void gui_sendmsg(int color, char *format, ...) {
    char text[256];
    va_list args;
    va_start(args, format);
    vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    if (color == GUI_RED) nes_engine_set_error("%s", text);
    else ESP_LOGI("nes_core", "%s", text);
}

void osd_getsoundinfo(sndinfo_t *info) {
    info->sample_rate = NES_AUDIO_RATE;
    info->bps = 16;
}

void vid_setpalette(rgb_t *palette) {
    for (int i = 0; i < 256; ++i) {
        s_palette[i] = ((palette[i].r >> 3) << 11) |
                       ((palette[i].g >> 2) << 5) | (palette[i].b >> 3);
    }
}

esp_err_t nes_engine_open(const char *path) {
    if (s_machine) {
        nes_engine_set_error("A cartridge is already running");
        return ESP_ERR_INVALID_STATE;
    }
    s_error[0] = 0;
    s_machine = nes_create();
    if (!s_machine) {
        nes_engine_set_error("Not enough memory for NES");
        return ESP_ERR_NO_MEM;
    }
    if (nes_insertcart(path, s_machine) != 0) {
        if (!s_error[0]) nes_engine_set_error("Cannot initialize cartridge");
        nes_destroy(&s_machine);
        return ESP_FAIL;
    }
    static bool registered;
    if (!registered) {
        input_register(&s_pad);
        registered = true;
    }
    s_pad.data = 0;
    input_setstrobe(false);
    input_strobe();
    return ESP_OK;
}

esp_err_t nes_engine_close(void) {
    if (!s_machine) return ESP_OK;
    esp_err_t err = nes_cartridge_save();
    nes_destroy(&s_machine);
    return err;
}

void nes_engine_frame(uint8_t controller, bool draw, int16_t *samples) {
    s_pad.data = controller;
    nes_renderframe(draw);
    s_machine->apu->process(samples, NES_FRAME_SAMPLES);
}

const uint8_t *nes_engine_pixels(int *pitch) {
    *pitch = s_machine->vidbuf->pitch;
    return s_machine->vidbuf->line[0];
}

const uint16_t *nes_engine_palette(void) { return s_palette; }
