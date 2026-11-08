// Exercise the real usb_power.cpp with simulated USB and Bluetooth events.
#include "test.h"
#include "usb_power.h"
#include "config.h"
#include <cstdint>

uint64_t g_fake_time_us=0;
bool g_bt_ds4=false;
bool spk_active=false;
uint8_t interrupt_in_data[63]{};
static bool physical_link=false,attached=false,suspended=false;
static uint8_t power_flags=0;
static unsigned attaches=0,detaches=0;
uint8_t config_power_flags(){return power_flags;}
bool bt_is_connected(){return physical_link;}
bool bt_disconnect(){physical_link=false;return true;}
void audio_set_mic_active(bool){}
bool tud_connect(){attached=true;++attaches;return true;}
bool tud_disconnect(){attached=false;++detaches;return true;}
bool tud_connected(){return attached;}
bool tud_suspended(){return suspended;}
bool tud_hid_n_ready(uint8_t){return attached&&!suspended;}
bool tud_hid_n_report(uint8_t,uint8_t,const void*,uint16_t){return attached;}
bool tud_remote_wakeup(){return true;}
void tud_suspend_cb(bool);
void tud_resume_cb();
void tud_mount_cb();

TEST(ds4_usb_persona_changes_only_after_detach_and_resume) {
    usb_power_init();
    g_fake_time_us=10000000;
    physical_link=true;g_bt_ds4=false;
    usb_power_controller(true);usb_power_task();
    CHECK(attached);CHECK(!usb_ds4());CHECK_EQ(attaches,1);
    tud_mount_cb();
    // Reconnect during the grace period, before the old full persona hides.
    usb_power_controller(false);physical_link=false;
    g_bt_ds4=true;physical_link=true;
    usb_power_controller(true);
    CHECK(!usb_ds4());CHECK(attached);
    usb_power_task();CHECK(!attached);CHECK(!usb_ds4());
    g_fake_time_us+=149999;usb_power_task();CHECK(!attached);
    ++g_fake_time_us;usb_power_task();CHECK(attached);CHECK(usb_ds4());
    CHECK_EQ(attaches,2);CHECK_EQ(detaches,1);
    // DS5 reconnects while host is asleep: keep DS4 descriptors until resume.
    suspended=true;tud_suspend_cb(true);
    usb_power_controller(false);g_bt_ds4=false;
    usb_power_controller(true);usb_power_task();
    CHECK(usb_ds4());CHECK(attached);CHECK_EQ(detaches,1);
    suspended=false;tud_resume_cb();usb_power_task();
    CHECK(!attached);CHECK(usb_ds4());
    g_fake_time_us+=150000;usb_power_task();
    CHECK(attached);CHECK(!usb_ds4());CHECK_EQ(attaches,3);CHECK_EQ(detaches,2);
}
