#pragma once
#include <cstdint>
bool tud_connect();
bool tud_disconnect();
bool tud_connected();
bool tud_suspended();
bool tud_hid_n_ready(uint8_t);
bool tud_hid_n_report(uint8_t,uint8_t,const void*,uint16_t);
bool tud_remote_wakeup();
