/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "audio_service.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <ios>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "logger.h"

#ifndef APP_USE_PIPEWIRE
#define APP_USE_PIPEWIRE 0
#endif

#if APP_USE_PIPEWIRE
#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>
#include <spa/param/audio/raw.h>
#endif

namespace platform::audio {
namespace {

constexpr uint32_t K_AUDIO_SAMPLE_RATE      = 16000;
constexpr uint32_t K_AUDIO_CHANNELS         = 1;
constexpr uint32_t K_AUDIO_BITS_PER_SAMPLE  = 16;
constexpr uint32_t K_AUDIO_FRAME_BYTES      = K_AUDIO_CHANNELS * K_AUDIO_BITS_PER_SAMPLE / 8;
constexpr uint32_t K_AUDIO_BYTES_PER_SAMPLE = K_AUDIO_BITS_PER_SAMPLE / 8;

struct WavAudio {
  uint32_t sample_rate{0};
  uint16_t channels{0};
  std::vector<int16_t> samples;
};

std::atomic<float>& audio_level() {
  static std::atomic<float> level{0.0F};
  return level;
}

void set_audio_level_from_samples(const int16_t* samples, std::size_t count) {
  if (!samples || count == 0) {
    audio_level().store(0.0F);
    return;
  }

  int32_t peak = 0;
  for (std::size_t i = 0; i < count; ++i) {
    peak = std::max<int32_t>(peak, std::abs(static_cast<int32_t>(samples[i])));
  }
  audio_level().store(std::min(1.0F, static_cast<float>(peak) / 32768.0F));
}

void write_wav_header(std::ofstream& file, uint32_t data_bytes) {
  const uint32_t byte_rate    = K_AUDIO_SAMPLE_RATE * K_AUDIO_FRAME_BYTES;
  const uint16_t block_align  = K_AUDIO_FRAME_BYTES;
  const uint32_t riff_size    = 36 + data_bytes;
  const uint16_t audio_format = 1;

  file.seekp(0, std::ios::beg);
  file.write("RIFF", 4);
  file.write(reinterpret_cast<const char*>(&riff_size), sizeof(riff_size));
  file.write("WAVE", 4);
  file.write("fmt ", 4);
  const uint32_t fmt_size = 16;
  file.write(reinterpret_cast<const char*>(&fmt_size), sizeof(fmt_size));
  file.write(reinterpret_cast<const char*>(&audio_format), sizeof(audio_format));
  const uint16_t channels = K_AUDIO_CHANNELS;
  file.write(reinterpret_cast<const char*>(&channels), sizeof(channels));
  file.write(reinterpret_cast<const char*>(&K_AUDIO_SAMPLE_RATE), sizeof(K_AUDIO_SAMPLE_RATE));
  file.write(reinterpret_cast<const char*>(&byte_rate), sizeof(byte_rate));
  file.write(reinterpret_cast<const char*>(&block_align), sizeof(block_align));
  const uint16_t bits_per_sample = K_AUDIO_BITS_PER_SAMPLE;
  file.write(reinterpret_cast<const char*>(&bits_per_sample), sizeof(bits_per_sample));
  file.write("data", 4);
  file.write(reinterpret_cast<const char*>(&data_bytes), sizeof(data_bytes));
}

bool read_wav_payload(const std::string& input_path,
                      std::vector<int16_t>& samples,
                      uint32_t* sample_rate  = nullptr,
                      uint16_t* channels_out = nullptr) {
  std::ifstream file(input_path, std::ios::binary);
  if (!file.is_open()) {
    LOG_ERROR("failed to open WAV file: {}", input_path);
    return false;
  }

  char riff[4] = {};
  file.read(riff, 4);
  if (std::strncmp(riff, "RIFF", 4) != 0) {
    LOG_ERROR("invalid WAV file: {}", input_path);
    return false;
  }
  file.seekg(20, std::ios::beg);
  uint16_t audio_format = 0;
  uint16_t channels     = 0;
  uint32_t rate         = 0;
  file.read(reinterpret_cast<char*>(&audio_format), sizeof(audio_format));
  file.read(reinterpret_cast<char*>(&channels), sizeof(channels));
  file.read(reinterpret_cast<char*>(&rate), sizeof(rate));
  file.seekg(34, std::ios::beg);
  uint16_t bits = 0;
  file.read(reinterpret_cast<char*>(&bits), sizeof(bits));
  if (audio_format != 1 || rate == 0 || channels < 1 || channels > 2 ||
      bits != K_AUDIO_BITS_PER_SAMPLE) {
    LOG_ERROR("unsupported WAV format: format={} channels={} rate={} bits={}",
              audio_format,
              channels,
              rate,
              bits);
    return false;
  }
  if (sample_rate) {
    *sample_rate = rate;
  }
  if (channels_out) {
    *channels_out = channels;
  }

  file.seekg(12, std::ios::beg);
  while (file.good()) {
    char chunk_id[4]    = {};
    uint32_t chunk_size = 0;
    file.read(chunk_id, 4);
    file.read(reinterpret_cast<char*>(&chunk_size), sizeof(chunk_size));
    if (!file.good()) {
      break;
    }
    if (std::strncmp(chunk_id, "data", 4) == 0) {
      samples.resize(chunk_size / sizeof(int16_t));
      file.read(reinterpret_cast<char*>(samples.data()),
                static_cast<std::streamsize>(samples.size() * sizeof(int16_t)));
      return !samples.empty() && (file.good() || file.eof());
    }
    file.seekg(chunk_size + (chunk_size & 1U), std::ios::cur);
  }

  LOG_ERROR("WAV data chunk not found: {}", input_path);
  return false;
}

#if APP_USE_PIPEWIRE
enum class PipeWireMode { RECORD, PLAYBACK };

struct PipeWireSession {
  PipeWireMode mode                        = PipeWireMode::RECORD;
  struct pw_main_loop* loop                = nullptr;
  struct pw_stream* stream                 = nullptr;
  std::vector<int16_t>* record_samples     = nullptr;
  const std::vector<int16_t>* play_samples = nullptr;
  uint32_t sample_rate                     = K_AUDIO_SAMPLE_RATE;
  uint16_t channels                        = K_AUDIO_CHANNELS;
  bool update_level                        = true;
  std::size_t target_samples               = 0;
  std::size_t sample_offset                = 0;
  bool drain_requested                     = false;
  bool drain_started                       = false;
  std::atomic<bool> running{false};
  std::atomic<bool> failed{false};
  std::atomic<bool> completed{false};
  std::mutex error_mutex;
  std::string error_message;
};

uint32_t frame_bytes(const PipeWireSession& session) {
  return static_cast<uint32_t>(session.channels) * K_AUDIO_BYTES_PER_SAMPLE;
}

void pipewire_init_once() {
  static std::once_flag init_flag;
  std::call_once(init_flag, []() { pw_init(nullptr, nullptr); });
}

void set_session_error(PipeWireSession& session, const std::string& message) {
  {
    std::lock_guard<std::mutex> lock(session.error_mutex);
    session.error_message = message;
  }
  session.failed.store(true);
  if (session.loop) {
    pw_main_loop_quit(session.loop);
  }
}

std::string session_error(PipeWireSession& session) {
  std::lock_guard<std::mutex> lock(session.error_mutex);
  return session.error_message;
}

void on_stream_state_changed(void* data,
                             enum pw_stream_state /*old_state*/,
                             enum pw_stream_state state,
                             const char* error) {
  auto* session = static_cast<PipeWireSession*>(data);
  if (!session) {
    return;
  }

  if (state == PW_STREAM_STATE_ERROR) {
    set_session_error(*session, error && error[0] != '\0' ? error : "PipeWire stream error");
  }
}

void on_stream_drained(void* data) {
  auto* session = static_cast<PipeWireSession*>(data);
  if (!session) {
    return;
  }

  session->completed.store(true);
  if (session->loop) {
    pw_main_loop_quit(session->loop);
  }
}

void process_record_buffer(PipeWireSession& session, struct pw_buffer& buffer) {
  if (!session.record_samples || !buffer.buffer || buffer.buffer->n_datas == 0) {
    return;
  }

  struct spa_data& data = buffer.buffer->datas[0];
  if (!data.data || !data.chunk) {
    return;
  }

  const uint32_t offset = std::min(data.chunk->offset, data.maxsize);
  uint32_t bytes        = std::min(data.chunk->size, data.maxsize - offset);
  bytes -= bytes % sizeof(int16_t);
  if (bytes == 0) {
    return;
  }

  auto* sample_data = reinterpret_cast<const int16_t*>(static_cast<uint8_t*>(data.data) + offset);
  const std::size_t count = bytes / sizeof(int16_t);
  const std::size_t remain =
      session.target_samples - std::min(session.record_samples->size(), session.target_samples);
  const std::size_t copy_count = std::min(count, remain);
  if (copy_count > 0) {
    const auto old_size = session.record_samples->size();
    session.record_samples->resize(old_size + copy_count);
    std::memcpy(session.record_samples->data() + old_size,
                sample_data,
                copy_count * sizeof(int16_t));
    set_audio_level_from_samples(sample_data, copy_count);
  }

  if (session.record_samples->size() >= session.target_samples) {
    session.completed.store(true);
  }
}

void process_playback_buffer(PipeWireSession& session, struct pw_buffer& buffer) {
  if (!session.play_samples || !buffer.buffer || buffer.buffer->n_datas == 0) {
    return;
  }

  struct spa_data& data = buffer.buffer->datas[0];
  if (!data.data || !data.chunk) {
    return;
  }

  if (session.drain_started) {
    data.chunk->offset = 0;
    data.chunk->stride = frame_bytes(session);
    data.chunk->size   = 0;
    return;
  }

  uint32_t bytes = data.maxsize;
  if (buffer.requested > 0) {
    bytes = std::min<uint32_t>(bytes, buffer.requested * frame_bytes(session));
  }
  bytes -= bytes % frame_bytes(session);
  if (bytes == 0) {
    return;
  }

  auto* output = static_cast<uint8_t*>(data.data);
  std::memset(output, 0, bytes);

  const std::size_t writable_samples = bytes / sizeof(int16_t);
  const std::size_t remaining_samples =
      session.play_samples->size() - std::min(session.sample_offset, session.play_samples->size());
  std::size_t copy_count = std::min(writable_samples, remaining_samples);
  copy_count -= copy_count % session.channels;
  if (copy_count > 0) {
    const int16_t* source = session.play_samples->data() + session.sample_offset;
    std::memcpy(output, source, copy_count * sizeof(int16_t));
    if (session.update_level) {
      set_audio_level_from_samples(source, copy_count);
    }
    session.sample_offset += copy_count;
  } else {
    if (session.update_level) {
      audio_level().store(0.0F);
    }
  }

  data.chunk->offset = 0;
  data.chunk->stride = frame_bytes(session);
  data.chunk->size   = bytes;

  if (session.sample_offset >= session.play_samples->size()) {
    session.drain_requested = true;
  }
}

void on_stream_process(void* data) {
  auto* session = static_cast<PipeWireSession*>(data);
  if (!session || !session->stream) {
    return;
  }

  struct pw_buffer* buffer = pw_stream_dequeue_buffer(session->stream);
  if (!buffer) {
    return;
  }

  if (session->mode == PipeWireMode::RECORD) {
    process_record_buffer(*session, *buffer);
  } else {
    process_playback_buffer(*session, *buffer);
  }

  const bool start_drain = session->mode == PipeWireMode::PLAYBACK && session->drain_requested &&
                           !session->drain_started;
  pw_stream_queue_buffer(session->stream, buffer);
  if (start_drain) {
    session->drain_started = true;
    const int result       = pw_stream_flush(session->stream, true);
    if (result < 0) {
      set_session_error(*session, "failed to drain PipeWire stream");
    }
  }
  if (session->completed.load() && session->loop) {
    pw_main_loop_quit(session->loop);
  }
}

bool run_pipewire_stream(PipeWireSession& session, std::chrono::milliseconds timeout) {
  pipewire_init_once();

  session.loop = pw_main_loop_new(nullptr);
  if (!session.loop) {
    LOG_ERROR("failed to create PipeWire main loop");
    return false;
  }

  struct pw_stream_events events = {};
  events.version                 = PW_VERSION_STREAM_EVENTS;
  events.state_changed           = on_stream_state_changed;
  events.process                 = on_stream_process;
  events.drained                 = on_stream_drained;

  struct pw_properties* properties =
      pw_properties_new(PW_KEY_MEDIA_TYPE,
                        "Audio",
                        PW_KEY_MEDIA_CATEGORY,
                        session.mode == PipeWireMode::RECORD ? "Capture" : "Playback",
                        PW_KEY_MEDIA_ROLE,
                        "Test",
                        PW_KEY_NODE_NAME,
                        session.mode == PipeWireMode::RECORD ? "factory-test-audio-record"
                                                             : "factory-test-audio-playback",
                        nullptr);

  session.stream =
      pw_stream_new_simple(pw_main_loop_get_loop(session.loop),
                           session.mode == PipeWireMode::RECORD ? "Factory Test Audio Record"
                                                                : "Factory Test Audio Playback",
                           properties,
                           &events,
                           &session);
  if (!session.stream) {
    pw_main_loop_destroy(session.loop);
    session.loop = nullptr;
    LOG_ERROR("failed to create PipeWire stream");
    return false;
  }

  struct spa_audio_info_raw audio_info = {};
  audio_info.format                    = SPA_AUDIO_FORMAT_S16;
  audio_info.rate                      = session.sample_rate;
  audio_info.channels                  = session.channels;
  if (session.channels == 1) {
    audio_info.position[0] = SPA_AUDIO_CHANNEL_MONO;
  } else {
    audio_info.position[0] = SPA_AUDIO_CHANNEL_FL;
    audio_info.position[1] = SPA_AUDIO_CHANNEL_FR;
  }

  uint8_t buffer[1024];
  struct spa_pod_builder builder = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
  const struct spa_pod* params[] = {
      spa_format_audio_raw_build(&builder, SPA_PARAM_EnumFormat, &audio_info),
  };

  const enum pw_direction direction =
      session.mode == PipeWireMode::RECORD ? PW_DIRECTION_INPUT : PW_DIRECTION_OUTPUT;
  const int connect_result = pw_stream_connect(
      session.stream,
      direction,
      PW_ID_ANY,
      static_cast<enum pw_stream_flags>(PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS),
      params,
      1);
  if (connect_result < 0) {
    LOG_ERROR("failed to connect PipeWire stream: {}", connect_result);
    pw_stream_destroy(session.stream);
    pw_main_loop_destroy(session.loop);
    session.stream = nullptr;
    session.loop   = nullptr;
    return false;
  }

  session.running.store(true);
  std::thread watchdog([&session, timeout]() {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (session.running.load() && std::chrono::steady_clock::now() < deadline) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    if (session.running.load()) {
      set_session_error(session, "PipeWire stream timed out");
    }
  });

  pw_main_loop_run(session.loop);
  session.running.store(false);
  if (watchdog.joinable()) {
    watchdog.join();
  }

  pw_stream_destroy(session.stream);
  pw_main_loop_destroy(session.loop);
  session.stream = nullptr;
  session.loop   = nullptr;

  if (session.failed.load()) {
    LOG_ERROR("PipeWire audio stream failed: {}", session_error(session));
    return false;
  }
  return session.completed.load();
}

bool record_wav_pipewire(const std::string& output_path, int seconds) {
  if (seconds <= 0) {
    LOG_ERROR("invalid audio record duration: {}", seconds);
    return false;
  }

  std::vector<int16_t> samples;
  const std::size_t target_samples =
      static_cast<std::size_t>(seconds) * K_AUDIO_SAMPLE_RATE * K_AUDIO_CHANNELS;
  samples.reserve(target_samples);

  PipeWireSession session;
  session.mode           = PipeWireMode::RECORD;
  session.record_samples = &samples;
  session.target_samples = target_samples;
  session.sample_rate    = K_AUDIO_SAMPLE_RATE;
  session.channels       = K_AUDIO_CHANNELS;
  session.update_level   = true;

  const bool recorded = run_pipewire_stream(session, std::chrono::seconds(seconds + 5));
  audio_level().store(0.0F);
  if (!recorded || samples.empty()) {
    LOG_ERROR("PipeWire recording did not capture audio samples");
    return false;
  }

  std::remove(output_path.c_str());
  std::ofstream file(output_path, std::ios::binary | std::ios::trunc);
  if (!file.is_open()) {
    LOG_ERROR("failed to create WAV file: {}", output_path);
    return false;
  }

  const uint32_t data_bytes = static_cast<uint32_t>(samples.size() * sizeof(int16_t));
  write_wav_header(file, data_bytes);
  file.write(reinterpret_cast<const char*>(samples.data()), data_bytes);
  if (!file.good()) {
    LOG_ERROR("failed to write WAV file: {}", output_path);
    return false;
  }

  LOG_INFO("PipeWire recording completed: {} sample(s)", samples.size());
  return true;
}

bool play_wav_pipewire(const std::string& input_path) {
  WavAudio audio;
  if (!read_wav_payload(input_path, audio.samples, &audio.sample_rate, &audio.channels)) {
    return false;
  }

  PipeWireSession session;
  session.mode         = PipeWireMode::PLAYBACK;
  session.play_samples = &audio.samples;
  session.sample_rate  = audio.sample_rate;
  session.channels     = audio.channels;
  session.update_level = true;

  const auto frame_count = audio.samples.size() / std::max<uint16_t>(1, audio.channels);
  const auto duration_ms =
      std::chrono::milliseconds(static_cast<int64_t>(frame_count * 1000 / audio.sample_rate));
  const bool played = run_pipewire_stream(session, duration_ms + std::chrono::seconds(5));
  audio_level().store(0.0F);
  return played;
}

struct KeyClickState {
  std::mutex mutex;
  std::string path;
  std::shared_ptr<WavAudio> audio;
  bool warmup_started{false};
};

KeyClickState& key_click_state() {
  static KeyClickState state;
  return state;
}

std::shared_ptr<WavAudio> load_key_click_audio(const std::string& input_path) {
  if (input_path.empty()) {
    return nullptr;
  }

  auto audio = std::make_shared<WavAudio>();
  if (!read_wav_payload(input_path, audio->samples, &audio->sample_rate, &audio->channels)) {
    return nullptr;
  }
  if (audio->sample_rate == 0 || audio->channels == 0 || audio->samples.empty()) {
    LOG_WARN("invalid key click sound: {}", input_path);
    return nullptr;
  }
  LOG_INFO("loaded key click sound: {} rate={} channels={} samples={}",
           input_path,
           audio->sample_rate,
           audio->channels,
           audio->samples.size());
  return audio;
}

bool play_key_click_pipewire(const WavAudio& audio) {
  PipeWireSession session;
  session.mode         = PipeWireMode::PLAYBACK;
  session.play_samples = &audio.samples;
  session.sample_rate  = audio.sample_rate;
  session.channels     = audio.channels;
  session.update_level = false;

  const auto frame_count = audio.samples.size() / std::max<uint16_t>(1, audio.channels);
  const auto duration_ms =
      std::chrono::milliseconds(static_cast<int64_t>(frame_count * 1000 / audio.sample_rate));
  return run_pipewire_stream(session, duration_ms + std::chrono::milliseconds(500));
}

void warm_up_key_click_pipewire(uint32_t sample_rate, uint16_t channels) {
  if (sample_rate == 0 || channels == 0) {
    return;
  }

  WavAudio silence;
  silence.sample_rate = sample_rate;
  silence.channels    = channels;
  silence.samples.resize(static_cast<std::size_t>(sample_rate) * channels / 5U, 0);
  LOG_INFO("warming up PipeWire key click stream: rate={} channels={}", sample_rate, channels);
  play_key_click_pipewire(silence);
}
#endif

}  // namespace

float current_audio_level() { return audio_level().load(); }

bool find_i2s_audio_device(AudioDevice& device, std::string& error_message) {
#if APP_USE_PIPEWIRE
  device.backend_name = "PipeWire";
  device.display_name = "PipeWire default source/sink";
  LOG_INFO("using PipeWire libpipewire audio backend");
  return true;
#else
  error_message =
      "PipeWire audio backend is not available. Install libpipewire-0.3 development "
      "headers in the build sysroot and rebuild.";
  LOG_ERROR("{}", error_message);
  return false;
#endif
}

bool record_wav(const AudioDevice& /*device*/, const std::string& output_path, int seconds) {
  LOG_INFO("recording WAV through PipeWire to {} for {}s", output_path, seconds);
#if APP_USE_PIPEWIRE
  return record_wav_pipewire(output_path, seconds);
#else
  return false;
#endif
}

bool play_wav(const AudioDevice& /*device*/, const std::string& input_path) {
  LOG_INFO("playing WAV through PipeWire from {}", input_path);
#if APP_USE_PIPEWIRE
  return play_wav_pipewire(input_path);
#else
  return false;
#endif
}

void set_key_click_sound_path(const std::string& input_path) {
#if APP_USE_PIPEWIRE
  auto& state = key_click_state();
  auto audio  = load_key_click_audio(input_path);
  bool should_warm_up = false;
  uint32_t warm_rate   = 0;
  uint16_t warm_ch     = 0;

  {
    std::lock_guard<std::mutex> lock(state.mutex);
    if (audio && !state.warmup_started) {
      state.warmup_started = true;
      should_warm_up       = true;
      warm_rate            = audio->sample_rate;
      warm_ch              = audio->channels;
    }
    state.path  = audio ? input_path : std::string{};
    state.audio = std::move(audio);
  }

  if (should_warm_up) {
    std::thread([warm_rate, warm_ch]() { warm_up_key_click_pipewire(warm_rate, warm_ch); })
        .detach();
  }
#else
  (void)input_path;
#endif
}

void play_key_click_sound() {
#if APP_USE_PIPEWIRE
  auto& state = key_click_state();
  std::shared_ptr<WavAudio> audio;
  {
    std::lock_guard<std::mutex> lock(state.mutex);
    audio = state.audio;
  }
  if (!audio) {
    return;
  }

  std::thread([audio]() { play_key_click_pipewire(*audio); }).detach();
#endif
}

}  // namespace platform::audio
