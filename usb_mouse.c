#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/rosc.h"
#include "tusb.h"
#include "bsp/board.h"
#include "ws2812.pio.h"

// ========================================================================
// 引脚定义
// ========================================================================
#define WS2812_PIN  16
#define LED_PIN     25

// ========================================================================
// 报告 ID
// ========================================================================
#define REPORT_ID_MOUSE    1
#define REPORT_ID_KEYBOARD 2

// ========================================================================
// WS2812 颜色定义
// ========================================================================
#define COLOR_RED    0xff0000
#define COLOR_GREEN  0x00ff00
#define COLOR_BLUE   0x0000ff
#define COLOR_AMBER  0xff8000
#define COLOR_PURPLE 0xff00ff
#define COLOR_OFF    0x000000

// ========================================================================
// HID 报告描述符
// ========================================================================
uint8_t const desc_hid_report[] = {
    TUD_HID_REPORT_DESC_MOUSE(HID_REPORT_ID(REPORT_ID_MOUSE)),
    TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(REPORT_ID_KEYBOARD))
};

// ========================================================================
// 配置描述符（单接口）
// ========================================================================
#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN)
uint8_t const desc_configuration[] = {
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, CONFIG_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
    TUD_HID_DESCRIPTOR(0, 0, HID_ITF_PROTOCOL_NONE, sizeof(desc_hid_report), 0x81, 64, 10)
};

// ========================================================================
// 设备描述符
// ========================================================================
tusb_desc_device_t const desc_device = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,
    .bDeviceClass       = 0x00,
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor           = 0x046D,
    .idProduct          = 0xC539,
    .bcdDevice          = 0x0100,
    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,
    .bNumConfigurations = 0x01
};

// ========================================================================
// 字符串描述符
// ========================================================================
static const char *string_desc_arr[] = {
    (const char[]){0x09, 0x04},
    "Logitech",
    "Pico Mouse+KB",
    "123456"
};

// ========================================================================
// TinyUSB 回调
// ========================================================================
const uint8_t* tud_descriptor_device_cb(void) { return (const uint8_t*)&desc_device; }
const uint8_t* tud_descriptor_configuration_cb(uint8_t index) { (void)index; return desc_configuration; }
const uint16_t* tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void)langid;
    static uint16_t str[32];
    if (index == 0) { memcpy(&str[1], string_desc_arr[0], 2); str[0] = 0x0304; return str; }
    if (index >= sizeof(string_desc_arr)/sizeof(string_desc_arr[0])) return NULL;
    const char *s = string_desc_arr[index];
    size_t len = strlen(s);
    if (len > 30) len = 30;
    for (size_t i=0; i<len; i++) str[1+i] = s[i];
    str[0] = (TUSB_DESC_STRING << 8) | (2*len + 2);
    return str;
}
const uint8_t* tud_hid_descriptor_report_cb(uint8_t instance) { (void)instance; return desc_hid_report; }
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t type, const uint8_t* buffer, uint16_t bufsize) {}
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t type, uint8_t* buffer, uint16_t reqlen) { return 0; }

// ========================================================================
// WS2812 驱动
// ========================================================================
static PIO ws2812_pio;
static uint ws2812_sm;
static uint ws2812_offset;

static inline void put_pixel(uint32_t pixel_grb) {
    pio_sm_put_blocking(ws2812_pio, ws2812_sm, pixel_grb << 8u);
}
static inline uint32_t rgb_to_grb(uint32_t rgb) {
    uint8_t r = (rgb >> 16) & 0xff;
    uint8_t g = (rgb >> 8) & 0xff;
    uint8_t b = rgb & 0xff;
    return (g << 16) | (r << 8) | b;
}
static inline uint32_t dim_color(uint32_t rgb, uint8_t shift) {
    uint8_t r = ((rgb >> 16) & 0xff) >> shift;
    uint8_t g = ((rgb >> 8) & 0xff) >> shift;
    uint8_t b = (rgb & 0xff) >> shift;
    return (r << 16) | (g << 8) | b;
}
static void set_led(uint32_t rgb) {
    put_pixel(rgb_to_grb(rgb));
}

static bool ws2812_init(void) {
    bool ok = pio_claim_free_sm_and_add_program_for_gpio_range(
        &ws2812_program, &ws2812_pio, &ws2812_sm, &ws2812_offset,
        WS2812_PIN, 1, true);
    if (!ok) return false;
    ws2812_program_init(ws2812_pio, ws2812_sm, ws2812_offset, WS2812_PIN, 800000, false);
    return true;
}

// ========================================================================
// LED 状态管理
// ========================================================================
typedef enum {
    STATE_BOOT,
    STATE_USB_READY,
    STATE_DISABLED,
    STATE_WORK,
    STATE_REST,
    STATE_ACTIVITY,
    STATE_ERROR
} led_state_t;

static led_state_t current_led_state = STATE_BOOT;

static void set_led_state(led_state_t state) {
    current_led_state = state;
    switch (state) {
        case STATE_BOOT:     set_led(COLOR_AMBER); break;
        case STATE_USB_READY:set_led(COLOR_BLUE);  break;
        case STATE_DISABLED: set_led(dim_color(COLOR_RED, 1)); break;
        case STATE_WORK:     set_led(dim_color(COLOR_GREEN, 1)); break;
        case STATE_REST:     set_led(dim_color(COLOR_AMBER, 2)); break;
        case STATE_ACTIVITY: set_led(COLOR_PURPLE); break;
        case STATE_ERROR:    set_led(COLOR_RED);   break;
    }
}

// ========================================================================
// 全局状态
// ========================================================================
static bool usb_mounted = false;
static bool suspended = false;
static bool enabled = false;
static bool in_work = true;
static uint32_t macro_until = 0;

// ========================================================================
// 发送函数
// ========================================================================
static void send_mouse(uint8_t buttons, int8_t dx, int8_t dy) {
    if (!enabled || suspended) return;
    uint8_t report[5] = {buttons, dx, dy, 0, 0};
    while (!tud_hid_ready()) tud_task();
    tud_hid_report(REPORT_ID_MOUSE, report, 5);
}

static void send_mouse_with_wheel(uint8_t buttons, int8_t dx, int8_t dy, int8_t wheel) {
    if (!enabled || suspended) return;
    uint8_t report[5] = {buttons, dx, dy, wheel, 0};
    while (!tud_hid_ready()) tud_task();
    tud_hid_report(REPORT_ID_MOUSE, report, 5);
}

static void send_keyboard(uint8_t modifier, uint8_t key) {
    if (!enabled || suspended) return;
    uint8_t report[8] = {modifier, 0x00, key, 0, 0, 0, 0, 0};
    while (!tud_hid_ready()) tud_task();
    tud_hid_report(REPORT_ID_KEYBOARD, report, 8);
}

static void send_keyboard_report_full(uint8_t modifier, uint8_t k1, uint8_t k2, uint8_t k3, uint8_t k4, uint8_t k5, uint8_t k6) {
    if (!enabled || suspended) return;
    uint8_t report[8] = {modifier, 0x00, k1, k2, k3, k4, k5, k6};
    while (!tud_hid_ready()) tud_task();
    tud_hid_report(REPORT_ID_KEYBOARD, report, 8);
}

static void send_keyboard_release(void) {
    if (!enabled || suspended) return;
    uint8_t report[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    while (!tud_hid_ready()) tud_task();
    tud_hid_report(REPORT_ID_KEYBOARD, report, 8);
}

// ========================================================================
// 左键点击
// ========================================================================
static void do_left_click(void) {
    send_mouse(0x01, 0, 0);
    sleep_ms(50 + rand() % 30);
    send_mouse(0x00, 0, 0);
    sleep_ms(20);
}

// ========================================================================
// 随机数
// ========================================================================
static int rand_range(int min, int max) {
    return min + rand() % (max - min + 1);
}

// ========================================================================
// 鼠标状态机
// ========================================================================
typedef enum {
    MOUSE_IDLE,
    MOUSE_MOVING,
    MOUSE_PAUSE,
    MOUSE_CLICKING
} mouse_state_t;

static mouse_state_t mouse_state = MOUSE_IDLE;
static uint32_t mouse_state_until = 0;
static int mouse_step = 0, mouse_total_steps = 0;
static float mouse_total_x = 0, mouse_total_y = 0;
static float mouse_cur_x = 0, mouse_cur_y = 0;
static uint32_t mouse_next_tick = 0;
static bool mouse_overshoot_done = false;

// 右侧偏好因子（每次上电随机生成，模拟用户习惯）
static float right_bias = 1.0f;
static float speed_preference = 1.0f;  // 0.8~1.2

static void mouse_task(void) {
    if (!enabled || !usb_mounted || suspended) return;
    if (!tud_hid_ready()) return;

    uint32_t now = to_ms_since_boot(get_absolute_time());
    if (now < mouse_next_tick) return;

    int8_t dx = 0, dy = 0;

    switch (mouse_state) {
        case MOUSE_IDLE:
            if (now > mouse_state_until) {
                // 生成移动目标
                mouse_total_x = rand_range(-200, 200);
                mouse_total_y = rand_range(-200, 200);
                // 右侧偏好（让移动倾向右侧）
                mouse_total_x = mouse_total_x * right_bias;
                if (rand() % 5 == 0) { mouse_total_x *= 2; mouse_total_y *= 2; }
                float dist = sqrtf(mouse_total_x * mouse_total_x + mouse_total_y * mouse_total_y);
                // 速度偏好
                int step_base = (dist > 150) ? rand_range(20, 40) : rand_range(40, 100);
                mouse_total_steps = (int)(step_base * speed_preference);
                if (mouse_total_steps < 10) mouse_total_steps = 10;
                mouse_cur_x = 0; mouse_cur_y = 0;
                mouse_step = 0;
                mouse_overshoot_done = false;
                mouse_state = MOUSE_MOVING;
                if (usb_mounted) set_led_state(STATE_ACTIVITY);
            }
            // 微漂移：idle 状态下偶尔小幅度移动
            static uint32_t last_idle_drift = 0;
            if (now - last_idle_drift > 5000 + rand() % 10000) {
                int8_t drift_dx = (rand() % 3) - 1;
                int8_t drift_dy = (rand() % 3) - 1;
                if (drift_dx || drift_dy) {
                    send_mouse(0, drift_dx, drift_dy);
                }
                last_idle_drift = now;
            }
            break;

        case MOUSE_MOVING: {
            float t = (float)mouse_step / mouse_total_steps;
            float ease;
            if (t < 0.3) ease = 3 * t * t;
            else if (t < 0.7) ease = 0.5 + (t - 0.3);
            else ease = 1 - (1 - t) * (1 - t);

            float target_x = mouse_total_x * ease;
            float target_y = mouse_total_y * ease;

            if (!mouse_overshoot_done && t > 0.9 && rand() % 10 < 3) {
                float overshoot = 1.0f + (rand_range(5, 15) / 100.0f);
                target_x = mouse_total_x * (ease * overshoot);
                target_y = mouse_total_y * (ease * overshoot);
                mouse_overshoot_done = true;
            }
            if (mouse_overshoot_done && t > 0.97) {
                target_x = mouse_total_x;
                target_y = mouse_total_y;
            }

            dx = (int)(target_x - mouse_cur_x);
            dy = (int)(target_y - mouse_cur_y);
            dx += (rand() % 3) - 1;
            dy += (rand() % 3) - 1;

            mouse_cur_x = target_x;
            mouse_cur_y = target_y;

            if (dx != 0 || dy != 0) {
                send_mouse(0, dx, dy);
            }
            mouse_step++;

            if (mouse_step >= mouse_total_steps) {
                if (rand() % 5 == 0) {
                    mouse_state = MOUSE_CLICKING;
                    mouse_next_tick = now + 1;
                } else {
                    mouse_state = MOUSE_PAUSE;
                    mouse_state_until = now + rand_range(500, 5000);
                    mouse_next_tick = now + 10;
                }
            }

            if (t < 0.2)      mouse_next_tick = now + rand_range(2, 6);
            else if (t < 0.8) mouse_next_tick = now + rand_range(1, 4);
            else              mouse_next_tick = now + rand_range(5, 15);
            break;
        }

        case MOUSE_CLICKING:
            do_left_click();
            mouse_state = MOUSE_PAUSE;
            mouse_state_until = now + rand_range(800, 4000);
            mouse_next_tick = now + 10;
            if (usb_mounted) {
                if (in_work) set_led_state(STATE_WORK);
                else set_led_state(STATE_REST);
            }
            break;

        case MOUSE_PAUSE:
            if (now > mouse_state_until) {
                // 30% 概率触发滚轮滚动
                if (rand() % 10 < 3) {
                    int8_t wheel = (rand() % 5) - 2;
                    if (wheel) {
                        send_mouse_with_wheel(0, 0, 0, wheel);
                        sleep_ms(50);
                        send_mouse_with_wheel(0, 0, 0, 0);
                    }
                }
                mouse_state = MOUSE_IDLE;
                mouse_state_until = now + rand_range(2000, 15000);
                if (usb_mounted) {
                    if (in_work) set_led_state(STATE_WORK);
                    else set_led_state(STATE_REST);
                }
            }
            mouse_next_tick = now + rand_range(10, 30);
            break;
    }
}

// ========================================================================
// 键盘状态机
// ========================================================================
typedef enum {
    KB_IDLE,
    KB_WAIT_START,
    KB_SEND_KEY,
    KB_SEND_MOD,
    KB_WAIT_INTERVAL,
    KB_FINISH
} kb_state_t;

typedef struct {
    kb_state_t state;
    uint32_t state_until;
    uint8_t step_index;
    uint8_t total_steps;
    uint8_t steps[12][2];
    uint16_t delays[12];
    bool running;
} kb_ctx_t;

static kb_ctx_t kb = { .state = KB_IDLE, .running = false };
static uint32_t next_kb_time = 0;

static void kb_reset(void) {
    kb.state = KB_IDLE;
    kb.running = false;
    kb.step_index = 0;
    kb.total_steps = 0;
}

static void kb_add_step(uint8_t mod, uint8_t key, uint16_t delay) {
    if (kb.total_steps >= 12) return;
    kb.steps[kb.total_steps][0] = mod;
    kb.steps[kb.total_steps][1] = key;
    kb.delays[kb.total_steps] = delay;
    kb.total_steps++;
}

static void kb_add_mod_step(uint8_t mod, uint8_t key, uint16_t delay) {
    kb_add_step(mod, key, delay);
}

static void kb_build_action(uint8_t type) {
    kb_reset();
    kb_add_step(0, 0, rand_range(150, 400));

    switch (type) {
        case 0: {  // Cmd+S
            kb_add_step(KEYBOARD_MODIFIER_LEFTGUI, HID_KEY_S, rand_range(30,60));
            kb_add_step(0, 0, 50);
            break;
        }
        case 1: {  // Cmd+Tab 闪切
            kb_add_step(KEYBOARD_MODIFIER_LEFTGUI, HID_KEY_TAB, rand_range(60,120));
            kb_add_step(KEYBOARD_MODIFIER_LEFTGUI | KEYBOARD_MODIFIER_LEFTSHIFT, HID_KEY_TAB, rand_range(40,80));
            kb_add_step(0, 0, 50);
            break;
        }
        case 2: {  // Spotlight 取消
            kb_add_step(KEYBOARD_MODIFIER_LEFTGUI, HID_KEY_SPACE, rand_range(150,350));
            kb_add_step(0, 0, rand_range(100,200));
            kb_add_step(0, HID_KEY_ESCAPE, rand_range(30,60));
            kb_add_step(0, 0, 50);
            break;
        }
        case 3: {  // 打字纠错 "teh" -> "the "
            kb_add_step(0, HID_KEY_T, rand_range(60,120));
            kb_add_step(0, 0, rand_range(50,100));
            kb_add_step(0, HID_KEY_E, rand_range(60,120));
            kb_add_step(0, 0, rand_range(50,100));
            kb_add_step(0, HID_KEY_H, rand_range(60,120));
            kb_add_step(0, 0, rand_range(100,200));
            kb_add_step(0, HID_KEY_BACKSPACE, rand_range(30,60));
            kb_add_step(0, 0, rand_range(50,100));
            kb_add_step(0, HID_KEY_T, rand_range(60,120));
            kb_add_step(0, 0, rand_range(50,100));
            kb_add_step(0, HID_KEY_H, rand_range(60,120));
            kb_add_step(0, 0, rand_range(50,100));
            kb_add_step(0, HID_KEY_E, rand_range(60,120));
            kb_add_step(0, 0, rand_range(50,100));
            kb_add_step(0, HID_KEY_SPACE, rand_range(30,60));
            kb_add_step(0, 0, 50);
            break;
        }
        case 4: {  // Cmd+` 切换同应用窗口
            kb_add_step(KEYBOARD_MODIFIER_LEFTGUI, HID_KEY_GRAVE_ACCENT, rand_range(30,60));
            kb_add_step(0, 0, 50);
            break;
        }
        case 5: {  // Cmd+Control+Space 表情面板
            kb_add_step(KEYBOARD_MODIFIER_LEFTGUI | KEYBOARD_MODIFIER_LEFTCTRL, HID_KEY_SPACE, rand_range(80,150));
            kb_add_step(0, 0, 50);
            break;
        }
        case 6: {  // 音量加
            kb_add_step(0, HID_KEY_VOLUME_UP, rand_range(30,60));
            kb_add_step(0, 0, 50);
            break;
        }
        case 7: {  // 方向键滚动（下+上）
            kb_add_step(0, HID_KEY_ARROW_DOWN, rand_range(80,200));
            kb_add_step(0, 0, rand_range(100,300));
            kb_add_step(0, HID_KEY_ARROW_UP, rand_range(80,200));
            kb_add_step(0, 0, 50);
            break;
        }
        default:
            kb_add_step(0, 0, 50);
            break;
    }
}

static void kb_machine(void) {
    if (kb.state == KB_IDLE) return;
    uint32_t now = to_ms_since_boot(get_absolute_time());
    if (now < kb.state_until) return;

    switch (kb.state) {
        case KB_WAIT_START:
            kb.state = KB_SEND_KEY;
            kb.step_index = 0;
            kb.state_until = now + 1;
            break;

        case KB_SEND_KEY: {
            if (kb.step_index >= kb.total_steps) {
                kb.state = KB_FINISH;
                kb.state_until = now + 1;
                return;
            }
            uint8_t mod = kb.steps[kb.step_index][0];
            uint8_t key = kb.steps[kb.step_index][1];
            uint16_t delay = kb.delays[kb.step_index];

            if (mod != 0 || key != 0) {
                if (mod != 0 && key != 0) {
                    send_keyboard(mod, key);
                    kb.state_until = now + 20;
                    kb.state = KB_SEND_MOD;
                } else if (key != 0) {
                    send_keyboard(0, key);
                    kb.state_until = now + 20;
                    kb.state = KB_SEND_KEY;
                    kb.steps[kb.step_index][1] = 0;
                }
            } else {
                send_keyboard_release();
                kb.step_index++;
                kb.state_until = now + delay;
                kb.state = KB_WAIT_INTERVAL;
            }
            break;
        }

        case KB_SEND_MOD: {
            uint8_t mod = kb.steps[kb.step_index][0];
            if (mod != 0) {
                send_keyboard(mod, 0);
                kb.state_until = now + 20;
                kb.state = KB_SEND_KEY;
                kb.steps[kb.step_index][0] = 0;
                kb.steps[kb.step_index][1] = 0;
            }
            break;
        }

        case KB_WAIT_INTERVAL:
            kb.step_index++;
            kb.state = KB_SEND_KEY;
            kb.state_until = now + 1;
            break;

        case KB_FINISH:
            send_keyboard_release();
            kb_reset();
            next_kb_time = to_ms_since_boot(get_absolute_time()) + rand_range(60000, 600000);
            break;

        default:
            kb_reset();
            break;
    }
}

static void keyboard_task(void) {
    if (!enabled || !usb_mounted || suspended) return;
    if (kb.running) {
        kb_machine();
        return;
    }
    uint32_t now = to_ms_since_boot(get_absolute_time());
    if (next_kb_time == 0) {
        next_kb_time = now + rand_range(60000, 600000);
        return;
    }
    if (now >= next_kb_time) {
        uint8_t action = rand() % 8;
        kb_build_action(action);
        kb.running = true;
        kb.state = KB_WAIT_START;
        kb.state_until = now + 1;
    }
}

// ========================================================================
// 工作/休息周期
// ========================================================================
static void macro_task(void) {
    if (!enabled) {
        in_work = true;
        return;
    }
    if (suspended) return;
    uint32_t now = to_ms_since_boot(get_absolute_time());
    if (macro_until == 0) {
        in_work = true;
        macro_until = now + rand_range(300000, 900000);
        if (usb_mounted) set_led_state(STATE_WORK);
        return;
    }
    if (now >= macro_until) {
        in_work = !in_work;
        if (in_work) {
            macro_until = now + rand_range(300000, 900000);
            if (usb_mounted) set_led_state(STATE_WORK);
            mouse_state = MOUSE_IDLE;
            mouse_state_until = now + rand_range(1000, 3000);
        } else {
            macro_until = now + rand_range(120000, 300000);
            if (usb_mounted) set_led_state(STATE_REST);
            mouse_state = MOUSE_PAUSE;
            mouse_state_until = now + 1000000;
        }
    }
}

// ========================================================================
// BOOTSEL 按键处理
// ========================================================================
static void bootsel_task(void) {
    static uint32_t last_poll_ms = 0;
    static bool first_run = true;
    static bool last_level = false;
    static uint32_t last_toggle_ms = 0;

    uint32_t now = to_ms_since_boot(get_absolute_time());
    if (now - last_poll_ms < 10) return;
    last_poll_ms = now;

    bool pressed = (board_button_read() == 0);

    if (first_run) {
        last_level = pressed;
        first_run = false;
        return;
    }

    if (pressed && !last_level) {
        if (now - last_toggle_ms > 300) {
            enabled = !enabled;
            last_toggle_ms = now;

            if (!usb_mounted) {
                set_led_state(STATE_USB_READY);
            } else if (!enabled) {
                set_led_state(STATE_DISABLED);
            } else {
                if (in_work) set_led_state(STATE_WORK);
                else set_led_state(STATE_REST);
            }

            if (!enabled) {
                mouse_state = MOUSE_IDLE;
                mouse_state_until = now + 1000000;
                kb_reset();
            } else {
                mouse_state = MOUSE_IDLE;
                mouse_state_until = now + rand_range(2000, 5000);
                macro_until = 0;
                next_kb_time = 0;
            }
        }
    }
    last_level = pressed;
}

// ========================================================================
// USB 事件回调
// ========================================================================
void tud_mount_cb(void) {
    usb_mounted = true;
    suspended = false;
    if (!enabled) {
        set_led_state(STATE_DISABLED);
    } else {
        if (in_work) set_led_state(STATE_WORK);
        else set_led_state(STATE_REST);
    }
}

void tud_umount_cb(void) {
    usb_mounted = false;
    suspended = false;
    set_led_state(STATE_USB_READY);
}

void tud_suspend_cb(bool remote_wakeup_en) {
    (void)remote_wakeup_en;
    suspended = true;
    set_led_state(STATE_USB_READY);
}

void tud_resume_cb(void) {
    suspended = false;
    if (usb_mounted) {
        if (!enabled) set_led_state(STATE_DISABLED);
        else if (in_work) set_led_state(STATE_WORK);
        else set_led_state(STATE_REST);
    } else {
        set_led_state(STATE_USB_READY);
    }
}

// ========================================================================
// 主函数
// ========================================================================
int main(void) {
    board_init();

    if (!ws2812_init()) {
        set_led(COLOR_RED);
        while (1) tight_loop_contents();
    }
    set_led_state(STATE_BOOT);

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, 1);

    // 硬件随机种子
    uint32_t seed = rosc_hw->randombit;
    seed ^= (uint32_t)to_ms_since_boot(get_absolute_time());
    seed ^= (uint32_t)main;
    srand(seed);

    // 生成个性化偏好（每次上电不同）
    right_bias = 0.9f + (float)(rand() % 40) / 100.0f;  // 0.90~1.30
    speed_preference = 0.8f + (float)(rand() % 40) / 100.0f;  // 0.80~1.20

    tusb_init();

    enabled = false;
    usb_mounted = false;
    suspended = false;
    in_work = true;
    macro_until = 0;
    mouse_state = MOUSE_IDLE;
    mouse_state_until = to_ms_since_boot(get_absolute_time()) + 1000000;
    next_kb_time = 0;
    kb_reset();

    while (1) {
        tud_task();
        bootsel_task();

        if (!suspended) {
            macro_task();
            if (enabled && usb_mounted) {
                if (in_work) {
                    mouse_task();
                    keyboard_task();
                }
            }
        }

        static uint32_t last_led = 0;
        uint32_t now = to_ms_since_boot(get_absolute_time());
        if (now - last_led > 1000) {
            gpio_put(LED_PIN, !gpio_get(LED_PIN));
            last_led = now;
        }

        if (!usb_mounted && current_led_state != STATE_USB_READY) {
            set_led_state(STATE_USB_READY);
        }
    }
}
