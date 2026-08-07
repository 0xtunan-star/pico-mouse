#include "pico/stdlib.h"
#include "tusb.h"
#include <string.h>


#define REPORT_ID_MOUSE    1
#define REPORT_ID_KEYBOARD 2


// HID Report Descriptor
uint8_t const desc_hid_report[] =
{
    TUD_HID_REPORT_DESC_MOUSE(
        HID_REPORT_ID(REPORT_ID_MOUSE)
    ),

    TUD_HID_REPORT_DESC_KEYBOARD(
        HID_REPORT_ID(REPORT_ID_KEYBOARD)
    )
};



#define CONFIG_TOTAL_LEN \
    (TUD_CONFIG_DESC_LEN + 2*TUD_HID_DESC_LEN)



uint8_t const desc_configuration[] =
{
    TUD_CONFIG_DESCRIPTOR(
        1,
        2,
        0,
        CONFIG_TOTAL_LEN,
        TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP,
        100
    ),


    // Mouse Interface

    TUD_HID_DESCRIPTOR(
        0,
        0,
        HID_ITF_PROTOCOL_MOUSE,
        sizeof(desc_hid_report),
        0x81,
        16,
        10
    ),


    // Keyboard Interface

    TUD_HID_DESCRIPTOR(
        1,
        0,
        HID_ITF_PROTOCOL_KEYBOARD,
        sizeof(desc_hid_report),
        0x82,
        16,
        10
    )
};



tusb_desc_device_t const desc_device =
{
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,

    .bcdUSB = 0x0200,

    .bDeviceClass = 0x00,
    .bDeviceSubClass = 0x00,
    .bDeviceProtocol = 0x00,

    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,


    .idVendor = 0x046D,
    .idProduct = 0xC539,

    .bcdDevice = 0x0100,

    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,

    .bNumConfigurations = 1
};



uint8_t const * tud_descriptor_device_cb(void)
{
    return (uint8_t const *)&desc_device;
}



uint8_t const * tud_descriptor_configuration_cb(uint8_t index)
{
    (void)index;
    return desc_configuration;
}



uint8_t const * tud_hid_descriptor_report_cb(uint8_t instance)
{
    (void)instance;

    return desc_hid_report;
}




void tud_hid_set_report_cb(
    uint8_t instance,
    uint8_t report_id,
    hid_report_type_t type,
    uint8_t const *buffer,
    uint16_t bufsize)
{

}



uint16_t tud_hid_get_report_cb(
    uint8_t instance,
    uint8_t report_id,
    hid_report_type_t type,
    uint8_t *buffer,
    uint16_t reqlen)
{
    return 0;
}




void send_mouse(int8_t dx,int8_t dy)
{
    uint8_t report[5];

    report[0]=0;
    report[1]=dx;
    report[2]=dy;
    report[3]=0;
    report[4]=0;


    tud_hid_report(
        REPORT_ID_MOUSE,
        report,
        sizeof(report)
    );
}



void send_keyboard(uint8_t modifier,uint8_t key)
{

    uint8_t report[8]={0};


    report[0]=modifier;
    report[2]=key;


    tud_hid_report(
        REPORT_ID_KEYBOARD,
        report,
        sizeof(report)
    );
}




int main()
{

    board_init();

    tusb_init();



    int step=0;


    while(1)
    {

        tud_task();



        if(tud_hid_ready())
        {


            // 鼠标移动

            int8_t dx =
                3*(step%20-10)/10;


            int8_t dy =
                3*(step%15-7)/10;



            send_mouse(dx,dy);


            step++;



            // 测试键盘 A

            static uint32_t last=0;

            uint32_t now =
                to_ms_since_boot(
                    get_absolute_time()
                );


            if(now-last>5000)
            {

                send_keyboard(
                    0,
                    HID_KEY_A
                );


                sleep_ms(50);


                send_keyboard(
                    0,
                    0
                );


                last=now;

            }


        }


        sleep_ms(20);

    }

}
