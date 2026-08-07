#include "pico/stdlib.h"
#include "tusb.h"
#include "bsp/board.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>

// ======================== HID REPORT IDs ============================
#define REPORT_ID_MOUSE      1
#define REPORT_ID_KEYBOARD   2

// ======================== HID Report Descriptor =====================
uint8_t const desc_hid_report[] = {
    TUD_HID_REPORT_DESC_MOUSE(HID_REPORT_ID(REPORT_ID_MOUSE)),
    TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(REPORT_ID_KEYBOARD))
};

// ======================== USB Configuration =========================
#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN)
uint8_t const desc_configuration[] = {
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, CONFIG_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
    TUD_HID_DESCRIPTOR(0, 0, HID_ITF_PROTOCOL_NONE, sizeof(desc_hid_report), 0x81, 64, 10)
};

// ======================== Device Descriptor =========================
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
    .iManufacturer      = 1,
    .iProduct           = 2,
    .iSerialNumber      = 3,
    .bNumConfigurations = 1
};

// ======================== Descriptor Callbacks ======================
uint8_t const * tud_descriptor_device_cb(void) {
    return (uint8_t const *)&desc_device;
}
uint8_t const * tud_descriptor_configuration_cb(uint8_t index) {
    (void)index;
    return desc_configuration;
}
uint16_t const * tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void)langid;
    static uint16_t buffer[32];
    if (index == 0) {
        buffer[0] = (TUSB_DESC_STRING << 8) | 4;
        buffer[1] = 0x0409;
        return buffer;
    }
    const char *str;
    switch (index) {
        case 1: str = "Logitech"; break;
        case 2: str = "Pico Mouse+KB"; break;
        case 3: str = "123456"; break;
        default: return NULL;
    }
    size_t len = strlen(str);
    for (size_t i = 0; i < len; i++) buffer[1+i] = str[i];
    buffer[0] = (TUSB_DESC_STRING << 8) | (2*len + 2);
    return buffer;
}
uint8_t const * tud_hid_descriptor_report_cb(uint8_t instance) {
    (void)instance;
    return desc_hid_report;
}
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t type,
                           uint8_t const *buffer, uint16_t bufsize) {
    (void)instance; (void)report_id; (void)type; (void)buffer; (void)bufsize;
}
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t type,
                               uint8_t *buffer, uint16_t reqlen) {
    (void)instance; (void)report_id; (void)type; (void)buffer; (void)reqlen;
    return 0;
}

// ======================== Send Functions ============================
static void send_mouse(int8_t dx, int8_t dy) {
    uint8_t report[5] = {0, dx, dy, 0, 0};
    tud_hid_report(REPORT_ID_MOUSE, report, sizeof(report));
}
static void send_mouse_click(uint8_t btn) {
    uint8_t report[5] = {btn, 0, 0, 0, 0};
    tud_hid_report(REPORT_ID_MOUSE, report, sizeof(report));
}
static void send_keyboard(uint8_t modifier, uint8_t key) {
    uint8_t report[8] = {modifier, 0, key, 0, 0, 0, 0, 0};
    tud_hid_report(REPORT_ID_KEYBOARD, report, sizeof(report));
}
static void send_keyboard_empty(void) {
    uint8_t report[8] = {0};
    tud_hid_report(REPORT_ID_KEYBOARD, report, sizeof(report));
}

// ======================== Random Helpers ===========================
static int rand_range(int min, int max) {
    return min + rand() % (max - min + 1);
}

// ======================== Mouse Task (Bezier + tremor + click) =====
static void mouse_task(void) {
    static enum {
        IDLE, MOVING, PAUSE, CLICK
    } state = IDLE;
    static uint32_t state_until = 0;
    static int step = 0, total_steps = 0;
    static float start_x = 0, start_y = 0, target_x = 0, target_y = 0;
    static float cur_x = 0, cur_y = 0;
    static uint32_t next_tick = 0;

    uint32_t now = to_ms_since_boot(get_absolute_time());

    // 只有HID就绪才执行
    if (!tud_hid_ready()) return;

    switch (state) {
        case IDLE:
            if (now >= state_until) {
                // 生成随机目标位移（-200~200）
                target_x = rand_range(-200, 200);
                target_y = rand_range(-200, 200);
                // 偶尔拉长距离
                if (rand() % 5 == 0) { target_x *= 2; target_y *= 2; }
                float dist = sqrtf(target_x*target_x + target_y*target_y);
                total_steps = (dist > 150) ? rand_range(20, 40) : rand_range(40, 100);
                start_x = 0; start_y = 0;
                cur_x = 0; cur_y = 0;
                step = 0;
                state = MOVING;
                next_tick = now + 1;
            }
            break;

        case MOVING:
            if (now >= next_tick) {
                float t = (float)step / total_steps;
                // 缓动函数：加速-匀速-减速
                float ease;
                if (t < 0.3) ease = 3 * t * t;
                else if (t < 0.7) ease = 0.5 + (t - 0.3);
                else ease = 1 - (1 - t) * (1 - t);
                float goal_x = target_x * ease;
                float goal_y = target_y * ease;
                int dx = (int)(goal_x - cur_x);
                int dy = (int)(goal_y - cur_y);
                // 微震颤（-1,0,1）
                dx += (rand() % 3) - 1;
                dy += (rand() % 3) - 1;
                if (dx != 0 || dy != 0) {
                    send_mouse(dx, dy);
                }
                cur_x = goal_x;
                cur_y = goal_y;
                step++;
                if (step >= total_steps) {
                    // 20%概率点击
                    if (rand() % 5 == 0) {
                        state = CLICK;
                        next_tick = now + 1;
                    } else {
                        state = PAUSE;
                        state_until = now + rand_range(500, 3000);
                        next_tick = now + 10;
                    }
                } else {
                    // 动态步进间隔（模拟速度变化）
                    int delay;
                    float t_local = (float)step / total_steps;
                    if (t_local < 0.2) delay = rand_range(2, 6);
                    else if (t_local < 0.8) delay = rand_range(1, 4);
                    else delay = rand_range(5, 15);
                    next_tick = now + delay;
                }
            }
            break;

        case PAUSE:
            if (now >= state_until) {
                state = IDLE;
                state_until = now + rand_range(3000, 8000); // 3~8秒间隔
            }
            break;

        case CLICK:
            if (now >= next_tick) {
                // 按下左键
                send_mouse_click(0x01);
                next_tick = now + rand_range(50, 120);
                // 释放左键
                state = PAUSE;
                state_until = now + rand_range(50, 120) + rand_range(800, 4000);
                send_mouse_click(0x00);
            }
            break;
    }
}

// ======================== Keyboard Task (Mac actions) ===============
static void keyboard_task(void) {
    static uint32_t next_action_time = 0;
    static bool action_in_progress = false;
    static uint8_t action_step = 0;
    static uint32_t step_until = 0;

    uint32_t now = to_ms_since_boot(get_absolute_time());

    if (!tud_hid_ready()) return;

    // 如果有动作正在进行，驱动状态机
    if (action_in_progress) {
        if (now < step_until) return;
        // 这里用简单的顺序发送实现，为了保持简洁，只使用非阻塞延迟
        // 但我们用简单的顺序执行加sleep_ms（会短暂阻塞，但键盘动作频率低，影响小）
        // 更好的做法是用状态机，但为简化且键盘动作少，直接顺序执行
        // 注意：为了不影响鼠标，我们会在每个步骤间用较短的sleep，但总阻塞时间小于200ms
        // 这里重新设计为简单顺序执行（因为动作少，且鼠标任务可以容忍短暂停顿）
        // 为了不破坏原有结构，我们用标志控制
        return;
    }

    // 首次运行或重置
    if (next_action_time == 0) {
        next_action_time = now + rand_range(90000, 270000); // 1.5~4.5分钟
        return;
    }

    if (now >= next_action_time && !action_in_progress) {
        // 随机选择动作 0~3
        int action = rand() % 4;
        action_in_progress = true;
        // 执行动作（顺序执行，用sleep_ms）
        switch (action) {
            case 0: { // Cmd+S
                send_keyboard(KEYBOARD_MODIFIER_LEFTGUI, HID_KEY_S);
                sleep_ms(rand_range(30, 60));
                send_keyboard_empty();
                break;
            }
            case 1: { // Cmd+Tab 闪切
                send_keyboard(KEYBOARD_MODIFIER_LEFTGUI, HID_KEY_TAB);
                sleep_ms(rand_range(60, 120));
                send_keyboard(KEYBOARD_MODIFIER_LEFTGUI | KEYBOARD_MODIFIER_LEFTSHIFT, HID_KEY_TAB);
                sleep_ms(rand_range(40, 80));
                send_keyboard_empty();
                break;
            }
            case 2: { // Spotlight 取消
                send_keyboard(KEYBOARD_MODIFIER_LEFTGUI, HID_KEY_SPACE);
                sleep_ms(rand_range(150, 350));
                send_keyboard_empty();
                sleep_ms(rand_range(100, 200));
                send_keyboard(0, HID_KEY_ESCAPE);
                sleep_ms(rand_range(30, 60));
                send_keyboard_empty();
                break;
            }
            case 3: { // 打字纠错 (teh -> the )
                // 输入 t
                send_keyboard(0, HID_KEY_T);
                sleep_ms(rand_range(60, 120));
                send_keyboard_empty();
                sleep_ms(rand_range(50, 100));
                // e
                send_keyboard(0, HID_KEY_E);
                sleep_ms(rand_range(60, 120));
                send_keyboard_empty();
                sleep_ms(rand_range(50, 100));
                // h
                send_keyboard(0, HID_KEY_H);
                sleep_ms(rand_range(60, 120));
                send_keyboard_empty();
                sleep_ms(rand_range(100, 200));
                // 退格
                send_keyboard(0, HID_KEY_BACKSPACE);
                sleep_ms(rand_range(30, 60));
                send_keyboard_empty();
                sleep_ms(rand_range(50, 100));
                // 输入 "the "
                send_keyboard(0, HID_KEY_T);
                sleep_ms(rand_range(60, 120));
                send_keyboard_empty();
                sleep_ms(rand_range(50, 100));
                send_keyboard(0, HID_KEY_H);
                sleep_ms(rand_range(60, 120));
                send_keyboard_empty();
                sleep_ms(rand_range(50, 100));
                send_keyboard(0, HID_KEY_E);
                sleep_ms(rand_range(60, 120));
                send_keyboard_empty();
                sleep_ms(rand_range(50, 100));
                send_keyboard(0, HID_KEY_SPACE);
                sleep_ms(rand_range(30, 60));
                send_keyboard_empty();
                break;
            }
        }
        // 动作结束
        action_in_progress = false;
        // 设置下次动作时间
        next_action_time = now + rand_range(90000, 270000);
        // 额外延迟，让手指“回到鼠标”
        sleep_ms(rand_range(200, 500));
        // 发送一个微小的鼠标移动模拟手回位
        send_mouse(rand_range(-2, 2), rand_range(-2, 2));
    }
}

// ======================== Main ======================================
int main() {
    board_init();
    tusb_init();

    gpio_init(25);
    gpio_set_dir(25, GPIO_OUT);
    gpio_put(25, 1);

    // 初始化随机种子
    srand(to_ms_since_boot(get_absolute_time()));

    // 让鼠标任务从IDLE开始，并设置首次等待
    // mouse_task内部会自己初始化state_until

    uint32_t last_led = 0;

    while (1) {
        tud_task();

        uint32_t now = to_ms_since_boot(get_absolute_time());

        // LED心跳
        if (now - last_led > 500) {
            gpio_put(25, !gpio_get(25));
            last_led = now;
        }

        // 执行鼠标任务（非阻塞）
        mouse_task();

        // 执行键盘任务（可能阻塞，因为内部用了sleep_ms，但动作频率低，可以接受）
        // 注意：为了不影响鼠标，键盘动作期间鼠标会暂停，但时间短（<2秒）
        keyboard_task();
    }
    return 0;
}
