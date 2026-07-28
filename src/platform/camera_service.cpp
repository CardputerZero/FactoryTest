/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "camera_service.h"

#if defined(FACTORY_TEST_SCONS_BUILD)
#include "factory_test_config.h"
#endif

#include <algorithm>
#include <atomic>
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
#include <libcamera/base/shared_fd.h>
#include <libcamera/base/unique_fd.h>
#include <libcamera/formats.h>
#include <libcamera/framebuffer_allocator.h>
#include <libcamera/libcamera.h>
#endif

#if APP_USE_LIBCAMERA && (defined(__unix__) || defined(__APPLE__))
#include <fcntl.h>
#include <linux/dma-buf.h>
#include <linux/dma-heap.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace platform::camera {
namespace {

uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return static_cast<uint16_t>(((r & 0xF8U) << 8U) | ((g & 0xFCU) << 3U) | (b >> 3U));
}

#if APP_USE_LIBCAMERA
constexpr const char* K_DMA_HEAP_PATH = "/dev/dma_heap/default_cma_region";

const char* validation_status_name(libcamera::CameraConfiguration::Status status) {
  switch (status) {
    case libcamera::CameraConfiguration::Valid:
      return "valid";
    case libcamera::CameraConfiguration::Adjusted:
      return "adjusted";
    case libcamera::CameraConfiguration::Invalid:
      return "invalid";
  }
  return "unknown";
}

std::string error_text(int error_code) {
  return error_code > 0 ? std::strerror(error_code) : "unknown error";
}

uint8_t clamp_channel(int value) { return static_cast<uint8_t>(std::clamp(value, 0, 255)); }

class DmaHeap {
 public:
  bool open(const char* path, int& error_code) {
    error_code   = 0;
    const int fd = ::open(path, O_RDWR | O_CLOEXEC, 0);
    if (fd < 0) {
      error_code = errno;
      return false;
    }
    path_ = path;
    fd_   = libcamera::UniqueFD(fd);
    LOG_DEBUG("camera DMA heap opened: path={}", path_);
    return true;
  }

  libcamera::UniqueFD allocate(const std::string& name, std::size_t size, int& error_code) const {
    error_code = 0;
    if (!fd_.isValid()) {
      error_code = ENODEV;
      return {};
    }

    dma_heap_allocation_data allocation{};
    allocation.len      = size;
    allocation.fd_flags = O_CLOEXEC | O_RDWR;
    if (::ioctl(fd_.get(), DMA_HEAP_IOCTL_ALLOC, &allocation) < 0) {
      error_code = errno;
      return {};
    }

    libcamera::UniqueFD buffer_fd(static_cast<int>(allocation.fd));
    if (::ioctl(buffer_fd.get(), DMA_BUF_SET_NAME, name.c_str()) < 0) {
      error_code = errno;
      return {};
    }
    return buffer_fd;
  }

  const std::string& path() const { return path_; }

 private:
  std::string path_{};
  libcamera::UniqueFD fd_{};
};

class NativePreview {
 public:
  static constexpr int K_CAPTURE_WIDTH                 = 640;
  static constexpr int K_CAPTURE_HEIGHT                = 480;
  static constexpr int K_PREVIEW_WIDTH                 = 226;
  static constexpr int K_PREVIEW_HEIGHT                = 170;
  static constexpr unsigned int K_CAPTURE_BUFFER_COUNT = 3;

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

    info.name = camera_->id();
    if (!configure_and_allocate(info, error_message)) {
      return false;
    }

    latest_frame_.assign(K_PREVIEW_WIDTH * K_PREVIEW_HEIGHT, 0);
    conversion_frame_.assign(K_PREVIEW_WIDTH * K_PREVIEW_HEIGHT, 0);

    const int start_result = camera_->start();
    if (start_result != 0) {
      error_message = "Failed to start libcamera stream.";
      LOG_ERROR("{} code={} detail={}", error_message, start_result, error_text(-start_result));
      return false;
    }

    camera_started_ = true;
    running_.store(true);
    camera_->requestCompleted.connect(this, &NativePreview::request_complete);
    callback_connected_ = true;
    for (auto& request : requests_) {
      const int queue_result = camera_->queueRequest(request.get());
      if (queue_result < 0) {
        error_message = "Failed to queue libcamera capture request.";
        LOG_ERROR("{} code={} detail={}", error_message, queue_result, error_text(-queue_result));
        return false;
      }
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
    running_.store(false);
    if (camera_ && callback_connected_) {
      camera_->requestCompleted.disconnect(this, &NativePreview::request_complete);
      callback_connected_ = false;
    }
    if (camera_ && camera_started_) {
      const int stop_result = camera_->stop();
      if (stop_result != 0) {
        LOG_ERROR("failed to stop libcamera stream: code={} detail={}",
                  stop_result,
                  error_text(-stop_result));
      }
      camera_started_ = false;
    }
    clear_capture_resources();
    config_.reset();
    if (camera_) {
      camera_->release();
      camera_.reset();
    }
    if (manager_) {
      manager_->stop();
      manager_.reset();
    }
    LOG_INFO("native libcamera preview stopped");
  }

  bool running() const { return running_.load(); }

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

  bool prepare_configuration(unsigned int requested_buffer_count,
                             unsigned int& validated_buffer_count,
                             std::string& error_message) {
    validated_buffer_count = 0;
    config_                = camera_->generateConfiguration({libcamera::StreamRole::Viewfinder});
    if (!config_ || config_->empty()) {
      error_message = "Failed to create libcamera viewfinder configuration.";
      LOG_ERROR("{}", error_message);
      return false;
    }

    auto& cfg       = config_->at(0);
    cfg.size        = libcamera::Size(K_CAPTURE_WIDTH, K_CAPTURE_HEIGHT);
    cfg.pixelFormat = libcamera::formats::YUV420;
    cfg.stride      = 0;
    if (requested_buffer_count > 0) {
      cfg.bufferCount = requested_buffer_count;
    }

    const auto validation = config_->validate();
    LOG_DEBUG(
        "camera configuration validation: status={} requested={}x{} format={} requested_buffers={} "
        "validated={}x{} validated_format={} stride={} frameSize={} bufferCount={}",
        validation_status_name(validation),
        K_CAPTURE_WIDTH,
        K_CAPTURE_HEIGHT,
        libcamera::formats::YUV420.toString(),
        requested_buffer_count,
        cfg.size.width,
        cfg.size.height,
        cfg.pixelFormat.toString(),
        cfg.stride,
        cfg.frameSize,
        cfg.bufferCount);
    if (validation == libcamera::CameraConfiguration::Invalid) {
      error_message = "Libcamera rejected the viewfinder configuration.";
      LOG_ERROR("{} requested_buffers={}", error_message, requested_buffer_count);
      return false;
    }
    if (cfg.size != libcamera::Size(K_CAPTURE_WIDTH, K_CAPTURE_HEIGHT) ||
        cfg.pixelFormat != libcamera::formats::YUV420) {
      error_message = "Libcamera cannot provide the required 640x480 YUV420 stream.";
      LOG_ERROR("{} validated={}x{} format={}",
                error_message,
                cfg.size.width,
                cfg.size.height,
                cfg.pixelFormat.toString());
      return false;
    }

    validated_buffer_count     = cfg.bufferCount;
    const int configure_result = camera_->configure(config_.get());
    if (configure_result < 0) {
      error_message = "Failed to configure libcamera viewfinder stream.";
      LOG_ERROR("{} code={} detail={} frameSize={} bufferCount={}",
                error_message,
                configure_result,
                error_text(-configure_result),
                cfg.frameSize,
                cfg.bufferCount);
      return false;
    }

    stream_        = cfg.stream();
    source_width_  = static_cast<int>(cfg.size.width);
    source_height_ = static_cast<int>(cfg.size.height);
    source_stride_ = static_cast<int>(cfg.stride);
    source_size_   = static_cast<std::size_t>(cfg.frameSize);
    source_format_ = cfg.pixelFormat;
    LOG_DEBUG("camera stream configured: {}x{} format={} stride={} frameSize={} bufferCount={}",
              source_width_,
              source_height_,
              source_format_.toString(),
              source_stride_,
              source_size_,
              cfg.bufferCount);
    return true;
  }

  bool allocate_from_heap(const char* heap_path,
                          unsigned int buffer_count,
                          std::string& error_message) {
    clear_capture_resources();

    DmaHeap heap;
    int error_code = 0;
    if (!heap.open(heap_path, error_code)) {
      LOG_DEBUG("camera DMA heap unavailable: path={} code={} detail={}",
                heap_path,
                error_code,
                error_text(error_code));
      return false;
    }

    for (unsigned int index = 0; index < buffer_count; ++index) {
      const std::string name = "FactoryTest-camera-" + std::to_string(index);
      auto fd                = heap.allocate(name, source_size_, error_code);
      if (!fd.isValid()) {
        error_message = "Failed to allocate camera DMA buffer.";
        LOG_ERROR("{} heap={} index={} size={} bufferCount={} code={} detail={}",
                  error_message,
                  heap.path(),
                  index,
                  source_size_,
                  buffer_count,
                  error_code,
                  error_text(error_code));
        clear_capture_resources();
        return false;
      }

      std::vector<libcamera::FrameBuffer::Plane> planes(1);
      planes[0].fd     = libcamera::SharedFD(std::move(fd));
      planes[0].offset = 0;
      planes[0].length = static_cast<unsigned int>(source_size_);
      auto buffer      = std::make_unique<libcamera::FrameBuffer>(planes);

      void* address =
          mmap(nullptr, source_size_, PROT_READ | PROT_WRITE, MAP_SHARED, planes[0].fd.get(), 0);
      if (address == MAP_FAILED) {
        error_code    = errno;
        error_message = "Failed to map camera DMA buffer.";
        LOG_ERROR("{} heap={} index={} size={} code={} detail={}",
                  error_message,
                  heap.path(),
                  index,
                  source_size_,
                  error_code,
                  error_text(error_code));
        clear_capture_resources();
        return false;
      }

      mapped_buffers_[buffer.get()] = {address, source_size_};
      capture_buffers_.push_back(buffer.get());
      frame_buffers_.push_back(std::move(buffer));
      LOG_DEBUG("camera DMA buffer allocated: heap={} index={} size={} fd={}",
                heap.path(),
                index,
                source_size_,
                planes[0].fd.get());
    }

    LOG_DEBUG("camera DMA allocation complete: heap={} frameSize={} bufferCount={} total={}",
              heap.path(),
              source_size_,
              buffer_count,
              source_size_ * buffer_count);
    return true;
  }

  bool allocate_with_libcamera(unsigned int buffer_count, std::string& error_message) {
    clear_capture_resources();

    allocator_                  = std::make_unique<libcamera::FrameBufferAllocator>(camera_);
    const int allocation_result = allocator_->allocate(stream_);
    if (allocation_result < 0) {
      error_message = "Failed to allocate camera buffers through libcamera.";
      LOG_ERROR("{} backend=FrameBufferAllocator frameSize={} bufferCount={} code={} detail={}",
                error_message,
                source_size_,
                buffer_count,
                allocation_result,
                error_text(-allocation_result));
      clear_capture_resources();
      return false;
    }

    const auto& buffers = allocator_->buffers(stream_);
    LOG_DEBUG(
        "camera libcamera allocation result: frameSize={} requested_buffers={} result={} "
        "actual_buffers={}",
        source_size_,
        buffer_count,
        allocation_result,
        buffers.size());
    if (buffers.empty()) {
      error_message = "Libcamera returned no camera buffers.";
      LOG_ERROR("{} frameSize={} bufferCount={}", error_message, source_size_, buffer_count);
      clear_capture_resources();
      return false;
    }

    for (std::size_t index = 0; index < buffers.size(); ++index) {
      auto* buffer = buffers[index].get();
      if (!buffer || buffer->planes().empty()) {
        error_message = "Libcamera returned an invalid camera buffer.";
        LOG_ERROR("{} index={}", error_message, index);
        clear_capture_resources();
        return false;
      }

      const int fd            = buffer->planes()[0].fd.get();
      std::size_t plane_bytes = 0;
      for (const auto& plane : buffer->planes()) {
        if (plane.fd.get() != fd) {
          error_message = "Libcamera returned a non-contiguous YUV420 buffer.";
          LOG_ERROR("{} index={} planes={} frameSize={}",
                    error_message,
                    index,
                    buffer->planes().size(),
                    source_size_);
          clear_capture_resources();
          return false;
        }
        plane_bytes += plane.length;
      }

      void* address = mmap(nullptr, source_size_, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
      if (address == MAP_FAILED) {
        const int error_code = errno;
        error_message        = "Failed to map libcamera-allocated camera buffer.";
        LOG_ERROR("{} index={} size={} fd={} code={} detail={}",
                  error_message,
                  index,
                  source_size_,
                  fd,
                  error_code,
                  error_text(error_code));
        clear_capture_resources();
        return false;
      }

      mapped_buffers_[buffer] = {address, source_size_};
      capture_buffers_.push_back(buffer);
      LOG_DEBUG(
          "camera buffer mapped: backend=FrameBufferAllocator index={} frameSize={} planeBytes={} "
          "planes={} fd={}",
          index,
          source_size_,
          plane_bytes,
          buffer->planes().size(),
          fd);
    }

    LOG_DEBUG(
        "camera DMA allocation complete: backend=FrameBufferAllocator frameSize={} bufferCount={} "
        "total={}",
        source_size_,
        capture_buffers_.size(),
        source_size_ * capture_buffers_.size());
    return true;
  }

  bool create_requests(std::string& error_message) {
    requests_.clear();
    for (auto* buffer : capture_buffers_) {
      auto request = camera_->createRequest();
      if (!request) {
        error_message = "Failed to create libcamera capture request.";
        LOG_ERROR("{}", error_message);
        return false;
      }
      const int add_result = request->addBuffer(stream_, buffer);
      if (add_result < 0) {
        error_message = "Failed to attach DMA buffer to libcamera request.";
        LOG_ERROR("{} code={} detail={}", error_message, add_result, error_text(-add_result));
        return false;
      }
      requests_.push_back(std::move(request));
    }
    LOG_DEBUG("camera capture requests created: count={}", requests_.size());
    return true;
  }

  bool configure_and_allocate(CameraInfo& info, std::string& error_message) {
    unsigned int validated_count = 0;
    if (!prepare_configuration(K_CAPTURE_BUFFER_COUNT, validated_count, error_message)) {
      return false;
    }

    std::string last_allocation_error;
    std::string attempt_error;
    LOG_DEBUG(
        "camera DMA allocation attempt: requested_buffers={} validated_buffers={} frameSize={}",
        K_CAPTURE_BUFFER_COUNT,
        validated_count,
        source_size_);
    if (allocate_from_heap(K_DMA_HEAP_PATH, validated_count, attempt_error)) {
      if (!create_requests(attempt_error)) {
        error_message = attempt_error;
        clear_capture_resources();
        return false;
      }
      info.width  = source_width_;
      info.height = source_height_;
      error_message.clear();
      return true;
    }
    if (!attempt_error.empty()) {
      last_allocation_error = attempt_error;
    }

    LOG_DEBUG(
        "camera DMA allocation fallback: backend=FrameBufferAllocator bufferCount={} frameSize={}",
        validated_count,
        source_size_);
    attempt_error.clear();
    if (allocate_with_libcamera(validated_count, attempt_error)) {
      if (!create_requests(attempt_error)) {
        error_message = attempt_error;
        clear_capture_resources();
        return false;
      }
      info.width  = source_width_;
      info.height = source_height_;
      error_message.clear();
      return true;
    }
    error_message = !attempt_error.empty()
                        ? attempt_error
                        : (!last_allocation_error.empty()
                               ? last_allocation_error
                               : "Failed to allocate camera DMA buffers from all backends.");
    LOG_ERROR("{}", error_message);
    return false;
  }

  void clear_capture_resources() {
    requests_.clear();
    for (auto& item : mapped_buffers_) {
      if (item.second.address && item.second.address != MAP_FAILED) {
        munmap(item.second.address, item.second.length);
      }
    }
    mapped_buffers_.clear();
    capture_buffers_.clear();
    frame_buffers_.clear();
    if (allocator_ && stream_) {
      const int free_result = allocator_->free(stream_);
      if (free_result < 0) {
        LOG_ERROR("failed to free libcamera camera buffers: code={} detail={}",
                  free_result,
                  error_text(-free_result));
      }
    }
    allocator_.reset();
  }

  bool sync_buffer(libcamera::FrameBuffer* buffer, uint64_t flags) const {
    if (!buffer || buffer->planes().empty()) {
      return false;
    }
    dma_buf_sync sync{};
    sync.flags = flags;
    if (::ioctl(buffer->planes()[0].fd.get(), DMA_BUF_IOCTL_SYNC, &sync) < 0) {
      const int error_code = errno;
      LOG_ERROR("camera DMA sync failed: flags={} code={} detail={}",
                flags,
                error_code,
                error_text(error_code));
      return false;
    }
    return true;
  }

  void copy_framebuffer(libcamera::FrameBuffer* buffer) {
    auto it = mapped_buffers_.find(buffer);
    if (it == mapped_buffers_.end() || !it->second.address) {
      return;
    }
    if (source_width_ <= 0 || source_height_ <= 0 || source_stride_ <= 0) {
      return;
    }
    const std::size_t y_plane_size  = static_cast<std::size_t>(source_stride_) * source_height_;
    const std::size_t uv_stride     = static_cast<std::size_t>(source_stride_) / 2;
    const std::size_t uv_plane_size = uv_stride * (source_height_ / 2);
    if (source_format_ != libcamera::formats::YUV420 ||
        y_plane_size + uv_plane_size * 2 > it->second.length) {
      if (!unsupported_format_logged_) {
        LOG_ERROR("invalid camera YUV420 buffer layout: format={} stride={} frameSize={} mapped={}",
                  source_format_.toString(),
                  source_stride_,
                  source_size_,
                  it->second.length);
        unsupported_format_logged_ = true;
      }
      return;
    }
    if (!sync_buffer(buffer, DMA_BUF_SYNC_START | DMA_BUF_SYNC_READ)) {
      return;
    }

    const auto* src     = static_cast<const uint8_t*>(it->second.address);
    const auto* y_plane = src;
    const auto* u_plane = y_plane + y_plane_size;
    const auto* v_plane = u_plane + uv_plane_size;
    for (int y = 0; y < K_PREVIEW_HEIGHT; ++y) {
      // The camera module is mounted opposite to the LVGL preview orientation.
      const int sy = source_height_ - 1 - (y * source_height_ / K_PREVIEW_HEIGHT);
      for (int x = 0; x < K_PREVIEW_WIDTH; ++x) {
        const int sx      = source_width_ - 1 - (x * source_width_ / K_PREVIEW_WIDTH);
        const int y_value = y_plane[static_cast<std::size_t>(sy) * source_stride_ + sx];
        const int u_value = u_plane[static_cast<std::size_t>(sy / 2) * uv_stride + sx / 2];
        const int v_value = v_plane[static_cast<std::size_t>(sy / 2) * uv_stride + sx / 2];
        const int c       = std::max(0, y_value - 16);
        const int d       = u_value - 128;
        const int e       = v_value - 128;
        const auto r      = clamp_channel((298 * c + 409 * e + 128) >> 8);
        const auto g      = clamp_channel((298 * c - 100 * d - 208 * e + 128) >> 8);
        const auto b      = clamp_channel((298 * c + 516 * d + 128) >> 8);
        conversion_frame_[static_cast<std::size_t>(y) * K_PREVIEW_WIDTH + x] = rgb565(r, g, b);
      }
    }
    const bool sync_ended = sync_buffer(buffer, DMA_BUF_SYNC_END | DMA_BUF_SYNC_READ);
    if (sync_ended) {
      std::lock_guard<std::mutex> lock(frame_mutex_);
      latest_frame_.swap(conversion_frame_);
      frame_ready_ = true;
    }
  }

  void request_complete(libcamera::Request* request) {
    if (!running_.load() || !request || request->status() == libcamera::Request::RequestCancelled) {
      return;
    }
    if (request->status() == libcamera::Request::RequestComplete && !request->buffers().empty()) {
      copy_framebuffer(request->buffers().begin()->second);
    }
    request->reuse(libcamera::Request::ReuseBuffers);
    const int queue_result = camera_->queueRequest(request);
    if (queue_result < 0 && running_.load()) {
      LOG_ERROR("failed to recycle camera request: code={} detail={}",
                queue_result,
                error_text(-queue_result));
    }
  }

  std::unique_ptr<libcamera::CameraManager> manager_{};
  std::shared_ptr<libcamera::Camera> camera_{};
  std::unique_ptr<libcamera::CameraConfiguration> config_{};
  std::unique_ptr<libcamera::FrameBufferAllocator> allocator_{};
  libcamera::Stream* stream_{nullptr};
  std::vector<std::unique_ptr<libcamera::FrameBuffer>> frame_buffers_{};
  std::vector<libcamera::FrameBuffer*> capture_buffers_{};
  std::vector<std::unique_ptr<libcamera::Request>> requests_{};
  std::map<libcamera::FrameBuffer*, MappedBuffer> mapped_buffers_{};
  mutable std::mutex frame_mutex_{};
  std::vector<uint16_t> latest_frame_{};
  std::vector<uint16_t> conversion_frame_{};
  libcamera::PixelFormat source_format_{};
  int source_width_{0};
  int source_height_{0};
  int source_stride_{0};
  std::size_t source_size_{0};
  std::atomic_bool running_{false};
  bool camera_started_{false};
  bool callback_connected_{false};
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
  {
    const auto cameras = manager.cameras();
    if (cameras.empty()) {
      error_message = "MIPI-CSI camera not detected.";
      LOG_ERROR("{}", error_message);
      manager.stop();
      return false;
    }
    camera.name = cameras.front()->id();
  }
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
