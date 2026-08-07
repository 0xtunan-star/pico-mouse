#include "pico/stdlib.h"
#include "tusb.h"


#define REPORT_ID_MOUSE     1
#define REPORT_ID_KEYBOARD  2


// =========================
// HID Report Descriptor
// =========================

uint8_t const desc_hid_report[] =
{
    // Mouse
    TUD_HID_REPORT_DESC_MOUSE(
        HID_REPORT_ID(REPORT_ID_MOUSE)
    ),


    // Keyboard
    TUD_HID_REPORT_DESC_KEYBOARD(
        HID_REPORT_ID(REPORT_ID_KEYBOARD)
    )
};



// =========================
// Device Descriptor
// =========================

tusb_desc_device_t const desc_device =
{
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



// =========================
// Configuration Descriptor
// =========================


enum
{
    ITF_NUM_MOUSE,
    ITF_NUM_KEYBOARD,

    ITF_NUM_TOTAL
};


#define CONFIG_TOTAL_LEN \
    (TUD_CONFIG_DESC_LEN + \
     TUD_HID_DESC_LEN + \
     TUD_HID_DESC_LEN)



uint8_t const desc_configuration[] =
{

    TUD_CONFIG_DESCRIPTOR(
        1,
        ITF_NUM_TOTAL,
        0,
        CONFIG_TOTAL_LEN,
        TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP,
        100
    ),



    // HID Mouse Interface 0

    TUD_HID_DESCRIPTOR(
        ITF_NUM_MOUSE,
        0,
        HID_ITF_PROTOCOL_MOUSE,
        sizeof(desc_hid_report),
        0x81,
        16,
        10
    ),



    // HID Keyboard Interface 1

    TUD_HID_DESCRIPTOR(
        ITF_NUM_KEYBOARD,
        0,
        HID_ITF_PROTOCOL_KEYBOARD,
        sizeof(desc_hid_report),
        0x82,
        16,
        10
    )
};




// =========================
// Descriptor callbacks
// =========================


const uint8_t* tud_descriptor_device_cb(void)
{
    return (const uint8_t*)&desc_device;
}



const uint8_t* tud_descriptor_configuration_cb(uint8_t index)
{
    (void)index;

    return desc_configuration;
}




const uint16_t* tud_descriptor_string_cb(
    uint8_t index,
    uint16_t langid
)
{
    static uint16_t str[32];


    if(index == 0)
    {
        str[0] = (TUSB_DESC_STRING << 8) | 4;
        str[1] = 0x0409;

        return str;
    }


    const char* text = NULL;


    switch(index)
    {
        case 1:
            text = "Logitech";
            break;


        case 2:
            text = "Pico Mouse Keyboard";
            break;


        case 3:
            text = "000001";
            break;


        default:
            return NULL;
    }



    uint8_t len = strlen(text);


    for(uint8_t i = 0; i < len; i++)
    {
        str[1+i] = text[i];
    }


    str[0] =
        (TUSB_DESC_STRING << 8)
        |
        (2*len + 2);



    return str;
}




const uint8_t* tud_hid_descriptor_report_cb(
    uint8_t instance
)
{
    (void)instance;

    return desc_hid_report;
}





void tud_hid_set_report_cb(
    uint8_t instance,
    uint8_t report_id,
    hid_report_type_t type,
    const uint8_t* buffer,
    uint16_t bufsize
)
{

}



uint16_t tud_hid_get_report_cb(
    uint8_t instance,
    uint8_t report_id,
    hid_report_type_t type,
    uint8_t* buffer,
    uint16_t reqlen
)
{
    return 0;
}





// =========================
// Send Mouse
// =========================

void send_mouse(int8_t x, int8_t y)
{

    uint8_t report[5] =
    {
        0,
        x,
        y,
        0,
        0
    };


    tud_hid_report(
        ITF_NUM_MOUSE,
        REPORT_ID_MOUSE,
        report,
        sizeof(report)
    );

}





// =========================
// Send Keyboard
// =========================

void send_keyboard(uint8_t key)
{

    uint8_t report[8] =
    {
        0,
        0,
        key,
        0,
        0,
        0,
        0,
        0
    };


    tud_hid_report(
        ITF_NUM_KEYBOARD,
        REPORT_ID_KEYBOARD,
        report,
        sizeof(report)
    );

}





// =========================
// Main
// =========================


int main()
{

    board_init();

    tusb_init();



    gpio_init(25);

    gpio_set_dir(
        25,
        GPIO_OUT
    );



    int step = 0;


    uint32_t last_key = 0;



    while(1)
    {

        tud_task();



        uint32_t now =
            to_ms_since_boot(
                get_absolute_time()
            );



        static uint32_t led = 0;



        if(now - led > 500)
        {
            gpio_put(
                25,
                !gpio_get(25)
            );

            led = now;
        }




        if(tud_hid_ready())
        {


            // 鼠标移动

            int8_t dx =
                3 * (step % 20 - 10) / 10;


            int8_t dy =
                3 * (step % 15 - 7) / 10;



            send_mouse(
                dx,
                dy
            );


            step++;



            // 每5秒空格

            if(now - last_key > 5000)
            {

                send_keyboard(
                    HID_KEY_SPACE
                );


                sleep_ms(50);



                send_keyboard(
                    0
                );



                last_key = now;

            }



        }



        sleep_ms(20);

    }

}
