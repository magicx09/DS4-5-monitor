#include <cstring>
#include "test.h"
#include "tusb.h"
#include "config.h"

bool is_dse = false;
bool g_usb_ds4 = false;
bool g_usb_wake_keyboard = false;
static Config_body config{};
const Config_body &get_config() { return config; }
bool usb_power_wake_enabled() { return false; }
void bt_get_addr(uint8_t *out) { const uint8_t mac[6] = {1,2,3,4,5,6}; std::memcpy(out, mac, 6); }
void slot_get_addr(int, uint8_t *out) { bt_get_addr(out); }
uint8_t board_usb_get_serial(uint16_t *out, uint8_t cap) {
    if (cap < 4) return 0;
    out[0]='1'; out[1]='2'; out[2]='3'; out[3]='4'; return 4;
}

TEST(wired_ds5_and_edge_have_no_serial_and_ds4_identity_is_preserved) {
    g_usb_wake_keyboard = false;
    for (int pass = 0; pass < 2; ++pass) {
        // Switch personas in both directions, using the same mutable descriptor.
        g_usb_ds4 = true;
        auto d = reinterpret_cast<const tusb_desc_device_t *>(tud_descriptor_device_cb());
        CHECK_EQ(d->idVendor, 0x054C); CHECK_EQ(d->idProduct, 0x09CC);
        CHECK_EQ(d->iSerialNumber, 3);
        CHECK(tud_descriptor_string_cb(3, 0x0409) != nullptr);
        g_usb_ds4 = false;
        for (uint8_t mode : {0, 1, 2}) {
            config.controller_mode = mode;
            is_dse = pass != 0;
            d = reinterpret_cast<const tusb_desc_device_t *>(tud_descriptor_device_cb());
            CHECK_EQ(d->idProduct, (mode == 1 || (mode == 2 && is_dse)) ? 0x0DF2 : 0x0CE6);
            CHECK_EQ(d->iSerialNumber, 0);
            CHECK(tud_descriptor_string_cb(3, 0x0409) == nullptr);
        }
    }
    g_usb_wake_keyboard = true;
    auto d = reinterpret_cast<const tusb_desc_device_t *>(tud_descriptor_device_cb());
    CHECK_EQ(d->idVendor, 0x2E8A); CHECK_EQ(d->iSerialNumber, 3);
    CHECK(tud_descriptor_string_cb(3, 0x0409) != nullptr);
    g_usb_wake_keyboard = false;
}

TEST(native_audio_topology_retains_four_channel_pcm_and_game_hid) {
    g_usb_wake_keyboard = false; g_usb_ds4 = false;
    for (uint8_t mode : {0, 1}) {
        config.controller_mode = mode;
        const auto p = tud_descriptor_configuration_cb(0);
        const unsigned total = p[2] | (p[3] << 8);
        CHECK_EQ(total, 227); CHECK_EQ(p[4], 4);
        unsigned itf = 255, alt = 0, formats = 0, hid = 0;
        for (unsigned pos = 9; pos < total;) {
            const unsigned n = p[pos];
            CHECK(n >= 2 && pos + n <= total);
            if (n < 2 || pos + n > total) break;
            const auto d = p + pos;
            if (d[1] == 4) { itf = d[2]; alt = d[3]; }
            if (itf == 1 && alt == 1 && d[1] == 0x24 && d[2] == 2) {
                CHECK_EQ(n, 11); CHECK_EQ(d[4], 4); CHECK_EQ(d[5], 2);
                CHECK_EQ(d[6], 16); CHECK_EQ(d[8] | (d[9] << 8) | (d[10] << 16), 48000);
                ++formats;
            }
            if (itf == 3 && d[1] == 0x21) {
                CHECK_EQ(d[7] | (d[8] << 8), mode == 0 ? 289 : 437);
                ++hid;
            }
            pos += n;
        }
        CHECK_EQ(formats, 1); CHECK_EQ(hid, 1);
    }
}
