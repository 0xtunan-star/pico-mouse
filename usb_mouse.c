#include "pico/stdlib.h"

#include "tusb.h"

#include "bsp/board.h"

#include <string.h>


#define REPORT_ID_MOUSE      1
#define REPORT_ID_KEYBOARD   2



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



// Configuration Descriptor

#define CONFIG_TOTAL_LEN \
    (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN)



uint8_t const desc_configuration[] =
{
    TUD_CONFIG_DESCRIPTOR(
        1,
        1,
        0,
        CONFIG_TOTAL_LEN,
        TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP,
        100
    ),


    TUD_HID_DESCRIPTOR(
        0,
        0,
        HID_ITF_PROTOCOL_NONE,
        sizeof(desc_hid_report),
        0x81,
        64,
        10
    )
};



// Device Descriptor


tusb_desc_device_t const desc_device =
{
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,

    .bcdUSB             = 0x0200,

    .bDeviceClass       = 0x00,
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,


    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,


    // Logitech 风格 VID/PID
    .idVendor           = 0x046D,
    .idProduct          = 0xC539,


    .bcdDevice          = 0x0100,


    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,


    .bNumConfigurations = 0x01
};




// USB callbacks


uint8_t const * tud_descriptor_device_cb(void)
{
    return (uint8_t const *)&desc_device;
}



uint8_t const * tud_descriptor_configuration_cb(
        uint8_t index)
{
    (void)index;

    return desc_configuration;
}




uint16_t const * tud_descriptor_string_cb(
        uint8_t index,
        uint16_t langid)
{

    (void)langid;


    static uint16_t buffer[32];


    if(index == 0)
    {
        buffer[0] = 
            (TUSB_DESC_STRING << 8) | 4;

        buffer[1] = 0x0409;

        return buffer;
    }



    const char *str;



    switch(index)
    {

        case 1:
            str = "Logitech";
            break;


        case 2:
            str = "USB Optical Mouse";
            break;


        case 3:
            str = "123456";
            break;


        default:
            return NULL;
    }



    size_t len = strlen(str);


    for(size_t i=0;i<len;i++)
    {
        buffer[1+i] = str[i];
    }



    buffer[0] =
        (TUSB_DESC_STRING << 8)
        |
        (2*len+2);



    return buffer;
}





uint8_t const * tud_hid_descriptor_report_cb(
        uint8_t instance)
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

    (void)instance;
    (void)report_id;
    (void)type;
    (void)buffer;
    (void)bufsize;

}






uint16_t tud_hid_get_report_cb(
        uint8_t instance,
        uint8_t report_id,
        hid_report_type_t type,
        uint8_t *buffer,
        uint16_t reqlen)
{

    (void)instance;
    (void)report_id;
    (void)type;
    (void)buffer;
    (void)reqlen;


    return 0;
}








// Send mouse report


static void send_mouse(
        int8_t dx,
        int8_t dy)
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








// Send keyboard report


static void send_keyboard(
        uint8_t modifier,
        uint8_t key)
{


    uint8_t report[8];


    memset(report,0,sizeof(report));


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



    gpio_init(25);

    gpio_set_dir(
        25,
        GPIO_OUT
    );




    uint32_t last_key_time=0;


    int step=0;



    while(1)
    {

        tud_task();



        uint32_t now =
            to_ms_since_boot(
                get_absolute_time()
            );



        static uint32_t last_led=0;



        if(now-last_led>500)
        {

            gpio_put(
                25,
                !gpio_get(25)
            );


            last_led=now;

        }





        if(tud_hid_ready())
        {


            // 微小鼠标移动

            int8_t dx =
                (step%20)-10;


            int8_t dy =
                (step%15)-7;



            dx/=10;
            dy/=10;



            send_mouse(
                dx,
                dy
            );


            step++;



            sleep_ms(20);





            // 每5秒发送一次空格

            if(now-last_key_time>5000)
            {


                send_keyboard(
                    0,
                    HID_KEY_SPACE
                );


                sleep_ms(50);



                send_keyboard(
                    0,
                    0
                );


                last_key_time=now;

            }


        }

    }


    return 0;
}
