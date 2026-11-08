#include "test.h"
#include "ds4_protocol.h"
#include "ds4_discovery.h"
#include "ds4_descriptor.h"
#include "settings_menu.h"
#include "slots.h"
#include "fakes.h"
#include <array>
#include <cstring>

static void seal(uint8_t *p, size_t n) {
    const uint32_t c = ds4::crc(p, n-4);
    for (int i = 0; i < 4; ++i) p[n-4+i] = c >> (8*i);
}
static std::array<uint8_t,79> packet() {
    std::array<uint8_t,79> p{};
    p[0]=0xa1; p[1]=0x11; p[2]=0xc0;
    ds4::neutral(p.data()+4);
    seal(p.data(),p.size()); return p;
}
TEST(ds4_input_checks_transport_length_crc_and_report_kind) {
    CHECK_EQ(ds4::crc(reinterpret_cast<const uint8_t *>("123456789"),9),0xcbf43926u);
    auto p=packet(); uint8_t out[63];
    for(size_t n=0;n<p.size();++n) CHECK(!ds4::input(p.data(),n,out));
    CHECK(ds4::input(p.data(),p.size(),out));
    CHECK_EQ(out[0],128); CHECK_EQ(out[4],8);
    p[20]^=1; CHECK(!ds4::input(p.data(),p.size(),out));
    seal(p.data(),p.size()); CHECK(ds4::input(p.data(),p.size(),out));
    for(uint8_t id: {0x01,0x12,0x13,0x19,0x31}) {
        p[1]=id; seal(p.data(),p.size()); CHECK(!ds4::input(p.data(),p.size(),out));
    }
    CHECK(!ds4::input(nullptr,79,out));
}
TEST(ds4_controls_view_does_not_corrupt_timestamp_touch_or_motion) {
    uint8_t raw[63]; ds4::neutral(raw);
    raw[0]=11; raw[1]=22; raw[2]=33; raw[3]=44;
    raw[4]=0x28; raw[5]=0x12; raw[6]=0xa3; raw[7]=67; raw[8]=89;
    raw[9]=0x34; raw[10]=0x12;
    for(int i=12;i<24;++i) raw[i]=i+10;
    raw[29]=0x36; raw[32]=1;
    const uint8_t fingers[8]={1,0x23,0x41,0x56,2,0x78,0x92,0xab};
    memcpy(raw+34,fingers,8);
    uint8_t v[63]; ds4::view(raw,v);
    CHECK_EQ(v[4],67); CHECK_EQ(v[5],89); CHECK_EQ(v[7],0x28);
    CHECK_EQ(v[8],0x12); CHECK_EQ(v[9],3); // DS4 counter must never become DS5 Mute
    CHECK_EQ(v[52],0x16); CHECK_EQ(v[53],1);
    CHECK(memcmp(v+15,raw+12,12)==0); CHECK(memcmp(v+32,fingers,8)==0);
    uint8_t copy[63]; memcpy(copy,raw,63); ds4::apply_view(v,copy);
    CHECK(memcmp(raw,copy,63)==0);
    v[0]=100; v[4]=201; v[7]=0x88; v[9]=0xff;
    ds4::apply_view(v,copy);
    CHECK_EQ(copy[0],100); CHECK_EQ(copy[7],201); CHECK_EQ(copy[4],0x88);
    CHECK_EQ(copy[6],0xa3); CHECK_EQ(copy[9],0x34);
    CHECK(memcmp(copy+18,raw+18,45)==0);
}
TEST(ds4_touch_history_is_bounded_by_usb_report_capacity) {
    auto p=packet(); uint8_t out[63],v[63];
    p[36]=4;
    for(int f=0;f<4;++f) { p[4+33+f*9]=f+1; p[4+34+f*9]=f+10; }
    seal(p.data(),p.size()); CHECK(ds4::input(p.data(),p.size(),out));
    CHECK_EQ(out[32],3); CHECK_EQ(out[33],2); CHECK_EQ(out[51],4);
    ds4::view(out,v); CHECK_EQ(v[32],13);
    p[36]=5; seal(p.data(),p.size()); CHECK(!ds4::input(p.data(),p.size(),out));
    ds4::neutral(out); ds4::view(out,v);
    CHECK_EQ(v[32],0x80); CHECK_EQ(v[36],0x80); CHECK(!ds4::active(v));
    v[9]=1; CHECK(ds4::active(v));
}
TEST(ds4_calibration_reorders_ranges_and_handles_aliasing) {
    uint8_t bt[36], usb[36]; for(int i=0;i<36;++i) bt[i]=i;
    ds4::calibration(bt,usb);
    const uint8_t want[12]={6,7,12,13,8,9,14,15,10,11,16,17};
    CHECK(memcmp(usb,bt,6)==0); CHECK(memcmp(usb+6,want,12)==0);
    CHECK(memcmp(usb+18,bt+18,18)==0);
    ds4::calibration(bt,bt); CHECK(memcmp(bt,usb,36)==0);
}
TEST(ds4_output_preserves_rumble_led_blink_and_disables_audio) {
    uint8_t p[31]{}, out[78]; p[0]=0xff; p[3]=77;p[4]=88;
    p[5]=11;p[6]=22;p[7]=33;p[8]=4;p[9]=5;
    CHECK(!ds4::output(p,30,out)); CHECK(!ds4::output(p,32,out));
    CHECK(ds4::output(p,31,out));
    CHECK_EQ(out[0],0x11); CHECK_EQ(out[1],0xc0);CHECK_EQ(out[2],0x20);
    CHECK_EQ(out[3],7); CHECK_EQ(out[6],77);CHECK_EQ(out[7],88);
    CHECK_EQ(out[8],11);CHECK_EQ(out[9],22);CHECK_EQ(out[10],33);
    CHECK_EQ(out[11],4);CHECK_EQ(out[12],5);
    for(int i=34;i<78;++i) CHECK_EQ(out[i],0);
}
TEST(ds4_candidate_does_not_accept_arbitrary_gamepads) {
    CHECK(ds4::candidate_name(reinterpret_cast<const uint8_t *>("Wireless Controller"),19));
    CHECK(!ds4::candidate_name(reinterpret_cast<const uint8_t *>("Wireless Controller clone"),25));
    CHECK(!ds4::candidate_name(nullptr,19));
    CHECK(!ds4::candidate_name(reinterpret_cast<const uint8_t *>("DualSense"),9));
}

TEST(ds4_discovery_probes_gamepads_without_an_eir_name) {
    using ds4::Candidate;
    // Ordinary and RSSI inquiry results both arrive with name_available=0.
    CHECK(ds4::discover(0x002508,nullptr,0,true)==Candidate::ProbeDs4);
    CHECK(ds4::discover(0x000508,nullptr,0,true)==Candidate::ProbeDs4);
    const uint8_t empty[1]={0};
    CHECK(ds4::discover(0x002508,empty,0,true)==Candidate::ProbeDs4);
    CHECK(ds4::discover(0x002508,empty,1,true)==Candidate::ProbeDs4);
    CHECK(ds4::discover(0x002508,nullptr,0,false)==Candidate::Reject);
    CHECK(ds4::discover(0x002540,nullptr,0,true)==Candidate::Reject); // keyboard
    CHECK(ds4::discover(0x002580,nullptr,0,true)==Candidate::Reject); // mouse
    CHECK(ds4::discover(0x000204,nullptr,0,true)==Candidate::Reject); // phone
}

TEST(ds4_discovery_uses_usb_identity_when_eir_supplies_it) {
    using ds4::Candidate;
    CHECK(ds4::discover(0x002508,nullptr,0,true,true,2,0x054c,0x05c4)==Candidate::Ds4);
    CHECK(ds4::discover(0x002508,nullptr,0,true,true,2,0x054c,0x09cc)==Candidate::Ds4);
    CHECK(ds4::discover(0x002508,nullptr,0,true,true,2,0x045e,0x09cc)==Candidate::Reject);
    CHECK(ds4::discover(0x002508,nullptr,0,true,true,2,0x054c,0x0ce6)==Candidate::Reject);
    CHECK(ds4::discover(0x002508,nullptr,0,false,true,2,0x054c,0x09cc)==Candidate::Reject);
}

TEST(ds4_discovery_keeps_named_ds5_and_other_controller_policy) {
    using ds4::Candidate;
    const auto pick=[](const char *s,bool enabled){
        return ds4::discover(0x002508,reinterpret_cast<const uint8_t*>(s),std::strlen(s),enabled);
    };
    CHECK(pick("DualSense Wireless Controller",false)==Candidate::Ds5);
    CHECK(pick("DualSense Edge Wireless Controller",true)==Candidate::Ds5);
    CHECK(pick("Wireless Controller",true)==Candidate::Ds4);
    CHECK(pick("Wireless Controller",false)==Candidate::Reject);
    CHECK(pick("Xbox Wireless Controller",true)==Candidate::Reject);
    CHECK(pick("Wireless Controller clone",true)==Candidate::Reject);
    const uint8_t terminated[]="Wireless Controller";
    CHECK(ds4::discover(0x002508,terminated,sizeof(terminated),true)==Candidate::Ds4);
}
TEST(ds4_slot_kind_persists_without_reassigning_other_slots) {
    fake_flash_erase_all(); g_fake_flash_fail=false; slots_load();
    const uint8_t a[6]={1,2,3,4,5,6},b[6]={6,5,4,3,2,1};
    CHECK(slot_assign(0,a)); CHECK(slot_assign(1,b,true));
    slots_load(); CHECK(!slot_is_ds4(0)); CHECK(slot_is_ds4(1));
    uint8_t addr[6]; slot_get_addr(0,addr); CHECK(memcmp(addr,a,6)==0);
    CHECK(slot_forget(1)); CHECK(!slot_is_ds4(1));
    CHECK(slot_assign(1,b)); CHECK(!slot_is_ds4(1));
}
TEST(ds4_settings_marks_unsupported_items_without_changing_ds5_values) {
    Config_body c{}; settings_menu::Nav nav;nav.in_group=true;nav.group=1;
    settings_menu::View v{c,0,c,0,false,nullptr,true};
    const char *label; char value[20];
    settings_menu::row_text(nav,0,v,label,value,sizeof(value));CHECK(std::strcmp(value,"--")!=0);
    settings_menu::row_text(nav,1,v,label,value,sizeof(value));CHECK(std::strcmp(value,"--")==0);
    CHECK(settings_menu::ds4_item_supported(0));CHECK(settings_menu::ds4_item_supported(6));
    CHECK(!settings_menu::ds4_item_supported(7));CHECK(!settings_menu::ds4_item_supported(16));
    CHECK(settings_menu::ds4_item_supported(17));CHECK(!settings_menu::ds4_item_supported(20));
}
TEST(ds4_hid_descriptor_declares_exact_input_output_and_calibration_lengths) {
    unsigned report_id=0,size=0,count=0;
    unsigned input_bits[256]{},output_bits[256]{},feature_bits[256]{};
    for(size_t i=0;i<sizeof(desc_hid_report_ds4);) {
        const uint8_t prefix=desc_hid_report_ds4[i++];
        const size_t n=(prefix&3)==3?4:prefix&3;
        CHECK(i+n<=sizeof(desc_hid_report_ds4));
        unsigned v=0;for(size_t b=0;b<n;++b)v|=unsigned(desc_hid_report_ds4[i++])<<(8*b);
        switch(prefix&0xfc) {
            case 0x84:report_id=v;break;case 0x74:size=v;break;case 0x94:count=v;break;
            case 0x80:input_bits[report_id]+=size*count;break;
            case 0x90:output_bits[report_id]+=size*count;break;
            case 0xb0:feature_bits[report_id]+=size*count;break;
        }
    }
    CHECK_EQ(input_bits[1],63*8); CHECK_EQ(output_bits[5],31*8);
    CHECK_EQ(feature_bits[2],36*8);CHECK_EQ(feature_bits[0xa3],48*8);
    CHECK_EQ(feature_bits[0xf6],0);
}

#include "flash_layout.h"
#include "hardware/flash.h"
extern uint8_t g_fake_flash[];
TEST(ds4_slot_extension_reads_legacy_home11_without_erasing_pairings) {
    fake_flash_erase_all(); g_fake_flash_fail=false;
    uint8_t *page=g_fake_flash+SLOTS_FLASH_OFFSET;
    const uint32_t magic=0x44533502u;memcpy(page,&magic,4);
    memset(page+4,0,28);
    const uint8_t a[6]={2,4,6,8,10,12},b[6]={1,3,5,7,9,11};
    memcpy(page+4,a,6);page[28]=1;
    uint8_t prefix[32];memcpy(prefix,page,32);
    slots_load();CHECK(slot_occupied(0));CHECK(!slot_is_ds4(0));
    CHECK(memcmp(prefix,page,32)==0);
    CHECK(slot_assign(1,b,true));slots_load();
    CHECK(!slot_is_ds4(0));CHECK(slot_is_ds4(1));
    uint8_t got[6];slot_get_addr(0,got);CHECK(memcmp(got,a,6)==0);
    CHECK_EQ(page[28],1);CHECK_EQ(page[29],1); // old occupied prefix still readable
}

TEST(ds4_firmware_info_has_no_crc_and_all_48_bytes_reach_usb) {
    uint8_t packet[50]{};
    packet[0]=0xa3; packet[1]=0xa3;
    for (unsigned i=2;i<50;++i) packet[i]=static_cast<uint8_t>(i*3+7);
    // Real firmware report layout has no CRC. An arbitrary non-CRC tail
    // must be preserved, not rejected or replaced by four padding bytes.
    CHECK(!ds4::valid_crc(packet,sizeof(packet)));
    CHECK(ds4::feature(packet,sizeof(packet))==ds4::FeatureCheck::Ok);
    uint8_t usb[48]{};
    CHECK_EQ(ds4::firmware_usb(packet+1,49,usb,sizeof(usb)),48);
    CHECK(memcmp(usb,packet+2,48)==0);
    CHECK_EQ(usb[47],packet[49]);
    packet[49]^=0x80;
    CHECK(ds4::feature(packet,sizeof(packet))==ds4::FeatureCheck::Ok);
    CHECK_EQ(ds4::firmware_usb(packet+1,49,usb,sizeof(usb)),48);
    CHECK_EQ(usb[47],packet[49]);
}
TEST(ds4_feature_validation_still_rejects_damaged_calibration) {
    uint8_t packet[42]{};packet[0]=0xa3;packet[1]=0x05;
    for (unsigned i=2;i<38;++i) packet[i]=i;
    seal(packet,sizeof(packet));
    CHECK(ds4::feature(packet,sizeof(packet))==ds4::FeatureCheck::Ok);
    packet[17]^=1;
    CHECK(ds4::feature(packet,sizeof(packet))==ds4::FeatureCheck::Crc);
    packet[17]^=1;packet[41]^=1;
    CHECK(ds4::feature(packet,sizeof(packet))==ds4::FeatureCheck::Crc);
}
TEST(ds4_feature_validation_bounds_transport_and_usb_buffers) {
    uint8_t firmware[51]{};firmware[0]=0xa3;firmware[1]=0xa3;
    for(size_t n=0;n<50;++n) CHECK(ds4::feature(firmware,n)==ds4::FeatureCheck::Length);
    CHECK(ds4::feature(firmware,51)==ds4::FeatureCheck::Length);
    firmware[0]=0xa1;
    CHECK(ds4::feature(firmware,50)==ds4::FeatureCheck::Unsupported);
    firmware[0]=0xa3;firmware[1]=0x06;
    CHECK(ds4::feature(firmware,50)==ds4::FeatureCheck::Unsupported);
    firmware[1]=0x05;
    for(size_t n=0;n<42;++n) CHECK(ds4::feature(firmware,n)==ds4::FeatureCheck::Length);
    CHECK(ds4::feature(firmware,43)==ds4::FeatureCheck::Length);
    CHECK(ds4::feature(nullptr,50)==ds4::FeatureCheck::Length);
    firmware[1]=0xa3;
    uint8_t out[48];memset(out,0x55,sizeof(out));
    CHECK_EQ(ds4::firmware_usb(firmware+1,49,out,47),0);
    CHECK_EQ(ds4::firmware_usb(firmware+1,48,out,48),0);
    CHECK_EQ(ds4::firmware_usb(nullptr,49,out,48),0);
    CHECK_EQ(ds4::firmware_usb(firmware+1,49,nullptr,48),0);
    for(uint8_t v:out)CHECK_EQ(v,0x55);
}
