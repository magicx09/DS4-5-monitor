// DS4 playback: USB 32 kHz stereo S16 -> SBC -> fixed 462-byte BT 0x17.
// No microphone stream. Reference: snipem/DS4Dongle 2b1e3078.
#include "audio.h"
#include "pico/time.h"
#if ENABLE_DS4
#include "bt.h"
#include "config.h"
#include "ds4_protocol.h"
#include "core0_ram.h"
#include "tusb.h"
#include "usb.h"
#include "pico/util/queue.h"
#include "classic/btstack_sbc_bluedroid.h"
#include <atomic>
#include <cmath>
#include <cstring>

extern uint8_t interrupt_in_data[63]; // DS4 view: [53] bit0 = headset inserted
namespace {
constexpr unsigned kSamples = 128 * 2;
constexpr unsigned kDepth = 8;
struct PcmBlock { uint32_t generation; int16_t data[kSamples]; };
struct SbcFrame { uint32_t generation; uint8_t data[ds4::kSbcFrameBytes]; };
queue_t pcm_fifo, sbc_fifo;
btstack_sbc_encoder_bluedroid_t encoder_context{};
const btstack_sbc_encoder_t *encoder = nullptr; // core1 only
static_assert(std::atomic<uint32_t>::is_always_lock_free);
std::atomic<uint32_t> encoder_state{0}, encoded{0}, encode_errors{0}, sbc_drops{0};
// All remaining state and diagnostic fields are owned by core0.
Ds4AudioDiag diag{};
PcmBlock accumulator{};
uint16_t pcm_pos = 0, report_counter = 0;
uint32_t generation = 1, last_pcm_us = 0;
uint8_t target = 0, sent_target = 0xff, sent_level = 0xff;
uint8_t packed_frames = 0;
uint8_t report[ds4::kAudioReportBytes]{};

void clear_stream() {
    ++generation;
    pcm_pos = 0;
    packed_frames = 0;
    // Bounded even if core1 completes a frame during this drain. Each frame
    // carries a generation, so a late old frame is discarded by the consumer.
    for (unsigned i = 0; i < kDepth && queue_try_remove(&pcm_fifo, nullptr); ++i) {}
    for (unsigned i = 0; i < kDepth && queue_try_remove(&sbc_fifo, nullptr); ++i) {}
}

void send_ready_reports() {
    if (encoder_state.load(std::memory_order_acquire) != 1 || !target || !bt_is_connected()) return;
    SbcFrame frame{};
    // At most two reports per main-loop pass; never spin on a producer.
    for (unsigned i = 0; i < kDepth && queue_try_remove(&sbc_fifo, &frame); ++i) {
        if (frame.generation != generation) continue;
        if (!packed_frames) ds4::audio_header(report, report_counter, target);
        std::memcpy(report + 6 + packed_frames * ds4::kSbcFrameBytes, frame.data, sizeof(frame.data));
        if (++packed_frames == ds4::kAudioFrames) {
            if (bt_ds4_audio_output(report, sizeof(report))) ++diag.queued;
            else ++diag.queue_errors;
            report_counter = static_cast<uint16_t>(report_counter + ds4::kAudioFrames);
            packed_frames = 0;
        }
    }
}
}

void ds4_audio_init() {
    queue_init(&pcm_fifo, sizeof(PcmBlock), kDepth);
    queue_init(&sbc_fifo, sizeof(SbcFrame), kDepth);
}

void ds4_audio_reset() {
    clear_stream();
    sent_target = sent_level = 0xff;
    last_pcm_us = 0;
}

void ds4_audio_controls(bool force) {
    if (!bt_is_ds4() || !bt_is_connected()) return;
    const uint8_t next = ds4::audio_target(get_config().speaker_select, (interrupt_in_data[53] & 1) != 0);
    if (next != target) { clear_stream(); target = next; }
    const uint8_t level = target ? ds4::audio_level(volume[0], mute[0] != 0) : 0;
    diag.target = target;
    diag.amp = level;
    if (force || sent_target != target || sent_level != level) {
        if (bt_ds4_volume(target, level)) {
            sent_target = target; sent_level = level;
            ++diag.control_queued;
        }
        // A full FIFO leaves the cache dirty; retry on the next loop.
    }
}

void __not_in_flash_func(ds4_audio_core1_start)() {
    encoder = btstack_sbc_encoder_bluedroid_init_instance(&encoder_context);
    // BTstack's buffer-length and frame-count accessors describe its internal
    // work buffer/codec configuration, not the final 112-byte SBC frame and
    // 128 PCM samples consumed by encode_signed_16(). Do not use them as a
    // readiness test: the reference DS4Dongle only gates on configure().
    if (!encoder || encoder->configure(&encoder_context, SBC_MODE_STANDARD, 16, 8,
            SBC_ALLOCATION_METHOD_LOUDNESS, 32000, 25, SBC_CHANNEL_MODE_DUAL_CHANNEL) != 0) {
        encoder_state.store(2, std::memory_order_release);
        return;
    }
    encoder_state.store(1, std::memory_order_release);
}

void __not_in_flash_func(ds4_audio_core1_step)() {
    if (encoder_state.load(std::memory_order_relaxed) != 1) return;
    PcmBlock block{};
    if (!queue_try_remove(&pcm_fifo, &block)) return;
    SbcFrame frame{};
    frame.generation = block.generation;
    if (encoder->encode_signed_16(&encoder_context, block.data, frame.data) != 0) {
        encode_errors.store(encode_errors.load(std::memory_order_relaxed) + 1, std::memory_order_relaxed);
        return;
    }
    encoded.store(encoded.load(std::memory_order_relaxed) + 1, std::memory_order_relaxed);
    if (!queue_try_add(&sbc_fifo, &frame)) {
        sbc_drops.store(sbc_drops.load(std::memory_order_relaxed) + 1, std::memory_order_relaxed);
        queue_try_remove(&sbc_fifo, nullptr);
        queue_try_add(&sbc_fifo, &frame);
    }
}

void CORE0_RAM_FUNC(ds4_audio_loop)() {
    if (!bt_is_ds4()) return;
    ds4_audio_controls();
    // Don't send buffered leftovers on a newly resumed host stream.
    const uint32_t now = time_us_32();
    if (last_pcm_us && uint32_t(now - last_pcm_us) > 100000) {
        clear_stream(); last_pcm_us = 0;
    }
    send_ready_reports();
    if (!tud_audio_available()) return;
    static int16_t raw[132 * 8 / sizeof(int16_t)];
    const uint16_t bytes = tud_audio_read(raw, sizeof(raw));
    const uint16_t samples = bytes / sizeof(int16_t);
    if (!samples) return;
    last_pcm_us = now;
    diag.usb_frames += samples / 2;
    diag.input_peak = diag.output_peak = 0;
    // Host volume controls the physical amp only. OLED volume is independent
    // digital trim. Applying host dB here too caused double attenuation.
    const float gain = mute[0] || !target ? 0.0f : std::pow(10.0f, get_config().speaker_volume / 20.0f);
    for (uint16_t i = 0; i + 1 < samples; i += 2) {
        if (raw[i] || raw[i+1]) ++diag.nonzero_frames;
        for (unsigned ch = 0; ch < 2; ++ch) {
            int32_t sample = raw[i + ch];
            const uint32_t magnitude = sample < 0 ? -sample : sample;
            if (magnitude > diag.input_peak) diag.input_peak = magnitude;
            sample = static_cast<int32_t>(sample * gain);
            if (sample > 32767) sample = 32767;
            if (sample < -32768) sample = -32768;
            const uint32_t output_magnitude = sample < 0 ? -sample : sample;
            if (output_magnitude > diag.output_peak) diag.output_peak = output_magnitude;
            if (target && bt_is_connected()) accumulator.data[pcm_pos++] = static_cast<int16_t>(sample);
        }
        if (pcm_pos == kSamples) {
            accumulator.generation = generation;
            if (!queue_try_add(&pcm_fifo, &accumulator)) {
                ++diag.pcm_drops;
                queue_try_remove(&pcm_fifo, nullptr);
                queue_try_add(&pcm_fifo, &accumulator);
            }
            ++diag.pcm_blocks;
            pcm_pos = 0;
        }
    }
    send_ready_reports();
}

Ds4AudioDiag ds4_audio_diagnostics() {
    Ds4AudioDiag out = diag;
    out.encoder_state = encoder_state.load(std::memory_order_acquire);
    out.encoded = encoded.load(std::memory_order_relaxed);
    out.encode_errors = encode_errors.load(std::memory_order_relaxed);
    out.sbc_drops = sbc_drops.load(std::memory_order_relaxed);
    if (!last_pcm_us || uint32_t(time_us_32() - last_pcm_us) > 250000) out.input_peak = out.output_peak = 0;
    return out;
}
#else
void ds4_audio_init() {}
void ds4_audio_loop() {}
void ds4_audio_core1_start() {}
void ds4_audio_core1_step() {}
void ds4_audio_reset() {}
void ds4_audio_controls(bool) {}
Ds4AudioDiag ds4_audio_diagnostics() { return {}; }
#endif
