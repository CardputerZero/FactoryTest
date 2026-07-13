/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "camera_service.h"

#if defined(FACTORY_TEST_SCONS_BUILD)
#include "factory_test_config.h"
#endif

#include <cerrno>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "logger.h"

#ifndef APP_USE_LIBCAMERA
#define APP_USE_LIBCAMERA 0
#endif

#if APP_USE_LIBCAMERA
#include <libcamera/formats.h>
#include <libcamera/libcamera.h>
#endif

#if APP_USE_LIBCAMERA && (defined(__unix__) || defined(__APPLE__))
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace platform::camera {
namespace {

uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return static_cast<uint16_t>(((r & 0xF8U) << 8U) | ((g & 0xFCU) << 3U) | (b >> 3U));
}

#if APP_USE_LIBCAMERA
class NativePreview {
 public:
  static constexpr int K_PREVIEW_WIDTH  = 226;
  static constexpr int K_PREVIEW_HEIGHT = 170;

  ~NativePreview() { stop(); }

  bool start(CameraInfo& info, std::string& error_message) {
    LOG_DEBUG("starting native libcamera preview pipeline");
    manager_ = std::make_unique<libcamera::CameraManager>();
    if (manager_->start() != 0) {
      error_message = "Failed to start libcamera CameraManager.";
      LOG_ERROR("{}", error_message);
      return false;
    }

    const auto cameras = manager_->cameras();
    if (cameras.empty()) {
      error_message = "MIPI-CSI camera not detected.";
      LOG_ERROR("{}", error_message);
      return false;
    }

    camera_ = cameras.front();
    if (camera_->acquire() != 0) {
      error_message = "Failed to acquire libcamera device.";
      LOG_ERROR("{}", error_message);
      return false;
    }

    config_ = camera_->generateConfiguration({libcamera::StreamRole::Viewfinder});
    if (!config_ || config_->empty()) {
      error_message = "Failed to create libcamera viewfinder configuration.";
      LOG_ERROR("{}", error_message);
      return false;
    }

    auto& cfg       = config_->at(0);
    cfg.size        = libcamera::Size(640, 480);
    cfg.pixelFormat = libcamera::formats::RGB888;
    cfg.bufferCount = 4;
    config_->validate();
    info.name   = camera_->id();
    info.width  = static_cast<int>(cfg.size.width);
    info.height = static_cast<int>(cfg.size.height);

    if (camera_->configure(config_.get()) != 0) {
      error_message = "Failed to configure libcamera viewfinder stream.";
      LOG_ERROR("{}", error_message);
      return false;
    }

    stream_        = cfg.stream();
    source_width_  = static_cast<int>(cfg.size.width);
    source_height_ = static_cast<int>(cfg.size.height);
    source_stride_ = static_cast<int>(cfg.stride);
    source_format_ = cfg.pixelFormat;
    latest_frame_.assign(K_PREVIEW_WIDTH * K_PREVIEW_HEIGHT, 0);

    allocator_ = std::make_unique<libcamera::FrameBufferAllocator>(camera_);
    if (allocator_->allocate(stream_) < 0) {
      error_message = "Failed to allocate libcamera frame buffers.";
      LOG_ERROR("{}", error_message);
      return false;
    }

    for (const auto& buffer : allocator_->buffers(stream_)) {
      if (!map_buffer(buffer.get())) {
        error_message = "Failed to map libcamera frame buffer.";
        LOG_ERROR("{}", error_message);
        return false;
      }
      std::unique_ptr<libcamera::Request> request = camera_->createRequest();
      if (!request || request->addBuffer(stream_, buffer.get()) < 0) {
        error_message = "Failed to create libcamera capture request.";
        LOG_ERROR("{}", error_message);
        return false;
      }
      requests_.push_back(std::move(request));
    }

    camera_->requestCompleted.connect(this, &NativePreview::request_complete);
    if (camera_->start() != 0) {
      error_message = "Failed to start libcamera stream.";
      LOG_ERROR("{}", error_message);
      return false;
    }

    running_ = true;
    for (auto& request : requests_) {
      camera_->queueRequest(request.get());
    }

    LOG_INFO("native libcamera preview started: {} {}x{} format={} stride={}",
             info.name,
             info.width,
             info.height,
             source_format_.toString(),
             source_stride_);
    return true;
  }

  void stop() {
    if (!camera_) {
      return;
    }
    running_ = false;
    camera_->requestCompleted.disconnect(this, &NativePreview::request_complete);
    camera_->stop();
    requests_.clear();
    unmap_buffers();
    if (allocator_ && stream_) {
      allocator_->free(stream_);
    }
    allocator_.reset();
    stream_ = nullptr;
    camera_->release();
    camera_.reset();
    if (manager_) {
      manager_->stop();
      manager_.reset();
    }
    LOG_INFO("native libcamera preview stopped");
  }

  bool running() const { return running_; }

  bool copy_frame(std::vector<uint16_t>& frame, int& width, int& height) const {
    std::lock_guard<std::mutex> lock(frame_mutex_);
    if (!frame_ready_) {
      return false;
    }
    frame  = latest_frame_;
    width  = K_PREVIEW_WIDTH;
    height = K_PREVIEW_HEIGHT;
    return true;
  }

 private:
  struct MappedBuffer {
    void* address{nullptr};
    std::size_t length{0};
  };

  bool map_buffer(libcamera::FrameBuffer* buffer) {
    if (!buffer || buffer->planes().empty()) {
      return false;
    }
    const auto& plane = buffer->planes()[0];
    const auto offset =
        plane.offset == libcamera::FrameBuffer::Plane::kInvalidOffset ? 0 : plane.offset;
    void* address = mmap(nullptr, plane.length, PROT_READ, MAP_SHARED, plane.fd.get(), offset);
    if (address == MAP_FAILED) {
      LOG_ERROR("failed to mmap libcamera buffer: {}", strerror(errno));
      return false;
    }
    mapped_buffers_[buffer] = {address, plane.length};
    return true;
  }

  void unmap_buffers() {
    for (auto& item : mapped_buffers_) {
      if (item.second.address && item.second.address != MAP_FAILED) {
        munmap(item.second.address, item.second.length);
      }
    }
    mapped_buffers_.clear();
  }

  bool read_rgb_pixel(const uint8_t* row, int x, uint8_t& r, uint8_t& g, uint8_t& b) {
    if (source_format_ == libcamera::formats::RGB888) {
      b = row[x * 3 + 0];
      g = row[x * 3 + 1];
      r = row[x * 3 + 2];
      return true;
    }
    if (source_format_ == libcamera::formats::BGR888) {
      r = row[x * 3 + 0];
      g = row[x * 3 + 1];
      b = row[x * 3 + 2];
      return true;
    }
    if (source_format_ == libcamera::formats::XRGB8888 ||
        source_format_ == libcamera::formats::ARGB8888) {
      b = row[x * 4 + 0];
      g = row[x * 4 + 1];
      r = row[x * 4 + 2];
      return true;
    }
    if (source_format_ == libcamera::formats::XBGR8888 ||
        source_format_ == libcamera::formats::ABGR8888) {
      r = row[x * 4 + 0];
      g = row[x * 4 + 1];
      b = row[x * 4 + 2];
      return true;
    }
    if (source_format_ == libcamera::formats::RGB565) {
      const uint16_t pixel = *reinterpret_cast<const uint16_t*>(row + x * 2);
      r                    = static_cast<uint8_t>(((pixel >> 11U) & 0x1FU) << 3U);
      g                    = static_cast<uint8_t>(((pixel >> 5U) & 0x3FU) << 2U);
      b                    = static_cast<uint8_t>((pixel & 0x1FU) << 3U);
      return true;
    }
    return false;
  }

  void copy_framebuffer(libcamera::FrameBuffer* buffer) {
    auto it = mapped_buffers_.find(buffer);
    if (it == mapped_buffers_.end() || !it->second.address) {
      return;
    }
    if (source_width_ <= 0 || source_height_ <= 0 || source_stride_ <= 0) {
      return;
    }
    std::vector<uint16_t> frame(K_PREVIEW_WIDTH * K_PREVIEW_HEIGHT);
    const auto* src = static_cast<const uint8_t*>(it->second.address);
    for (int y = 0; y < K_PREVIEW_HEIGHT; ++y) {
      // The camera module is mounted opposite to the LVGL preview orientation.
      const int sy    = source_height_ - 1 - (y * source_height_ / K_PREVIEW_HEIGHT);
      const auto* row = src + sy * source_stride_;
      for (int x = 0; x < K_PREVIEW_WIDTH; ++x) {
        const int sx = source_width_ - 1 - (x * source_width_ / K_PREVIEW_WIDTH);
        uint8_t r = 0, g = 0, b = 0;
        if (!read_rgb_pixel(row, sx, r, g, b)) {
          if (!unsupported_format_logged_) {
            LOG_ERROR("unsupported libcamera preview pixel format: {}", source_format_.toString());
            unsupported_format_logged_ = true;
          }
          return;
        }
        frame[static_cast<std::size_t>(y) * K_PREVIEW_WIDTH + x] = rgb565(r, g, b);
      }
    }
    std::lock_guard<std::mutex> lock(frame_mutex_);
    latest_frame_ = std::move(frame);
    frame_ready_  = true;
  }

  void request_complete(libcamera::Request* request) {
    if (!running_ || !request || request->status() == libcamera::Request::RequestCancelled) {
      return;
    }
    if (request->status() == libcamera::Request::RequestComplete && !request->buffers().empty()) {
      copy_framebuffer(request->buffers().begin()->second);
    }
    request->reuse(libcamera::Request::ReuseBuffers);
    camera_->queueRequest(request);
  }

  std::unique_ptr<libcamera::CameraManager> manager_{};
  std::shared_ptr<libcamera::Camera> camera_{};
  std::unique_ptr<libcamera::CameraConfiguration> config_{};
  std::unique_ptr<libcamera::FrameBufferAllocator> allocator_{};
  libcamera::Stream* stream_{nullptr};
  std::vector<std::unique_ptr<libcamera::Request>> requests_{};
  std::map<libcamera::FrameBuffer*, MappedBuffer> mapped_buffers_{};
  mutable std::mutex frame_mutex_{};
  std::vector<uint16_t> latest_frame_{};
  libcamera::PixelFormat source_format_{};
  int source_width_{0};
  int source_height_{0};
  int source_stride_{0};
  bool running_{false};
  bool frame_ready_{false};
  bool unsupported_format_logged_{false};
};
#endif

}  // namespace

PreviewSession::~PreviewSession() { stop(); }

bool PreviewSession::start(const CameraInfo& camera, std::string& error_message) {
  stop();

#if APP_USE_LIBCAMERA
  auto info    = camera;
  auto* native = new NativePreview();
  if (native->start(info, error_message)) {
    native_ = native;
    return true;
  }
  delete native;
  return false;
#else
  (void)camera;
  error_message = "Native libcamera support is not enabled in this build.";
  LOG_ERROR("{}", error_message);
  return false;
#endif
}

void PreviewSession::stop() {
#if APP_USE_LIBCAMERA
  if (native_) {
    static_cast<NativePreview*>(native_)->stop();
    delete static_cast<NativePreview*>(native_);
    native_ = nullptr;
  }
#endif
}

bool PreviewSession::running() const {
#if APP_USE_LIBCAMERA
  if (native_) {
    return static_cast<NativePreview*>(native_)->running();
  }
#endif
  return false;
}

bool PreviewSession::copy_frame_rgb565(std::vector<uint16_t>& frame,
                                       int& width,
                                       int& height) const {
#if APP_USE_LIBCAMERA
  if (native_) {
    return static_cast<const NativePreview*>(native_)->copy_frame(frame, width, height);
  }
#endif
  (void)frame;
  (void)width;
  (void)height;
  return false;
}

bool find_mipi_csi_camera(CameraInfo& camera, std::string& error_message) {
#if APP_USE_LIBCAMERA
  LOG_DEBUG("initializing native libcamera MIPI-CSI camera discovery");
  libcamera::CameraManager manager;
  if (manager.start() != 0) {
    error_message = "Failed to start libcamera CameraManager.";
    LOG_ERROR("{}", error_message);
    return false;
  }
  const auto cameras = manager.cameras();
  if (cameras.empty()) {
    error_message = "MIPI-CSI camera not detected.";
    LOG_ERROR("{}", error_message);
    manager.stop();
    return false;
  }
  camera.name   = cameras.front()->id();
  camera.width  = 640;
  camera.height = 480;
  manager.stop();
  LOG_INFO("detected MIPI-CSI camera through native libcamera: {}", camera.name);
  return true;
#else
  (void)camera;
  error_message = "Native libcamera support is not enabled in this build.";
  LOG_ERROR("{}", error_message);
  return false;
#endif
}

}  // namespace platform::camera
