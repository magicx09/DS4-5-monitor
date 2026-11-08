# Host-side unit tests for the firmware's pure logic, with ASan + UBSan.
# No hardware and no Pico SDK: stubs/ stands in for the SDK headers, fakes.cpp
# for the rest of the firmware (flash is a RAM array). Run: tests/host/run.sh
cmake_minimum_required(VERSION 3.16)
project(ds5_host_tests CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(FW ${CMAKE_CURRENT_SOURCE_DIR}/../..)
set(SAN -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)

add_executable(host_tests
        main.cpp
        fakes.cpp
        test_oled_flush.cpp
        test_oled_pad.cpp
        test_stick_curve.cpp
        test_calib.cpp
        test_remap.cpp
        test_config.cpp
        test_controller_profiles.cpp
        test_cmd.cpp
        test_ctrl_info.cpp
        test_dse.cpp
        test_perf.cpp
        test_version_label.cpp
        test_title_scroll.cpp
        test_fw_report.cpp
        test_haptics_decimate.cpp
        test_grip_wave.cpp
        test_usb_power.cpp
        test_oled_keys.cpp
        test_oled_vpad.cpp
        test_state_override.cpp
        test_audio_levels.cpp
        test_auto_haptics.cpp
        test_speaker_resample.cpp
        test_speaker_mono.cpp
        test_rssi_poll.cpp
        test_signal_screen.cpp
        test_amp_volume.cpp
        test_voice_cue.cpp
        test_ds4.cpp
        test_slots.cpp
        test_scan_policy.cpp
        test_bt_policy.cpp
        test_ctrl_send.cpp
        test_mic_level.cpp
        test_pair_prompt.cpp
        test_oled_toast.cpp
        test_identify_screen.cpp
        test_settings_merge.cpp
        test_settings_menu.cpp
        test_settings_hold.cpp
        test_diag_rows.cpp
        test_mic_session.cpp
        test_config_fixtures.cpp
        test_heap.cpp
        test_fw_identity.cpp
        # The firmware sources under test, unmodified.
        ${FW}/src/calib.cpp
        ${FW}/src/remap.cpp
        ${FW}/src/config.cpp
        ${FW}/src/cmd.cpp
        ${FW}/src/perf.cpp
        ${FW}/src/ctrl_info.cpp
        ${FW}/src/dse.cpp
        ${FW}/src/slots.cpp
)
# stubs/ first so its SDK headers win.
target_include_directories(host_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/stubs ${FW}/src)
target_compile_definitions(host_tests PRIVATE
        ENABLE_COMPANION_VENDOR=1
        ENABLE_SERIAL=0
        ENABLE_VERBOSE=0
        DS5_BOARD_NAME="Host"
        DS5_BOARD_TYPE_ID=1
        SYS_CLOCK_KHZ=150000
        FIRMWARE_VERSION="host-test"
        PICO_PROGRAM_VERSION_STRING="host-test"
        CONFIG_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/config"  # test_config_fixtures.cpp
)
# Same char signedness as the firmware build (CMakeLists.txt: -fsigned-char).
target_compile_options(host_tests PRIVATE -O1 -g -fsigned-char -Wall -Wextra -Wno-unused-parameter ${SAN})
target_link_options(host_tests PRIVATE ${SAN})
if(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        # Windows ASan exports strong new/delete symbols. Count allocations
        # through its supported hooks instead of overriding those symbols.
        target_compile_definitions(host_tests PRIVATE DS5_WINDOWS_ASAN=1)
endif()
find_package(Threads REQUIRED)  # test_perf.cpp: a writer thread stands in for core1
target_link_libraries(host_tests PRIVATE Threads::Threads)

# Test the actual Chinese firmware templates, not a Python layout model.
# Separate executables avoid mixing English/Chinese inline definitions.
enable_testing()
add_test(NAME host_tests COMMAND host_tests)
foreach(row IN ITEMS 14 16)
        set(zh_target oled_zh_prop${row}_tests)
        add_executable(${zh_target} main.cpp test_oled_zh.cpp test_settings_hold.cpp)
        target_include_directories(${zh_target} PRIVATE stubs ${FW}/src)
        target_compile_definitions(${zh_target} PRIVATE OLED_ZH=1 OLED_ROW_HEIGHT=12 OLED_SETTINGS_ROW_HEIGHT=${row})
        target_compile_options(${zh_target} PRIVATE -O1 -g -fsigned-char -Wall -Wextra ${SAN})
        target_link_options(${zh_target} PRIVATE ${SAN})
        add_test(NAME ${zh_target} COMMAND ${zh_target})
endforeach()


# Real USB attachment lifecycle with DS4 enabled, isolated from the DS5 fakes.
add_executable(ds4_usb_tests main.cpp test_ds4_usb_lifecycle.cpp ${FW}/src/usb_power.cpp)
target_include_directories(ds4_usb_tests PRIVATE ds4_stubs stubs ${FW}/src)
target_compile_definitions(ds4_usb_tests PRIVATE ENABLE_DS4=1 ENABLE_SERIAL=0)
target_compile_options(ds4_usb_tests PRIVATE -O1 -g ${SAN})
target_link_options(ds4_usb_tests PRIVATE ${SAN})
add_test(NAME ds4_usb_tests COMMAND ds4_usb_tests)

# Exercise the real state manager: host HID audio initialization must survive
# the delayed full-state replay, while USB controls and session reset still work.
add_executable(native_audio_tests main.cpp test_native_audio_state.cpp ${FW}/src/state_mgr.cpp)
target_include_directories(native_audio_tests PRIVATE stubs ${FW}/src)
target_compile_definitions(native_audio_tests PRIVATE ENABLE_DS4=1)
target_compile_options(native_audio_tests PRIVATE -O1 -g ${SAN})
target_link_options(native_audio_tests PRIVATE ${SAN})
add_test(NAME native_audio_tests COMMAND native_audio_tests)

# Compile the actual descriptor callbacks against the pinned TinyUSB headers.
set(TINYUSB_SOURCE "" CACHE PATH "TinyUSB source directory for descriptor tests")
if(TINYUSB_SOURCE)
        add_executable(usb_descriptor_tests main.cpp test_usb_descriptors.cpp ${FW}/src/usb_descriptors.cpp)
        target_include_directories(usb_descriptor_tests PRIVATE stubs ${FW}/src ${TINYUSB_SOURCE})
        target_compile_definitions(usb_descriptor_tests PRIVATE CFG_TUSB_MCU=OPT_MCU_NONE ENABLE_DS4=1 ENABLE_SERIAL=0 ENABLE_COMPANION_VENDOR=0)
        target_compile_options(usb_descriptor_tests PRIVATE -O1 -g ${SAN})
        target_link_options(usb_descriptor_tests PRIVATE ${SAN})
        add_test(NAME usb_descriptor_tests COMMAND usb_descriptor_tests)
endif()
