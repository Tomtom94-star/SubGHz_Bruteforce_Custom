
#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <subghz/subghz.h>
#include <furi_hal.h>

#define MAX_KEY 0xFFFF

typedef enum {
    MODE_BRUTE_FORCE,
    MODE_MANUAL_INPUT,
    MODE_EXIT,
} AppMode;

static uint32_t manual_key = 0;

static void draw_callback(Canvas* canvas, void* ctx) {
    AppMode* mode = ctx;
    char buffer[32];
    canvas_clear(canvas);

    switch(*mode) {
    case MODE_BRUTE_FORCE:
        canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignCenter, "Mode: Brute Force");
        break;
    case MODE_MANUAL_INPUT:
        snprintf(buffer, sizeof(buffer), "Insert key: 0x%04X", manual_key);
        canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignCenter, buffer);
        break;
    case MODE_EXIT:
        canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignCenter, "Exiting...");
        break;
    }
}

static uint32_t input_key(FuriMessageQueue* input_queue, Gui* gui, ViewPort* view_port) {
    manual_key = 0;
    while(1) {
        InputEvent input_event;
        if(furi_message_queue_get(input_queue, &input_event, FuriWaitForever) == FuriStatusOk) {
            if(input_event.type == InputTypePress) {
                switch(input_event.key) {
                    case InputKeyUp:
                        manual_key = (manual_key + 1) & MAX_KEY;
                        break;
                    case InputKeyDown:
                        manual_key = (manual_key - 1) & MAX_KEY;
                        break;
                    case InputKeyBack:
                        return UINT32_MAX; // cancella / esci
                    case InputKeyOk:
                        return manual_key;
                    default:
                        break;
                }
                gui_refresh(gui);
            }
        }
    }
}

int32_t subghz_bruteforce_custom_app(void* p) {
    (void)p;
    FuriMessageQueue* input_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    Gui* gui = furi_record_open("gui");
    ViewPort* view_port = view_port_alloc();

    AppMode mode = MODE_BRUTE_FORCE;

    view_port_draw_callback_set(view_port, draw_callback, &mode);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    SubGhzTxConfig tx_config = {
        .frequency = 433900000,
        .protocol = SubGhzProtocolRaw,
        .modulation = SubGhzModulationASK,
        .bitrate = 2000,
    };

    // Menu semplice per scegliere modalità
    char msg[32];
    int exit = 0;
    while(!exit) {
        // Mostra menu
        mode = MODE_BRUTE_FORCE;
        snprintf(msg, sizeof(msg), "Press OK: Brute Force\nPress Back: Manual Input");
        furi_log_print_format(furi_log_get_default(), FuriLogLevelInfo,
            "SubGHz_Bruteforce", "%s", msg);

        gui_refresh(gui);

        // Attendi input
        InputEvent event;
        if(furi_message_queue_get(input_queue, &event, FuriWaitForever) == FuriStatusOk) {
            if(event.type == InputTypePress) {
                if(event.key == InputKeyOk) {
                    // Brute force completo
                    for(uint32_t key = 0; key <= MAX_KEY; key++) {
                        uint8_t data[2] = {
                            (key >> 8) & 0xFF,
                            key & 0xFF
                        };
                        subghz_tx(&tx_config, data, 2);
                        snprintf(msg, sizeof(msg), "TX: 0x%04X", key);
                        furi_log_print_format(furi_log_get_default(), FuriLogLevelInfo,
                            "SubGHz_Bruteforce", "%s", msg);
                        furi_delay_ms(100);

                        // Aggiorna display
                        mode = MODE_BRUTE_FORCE;
                        gui_refresh(gui);
                    }
                } else if(event.key == InputKeyBack) {
                    // Modalità input manuale
                    mode = MODE_MANUAL_INPUT;
                    gui_refresh(gui);
                    uint32_t key = input_key(input_queue, gui, view_port);
                    if(key != UINT32_MAX) {
                        uint8_t data[2] = {
                            (key >> 8) & 0xFF,
                            key & 0xFF
                        };
                        subghz_tx(&tx_config, data, 2);
                        snprintf(msg, sizeof(msg), "TX manuale: 0x%04X", key);
                        furi_log_print_format(furi_log_get_default(), FuriLogLevelInfo,
                            "SubGHz_Bruteforce", "%s", msg);
                        furi_delay_ms(500);
                    }
                } else if(event.key == InputKeyBack) {
                    exit = 1;
                }
            }
        }
    }

    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_message_queue_free(input_queue);
    furi_record_close("gui");

    return 0;
}
