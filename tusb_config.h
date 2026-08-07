#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Board configuration
#define CFG_TUSB_MCU                OPT_MCU_RP2040

// USB Device configuration
#define CFG_TUD_ENABLED             1

// USB Device endpoint count
#define CFG_TUD_ENDPOINT0_SIZE       64

// Disable CDC (Serial) - we only need HID
#define CFG_TUD_CDC                 0
#define CFG_TUD_MSC                 0
#define CFG_TUD_HID                 1

// HID configuration
#define CFG_TUD_HID_EPIN_BUFSIZE    64
#define CFG_TUD_HID_EPOUT_BUFSIZE   64

// RHport mode configuration
#define CFG_TUSB_RHPORT0_MODE       (OPT_MODE_DEVICE | OPT_MODE_FULL_SPEED)

#ifdef __cplusplus
}
#endif

