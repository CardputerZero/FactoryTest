/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "audio_service.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#define MINIAUDIO_IMPLEMENTATION
#ifdef APP_MINIAUDIO_HEADER
#include APP_MINIAUDIO_HEADER
#else
#include <miniaudio.h>
#endif

#include "logger.h"

namespace platform::audio {
namespace {

constexpr ma_uint32 K_AUDIO_SAMPLE_RATE = 16000;
constexpr ma_uint32 K_AUDIO_CHANNELS    = 1;
constexpr ma_uint32 K_PLAYBACK_CHANNELS = 2;
constexpr float K_DEFAULT_VOLUME_LEVEL  = 0.5F;

struct WavAudio {
  ma_uint32 sample_rate{0};
  ma_uint32 channels{0};
  std::vector<int16_t> samples;
};

struct CaptureState {
  std::mutex mutex;
  std::condition_variable cv;
  std::vector<int16_t> samples;
  std::size_t target_samples{0};
  bool complete{false};
};

struct PlaybackState {
  std::shared_ptr<WavAudio> audio;
  std::atomic<std::size_t> frame_offset{0};
  std::atomic<bool> complete{false};
  std::mutex mutex;
  std::condition_variable cv;
};

struct SelectedDevice {
  std::string name;
  std::string id;
  bool found{false};
};

struct SelectedAudioDevice {
  AudioDevice info;
  bool playback_found{false};
  bool capture_found{false};
};

struct MiniaudioContext {
  ma_context context{};
  bool initialized{false};

  MiniaudioContext() {
    ma_result result;
#if defined(__linux__)
    const ma_backend backends[] = {ma_backend_pulseaudio};
    result = ma_context_init(backends, 1, nullptr, &context);
#else
    result = ma_context_init(nullptr, 0, nullptr, &context);
#endif
    if (result == MA_SUCCESS) {
      initialized = true;
    } else {
      LOG_ERROR("failed to initialize miniaudio context: {}", ma_result_description(result));
    }
  }

  ~MiniaudioContext() {
    if (initialized) {
      ma_context_uninit(&context);
    }
  }

  MiniaudioContext(const MiniaudioContext&)            = delete;
  MiniaudioContext& operator=(const MiniaudioContext&) = delete;
};

struct KeyClickState {
  std::mutex mutex;
  std::string path;
  std::shared_ptr<WavAudio> audio;
  std::unique_ptr<MiniaudioContext> context;
  ma_device device{};
  ma_uint32 sample_rate{0};
  bool device_initialized{false};
  bool device_started{false};
  std::atomic<std::size_t> frame_offset{0};
  std::atomic<bool> active{false};

  ~KeyClickState() {
    active.store(false);
    if (device_started) {
      ma_device_stop(&device);
    }
    if (device_initialized) {
      ma_device_uninit(&device);
    }
  }
};

std::mutex& playback_mutex() {
  static std::mutex mutex;
  return mutex;
}

std::atomic<float>& global_volume_level() {
  static std::atomic<float> level{K_DEFAULT_VOLUME_LEVEL};
  return level;
}

float current_volume_level() { return global_volume_level().load(); }

SelectedAudioDevice& selected_audio_device() {
  static SelectedAudioDevice device;
  return device;
}

std::mutex& selected_audio_device_mutex() {
  static std::mutex mutex;
  return mutex;
}

KeyClickState& key_click_state() {
  static KeyClickState state;
  return state;
}

std::string miniaudio_device_id(ma_backend backend, const ma_device_id& id) {
  if (backend == ma_backend_pulseaudio) {
    return id.pulse;
  }
  if (backend == ma_backend_coreaudio) {
    return id.coreaudio;
  }
  if (backend == ma_backend_sndio) {
    return id.sndio;
  }
  if (backend == ma_backend_audio4) {
    return id.audio4;
  }
  if (backend == ma_backend_oss) {
    return id.oss;
  }

  constexpr char kHex[] = "0123456789abcdef";
  constexpr std::size_t kMaxBytesToLog = 16;
  const auto* bytes = reinterpret_cast<const unsigned char*>(&id);
  std::string value;
  value.reserve(kMaxBytesToLog * 2);
  for (std::size_t i = 0; i < kMaxBytesToLog && i < sizeof(ma_device_id); ++i) {
    value.push_back(kHex[bytes[i] >> 4U]);
    value.push_back(kHex[bytes[i] & 0x0FU]);
  }
  return value;
}

const char* share_mode_name(ma_share_mode share_mode) {
  return share_mode == ma_share_mode_exclusive ? "exclusive" : "shared";
}

SelectedDevice select_system_audio_device(ma_backend backend,
                                          ma_device_info* devices,
                                          ma_uint32 device_count,
                                          const char* direction) {
  SelectedDevice selected;
  if (device_count == 0) {
    LOG_ERROR("no miniaudio {} system device found", direction);
    return selected;
  }
  for (ma_uint32 i = 0; i < device_count; ++i) {
    if (devices[i].isDefault) {
      selected.name  = devices[i].name;
      selected.id    = miniaudio_device_id(backend, devices[i].id);
      selected.found = true;
      break;
    }
  }
  if (!selected.found) {
    selected.name  = devices[0].name;
    selected.id    = miniaudio_device_id(backend, devices[0].id);
    selected.found = true;
  }

  LOG_INFO("selected miniaudio {} system device: name='{}' id='{}'",
           direction,
           selected.name,
           selected.id);
  return selected;
}

bool enumerate_system_audio_devices(MiniaudioContext& context,
                                    SelectedDevice& playback,
                                    SelectedDevice& capture) {
  ma_device_info* playback_devices = nullptr;
  ma_device_info* capture_devices  = nullptr;
  ma_uint32 playback_count         = 0;
  ma_uint32 capture_count          = 0;
  const ma_result result           = ma_context_get_devices(&context.context,
                                                            &playback_devices,
                                                            &playback_count,
                                                            &capture_devices,
                                                            &capture_count);
  if (result != MA_SUCCESS || playback_count == 0 || capture_count == 0) {
    LOG_ERROR("miniaudio device enumeration failed: {} playback={} capture={}",
              ma_result_description(result),
              playback_count,
              capture_count);
    return false;
  }

  playback = select_system_audio_device(context.context.backend,
                                        playback_devices,
                                        playback_count,
                                        "playback");
  capture =
      select_system_audio_device(context.context.backend, capture_devices, capture_count, "capture");
  return playback.found && capture.found;
}

SelectedAudioDevice snapshot_selected_audio_device() {
  std::lock_guard<std::mutex> lock(selected_audio_device_mutex());
  return selected_audio_device();
}

SelectedDevice snapshot_selected_playback_device() {
  const auto selected = snapshot_selected_audio_device();
  return {selected.info.playback_device, {}, selected.playback_found};
}

SelectedDevice snapshot_selected_capture_device() {
  const auto selected = snapshot_selected_audio_device();
  return {selected.info.capture_device, {}, selected.capture_found};
}

void capture_data_callback(ma_device* device,
                           void* output,
                           const void* input,
                           ma_uint32 frame_count) {
  (void)output;
  auto* state = static_cast<CaptureState*>(device->pUserData);
  if (!state || !input || frame_count == 0) {
    return;
  }

  auto* samples                   = static_cast<const int16_t*>(input);
  const std::size_t input_samples = static_cast<std::size_t>(frame_count) * K_AUDIO_CHANNELS;

  {
    std::lock_guard<std::mutex> lock(state->mutex);
    if (state->complete) {
      return;
    }

    const std::size_t remaining =
        state->target_samples - std::min(state->samples.size(), state->target_samples);
    const std::size_t copy_count = std::min(input_samples, remaining);
    state->samples.insert(state->samples.end(), samples, samples + copy_count);
    if (state->samples.size() >= state->target_samples) {
      state->complete = true;
    }
  }

  if (state->complete) {
    state->cv.notify_one();
  }
}

void playback_data_callback(ma_device* device,
                            void* output,
                            const void* input,
                            ma_uint32 frame_count) {
  (void)input;
  auto* state = static_cast<PlaybackState*>(device->pUserData);
  auto* out   = static_cast<int16_t*>(output);
  if (!out || frame_count == 0) {
    return;
  }

  const ma_uint32 output_channels = std::max<ma_uint32>(1, device->playback.channels);
  std::memset(out, 0, static_cast<std::size_t>(frame_count) * output_channels * sizeof(int16_t));
  if (!state || !state->audio || state->complete.load()) {
    return;
  }

  const auto& audio              = *state->audio;
  const ma_uint32 input_channels = std::max<ma_uint32>(1, audio.channels);
  const std::size_t total_frames = audio.samples.size() / input_channels;
  const std::size_t offset       = state->frame_offset.load();
  if (offset >= total_frames) {
    state->complete.store(true);
    state->cv.notify_one();
    return;
  }

  const std::size_t copy_frames = std::min<std::size_t>(frame_count, total_frames - offset);
  for (std::size_t frame = 0; frame < copy_frames; ++frame) {
    const std::size_t input_base  = (offset + frame) * input_channels;
    const std::size_t output_base = frame * output_channels;
    for (ma_uint32 channel = 0; channel < output_channels; ++channel) {
      const ma_uint32 input_channel = input_channels == 1 ? 0 : std::min(channel, input_channels - 1);
      out[output_base + channel] = audio.samples[input_base + input_channel];
    }
  }

  const std::size_t next_offset = offset + copy_frames;
  state->frame_offset.store(next_offset);
  if (next_offset >= total_frames) {
    state->complete.store(true);
    state->cv.notify_one();
  }
}

void key_click_data_callback(ma_device* device,
                             void* output,
                             const void* input,
                             ma_uint32 frame_count) {
  (void)input;
  auto* state = static_cast<KeyClickState*>(device->pUserData);
  auto* out   = static_cast<int16_t*>(output);
  if (!out || frame_count == 0) {
    return;
  }

  const ma_uint32 output_channels = std::max<ma_uint32>(1, device->playback.channels);
  std::memset(out, 0, static_cast<std::size_t>(frame_count) * output_channels * sizeof(int16_t));
  if (!state || !state->active.load()) {
    return;
  }

  std::shared_ptr<WavAudio> audio = std::atomic_load(&state->audio);
  if (!audio || audio->samples.empty() || audio->sample_rate == 0 || audio->channels == 0) {
    state->active.store(false);
    return;
  }

  const ma_uint32 input_channels = std::max<ma_uint32>(1, audio->channels);
  const std::size_t total_frames = audio->samples.size() / input_channels;
  const std::size_t offset       = state->frame_offset.load();
  if (offset >= total_frames) {
    state->active.store(false);
    return;
  }

  const std::size_t copy_frames = std::min<std::size_t>(frame_count, total_frames - offset);
  for (std::size_t frame = 0; frame < copy_frames; ++frame) {
    const std::size_t input_base  = (offset + frame) * input_channels;
    const std::size_t output_base = frame * output_channels;
    for (ma_uint32 channel = 0; channel < output_channels; ++channel) {
      const ma_uint32 input_channel = input_channels == 1 ? 0 : std::min(channel, input_channels - 1);
      out[output_base + channel] = audio->samples[input_base + input_channel];
    }
  }

  const std::size_t next_offset = offset + copy_frames;
  state->frame_offset.store(next_offset);
  if (next_offset >= total_frames) {
    state->active.store(false);
  }
}

void stop_key_click_device_locked(KeyClickState& state) {
  state.active.store(false);
  state.frame_offset.store(0);
  if (state.device_started) {
    ma_device_stop(&state.device);
    state.device_started = false;
  }
  if (state.device_initialized) {
    ma_device_uninit(&state.device);
    state.device_initialized = false;
  }
  state.context.reset();
  state.sample_rate = 0;
}

void stop_key_click_device() {
  auto& state = key_click_state();
  std::lock_guard<std::mutex> lock(state.mutex);
  stop_key_click_device_locked(state);
}

bool ensure_key_click_device_locked(KeyClickState& state, const std::shared_ptr<WavAudio>& audio) {
  if (!audio || audio->sample_rate == 0 || audio->channels == 0 || audio->samples.empty()) {
    return false;
  }
  if (state.device_started && state.sample_rate == audio->sample_rate) {
    return true;
  }

  std::lock_guard<std::mutex> playback_lock(playback_mutex());
  stop_key_click_device_locked(state);

  state.context = std::make_unique<MiniaudioContext>();
  if (!state.context->initialized) {
    state.context.reset();
    return false;
  }

  ma_device_config config   = ma_device_config_init(ma_device_type_playback);
  config.playback.format    = ma_format_s16;
  config.playback.channels  = K_PLAYBACK_CHANNELS;
  config.playback.shareMode = ma_share_mode_shared;
  config.sampleRate         = audio->sample_rate;
  config.dataCallback       = key_click_data_callback;
  config.pUserData          = &state;

  LOG_INFO("initializing key click playback: backend={} device=default rate={} input_channels={} output_channels={} share=shared",
           ma_get_backend_name(state.context->context.backend),
           audio->sample_rate,
           audio->channels,
           K_PLAYBACK_CHANNELS);

  ma_result result = ma_device_init(&state.context->context, &config, &state.device);
  if (result != MA_SUCCESS) {
    LOG_ERROR("failed to initialize key click playback: {}", ma_result_description(result));
    state.context.reset();
    return false;
  }
  state.device_initialized = true;

  result = ma_device_set_master_volume(&state.device, current_volume_level());
  if (result != MA_SUCCESS) {
    LOG_ERROR("failed to set key click playback volume: {}", ma_result_description(result));
    stop_key_click_device_locked(state);
    return false;
  }

  result = ma_device_start(&state.device);
  if (result != MA_SUCCESS) {
    LOG_ERROR("failed to start key click playback: {}", ma_result_description(result));
    stop_key_click_device_locked(state);
    return false;
  }

  state.device_started = true;
  state.sample_rate    = audio->sample_rate;
  return true;
}

bool write_wav_file(const std::string& output_path, const std::vector<int16_t>& samples) {
  if (samples.empty()) {
    return false;
  }

  std::remove(output_path.c_str());
  ma_encoder_config config = ma_encoder_config_init(ma_encoding_format_wav,
                                                    ma_format_s16,
                                                    K_AUDIO_CHANNELS,
                                                    K_AUDIO_SAMPLE_RATE);
  ma_encoder encoder;
  ma_result result = ma_encoder_init_file(output_path.c_str(), &config, &encoder);
  if (result != MA_SUCCESS) {
    LOG_ERROR("failed to create WAV file {}: {}", output_path, ma_result_description(result));
    return false;
  }

  ma_uint64 frames_written    = 0;
  const ma_uint64 frame_count = samples.size() / K_AUDIO_CHANNELS;
  result = ma_encoder_write_pcm_frames(&encoder, samples.data(), frame_count, &frames_written);
  ma_encoder_uninit(&encoder);
  if (result != MA_SUCCESS || frames_written != frame_count) {
    LOG_ERROR("failed to write WAV file {}: {}", output_path, ma_result_description(result));
    return false;
  }

  return true;
}

void normalize_to_full_scale(WavAudio& audio) {
  int32_t peak = 0;
  for (const auto sample : audio.samples) {
    const int32_t sample_value = static_cast<int32_t>(sample);
    const int32_t value        = sample_value < 0 ? -sample_value : sample_value;
    peak                       = std::max(peak, value);
  }
  if (peak <= 0 || peak >= INT16_MAX) {
    return;
  }

  constexpr int32_t K_TARGET_PEAK = INT16_MAX;
  for (auto& sample : audio.samples) {
    int64_t scaled = static_cast<int64_t>(sample) * K_TARGET_PEAK / peak;
    scaled         = std::max<int64_t>(INT16_MIN, std::min<int64_t>(INT16_MAX, scaled));
    sample         = static_cast<int16_t>(scaled);
  }
}

std::shared_ptr<WavAudio> load_wav_audio(const std::string& input_path) {
  if (input_path.empty()) {
    return nullptr;
  }

  ma_decoder_config config = ma_decoder_config_init(ma_format_s16, 0, 0);
  ma_uint64 frame_count    = 0;
  void* pcm_frames         = nullptr;
  ma_result result         = ma_decode_file(input_path.c_str(), &config, &frame_count, &pcm_frames);
  if (result != MA_SUCCESS || !pcm_frames || frame_count == 0 || config.channels == 0 ||
      config.sampleRate == 0) {
    LOG_ERROR("failed to decode WAV file {}: {}", input_path, ma_result_description(result));
    if (pcm_frames) {
      ma_free(pcm_frames, nullptr);
    }
    return nullptr;
  }

  auto audio                     = std::make_shared<WavAudio>();
  audio->sample_rate             = config.sampleRate;
  audio->channels                = config.channels;
  const std::size_t sample_count = static_cast<std::size_t>(frame_count) * config.channels;
  auto* samples                  = static_cast<const int16_t*>(pcm_frames);
  audio->samples.assign(samples, samples + sample_count);
  ma_free(pcm_frames, nullptr);
  return audio;
}

bool play_audio(const std::shared_ptr<WavAudio>& audio,
                std::chrono::milliseconds extra_timeout) {
  if (!audio || audio->samples.empty() || audio->sample_rate == 0 || audio->channels == 0) {
    return false;
  }

  stop_key_click_device();
  std::lock_guard<std::mutex> playback_lock(playback_mutex());
  MiniaudioContext context;
  if (!context.initialized) {
    return false;
  }

  SelectedDevice playback = snapshot_selected_playback_device();

  PlaybackState state;
  state.audio = audio;

  constexpr ma_share_mode share_mode = ma_share_mode_shared;
  ma_device_config config   = ma_device_config_init(ma_device_type_playback);
  config.playback.format    = ma_format_s16;
  config.playback.channels  = K_PLAYBACK_CHANNELS;
  config.playback.shareMode = share_mode;
  config.sampleRate         = audio->sample_rate;
  config.dataCallback       = playback_data_callback;
  config.pUserData          = &state;

  const float volume = current_volume_level();
  LOG_INFO("initializing miniaudio playback: backend={} device='{}' rate={} input_channels={} output_channels={} share={} volume={:.2f}",
           ma_get_backend_name(context.context.backend),
           playback.found ? playback.name : "default",
           audio->sample_rate,
           audio->channels,
           K_PLAYBACK_CHANNELS,
           share_mode_name(share_mode),
           volume);

  ma_device device;
  ma_result result = ma_device_init(&context.context, &config, &device);
  if (result != MA_SUCCESS) {
    LOG_ERROR("failed to initialize miniaudio playback: {}", ma_result_description(result));
    return false;
  }

  result = ma_device_set_master_volume(&device, volume);
  if (result != MA_SUCCESS) {
    LOG_ERROR("failed to set miniaudio playback volume: {}", ma_result_description(result));
    ma_device_uninit(&device);
    return false;
  }

  result = ma_device_start(&device);
  if (result != MA_SUCCESS) {
    LOG_ERROR("failed to start miniaudio playback: {}", ma_result_description(result));
    ma_device_uninit(&device);
    return false;
  }
  const std::size_t total_frames = audio->samples.size() / audio->channels;
  const auto duration =
      std::chrono::milliseconds(static_cast<int64_t>(total_frames * 1000 / audio->sample_rate));
  std::unique_lock<std::mutex> lock(state.mutex);
  const bool complete = state.cv.wait_for(lock, duration + extra_timeout, [&state]() {
    return state.complete.load();
  });
  ma_device_stop(&device);
  ma_device_uninit(&device);
  return complete;
}

}  // namespace

bool find_audio_device(AudioDevice& device, std::string& error_message) {
  MiniaudioContext context;
  if (!context.initialized) {
    error_message = "Audio device error";
    return false;
  }

  SelectedDevice playback;
  SelectedDevice capture;
  if (!enumerate_system_audio_devices(context, playback, capture)) {
    error_message = "Audio device error";
    return false;
  }

  device.backend_name    = "miniaudio";
  device.display_name    = std::string("miniaudio ") + ma_get_backend_name(context.context.backend);
  device.playback_device = playback.name;
  device.capture_device  = capture.name;
  {
    std::lock_guard<std::mutex> lock(selected_audio_device_mutex());
    auto& selected          = selected_audio_device();
    selected.info           = device;
    selected.playback_found = true;
    selected.capture_found  = true;
  }
  LOG_INFO("using miniaudio backend {} playback={} capture={}",
           ma_get_backend_name(context.context.backend),
           device.playback_device,
           device.capture_device);
  return true;
}

bool record_wav(const AudioDevice& device, const std::string& output_path, int seconds) {
  (void)device;
  if (seconds <= 0) {
    LOG_ERROR("invalid audio record duration: {}", seconds);
    return false;
  }

  MiniaudioContext context;
  if (!context.initialized) {
    return false;
  }
  SelectedDevice capture = snapshot_selected_capture_device();

  CaptureState state;
  state.target_samples = static_cast<std::size_t>(seconds) * K_AUDIO_SAMPLE_RATE * K_AUDIO_CHANNELS;
  state.samples.reserve(state.target_samples);

  ma_device_config config  = ma_device_config_init(ma_device_type_capture);
  config.capture.format    = ma_format_s16;
  config.capture.channels  = K_AUDIO_CHANNELS;
  config.sampleRate        = K_AUDIO_SAMPLE_RATE;
  config.dataCallback      = capture_data_callback;
  config.pUserData         = &state;

  LOG_INFO("initializing miniaudio capture: backend={} device='{}' rate={} channels={}",
           ma_get_backend_name(context.context.backend),
           capture.found ? capture.name : "default",
           K_AUDIO_SAMPLE_RATE,
           K_AUDIO_CHANNELS);

  ma_device device_handle;
  ma_result result = ma_device_init(&context.context, &config, &device_handle);
  if (result != MA_SUCCESS) {
    LOG_ERROR("failed to initialize miniaudio capture: {}", ma_result_description(result));
    return false;
  }

  result = ma_device_start(&device_handle);
  if (result != MA_SUCCESS) {
    LOG_ERROR("failed to start miniaudio capture: {}", ma_result_description(result));
    ma_device_uninit(&device_handle);
    return false;
  }

  std::unique_lock<std::mutex> lock(state.mutex);
  const bool complete = state.cv.wait_for(lock, std::chrono::seconds(seconds + 3), [&state]() {
    return state.complete;
  });
  lock.unlock();
  ma_device_stop(&device_handle);
  ma_device_uninit(&device_handle);

  if (!complete || state.samples.empty()) {
    LOG_ERROR("miniaudio recording did not capture audio samples");
    return false;
  }

  if (!write_wav_file(output_path, state.samples)) {
    return false;
  }

  return true;
}

bool play_wav(const AudioDevice& device, const std::string& input_path) {
  (void)device;
  auto audio = load_wav_audio(input_path);
  if (!audio) {
    return false;
  }
  normalize_to_full_scale(*audio);
  const bool played = play_audio(audio, std::chrono::seconds(3));
  initialize_key_click_sound();
  return played;
}

void set_volume_level(float level) {
  const float clamped_level = std::max(0.0F, std::min(1.0F, level));
  global_volume_level().store(clamped_level);

  auto& state = key_click_state();
  std::lock_guard<std::mutex> lock(state.mutex);
  if (!state.device_initialized) {
    return;
  }

  const ma_result result = ma_device_set_master_volume(&state.device, clamped_level);
  if (result != MA_SUCCESS) {
    LOG_ERROR("failed to update key click playback volume: {}", ma_result_description(result));
  }
}

void set_key_click_sound_path(const std::string& input_path) {
  auto audio = load_wav_audio(input_path);

  auto& state = key_click_state();
  {
    std::lock_guard<std::mutex> lock(state.mutex);
    state.path = audio ? input_path : std::string{};
    std::atomic_store(&state.audio, audio);
    if (!audio) {
      stop_key_click_device_locked(state);
    }
  }
}

bool initialize_key_click_sound() {
  auto& state                     = key_click_state();
  std::shared_ptr<WavAudio> audio = std::atomic_load(&state.audio);
  if (!audio) {
    return false;
  }

  std::lock_guard<std::mutex> lock(state.mutex);
  return ensure_key_click_device_locked(state, audio);
}

void play_key_click_sound() {
  auto& state                     = key_click_state();
  std::shared_ptr<WavAudio> audio = std::atomic_load(&state.audio);
  if (!audio) {
    return;
  }

  {
    std::lock_guard<std::mutex> lock(state.mutex);
    if (!ensure_key_click_device_locked(state, audio)) {
      return;
    }
    state.frame_offset.store(0);
    state.active.store(true);
  }
}

}  // namespace platform::audio
