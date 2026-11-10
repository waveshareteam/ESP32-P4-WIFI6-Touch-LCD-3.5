/* SPDX-License-Identifier: Apache-2.0 */
#include "XiaozhiCameraPreview.hpp"
#include "esp_heap_caps.h"
#include <cstring>

namespace esp_brookesia::apps {
XiaozhiCameraPreview::XiaozhiCameraPreview()
    : _mutex(xSemaphoreCreateMutexStatic(&_mutex_storage)) {}
XiaozhiCameraPreview::~XiaozhiCameraPreview()
{
    heap_caps_free(_pixels);
    if (_mutex) vSemaphoreDelete(_mutex);
}
XiaozhiCameraPreview &xiaozhiCameraPreview()
{
    static XiaozhiCameraPreview preview;
    return preview;
}
bool XiaozhiCameraPreview::active(uint32_t session) const
{
    return (session & 1U) && _session.load() == session;
}
void XiaozhiCameraPreview::setEnabled(bool enabled)
{
    if (!_mutex) return;
    xSemaphoreTake(_mutex, portMAX_DELAY);
    const uint32_t session = _session.load();
    if (static_cast<bool>(session & 1U) != enabled) {
        _session.store(session + 1);
        heap_caps_free(_pixels);
        _pixels = nullptr;
        _snapshot = {};
        _frozen = false;
        _reply_started = _reply_stopped = false;
        ++_revision;
    }
    xSemaphoreGive(_mutex);
}
bool XiaozhiCameraPreview::begin(uint32_t session)
{
    if (!_mutex) return false;
    xSemaphoreTake(_mutex, portMAX_DELAY);
    bool ok = active(session);
    if (ok) {
        if (!_pixels) _pixels = static_cast<uint16_t *>(heap_caps_malloc(
            BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        _snapshot = {Phase::Preview, false};
        _frozen = false;
        _reply_started = _reply_stopped = false;
        ++_revision;
        ok = _pixels != nullptr;
    }
    xSemaphoreGive(_mutex);
    return ok;
}
bool XiaozhiCameraPreview::publish(uint32_t session, const Camera::CaptureFrame &frame, bool final_frame)
{
    if (!_mutex || !frame.pixels || !frame.width || !frame.height ||
        (frame.bytes_per_pixel != 2 && frame.bytes_per_pixel != 3) ||
        frame.width > SIZE_MAX / frame.bytes_per_pixel ||
        frame.stride < size_t(frame.width) * frame.bytes_per_pixel ||
        frame.height > frame.size / frame.stride) return false;
    xSemaphoreTake(_mutex, portMAX_DELAY);
    const bool ok = active(session) && _pixels && !_frozen;
    if (ok) {
        // Fit the full uploaded image without cropping, stretching or rotation.
        memset(_pixels, 0, BYTES);
        unsigned w = WIDTH, h = HEIGHT;
        if (uint64_t(frame.width) * HEIGHT > uint64_t(frame.height) * WIDTH)
            h = unsigned(uint64_t(WIDTH) * frame.height / frame.width);
        else w = unsigned(uint64_t(HEIGHT) * frame.width / frame.height);
        if (!w) w = 1;
        if (!h) h = 1;
        for (unsigned y = 0; y < h; ++y) {
            const size_t row = size_t(uint64_t(y) * frame.height / h) * frame.stride;
            for (unsigned x = 0; x < w; ++x) {
                const auto *p = frame.pixels + row + size_t(uint64_t(x) * frame.width / w) * frame.bytes_per_pixel;
                uint16_t pixel;
                if (frame.bytes_per_pixel == 2) memcpy(&pixel, p, sizeof(pixel));
                else pixel = uint16_t((p[2] >> 3) << 11 | (p[1] >> 2) << 5 | (p[0] >> 3));
                _pixels[(y + (HEIGHT-h)/2) * WIDTH + x + (WIDTH-w)/2] = pixel;
            }
        }
        _snapshot = {final_frame ? Phase::Encoding : Phase::Preview, true};
        _frozen = final_frame;
        ++_revision;
    }
    xSemaphoreGive(_mutex);
    return ok;
}
void XiaozhiCameraPreview::setPhase(uint32_t session, Phase phase)
{
    if (!_mutex) return;
    xSemaphoreTake(_mutex, portMAX_DELAY);
    if (active(session) && _snapshot.phase != phase) {
        _snapshot.phase = phase;
        _reply_started = _reply_stopped = false;
        ++_revision;
    }
    xSemaphoreGive(_mutex);
}
void XiaozhiCameraPreview::dismissLocked()
{
    // Keep the enabled bit, but reject callbacks holding the old token.
    if (_session.load() & 1U) _session.fetch_add(2);
    heap_caps_free(_pixels);
    _pixels = nullptr;
    _snapshot = {};
    _frozen = false;
    _reply_started = _reply_stopped = false;
    ++_revision;
}
void XiaozhiCameraPreview::dismiss()
{
    if (!_mutex) return;
    xSemaphoreTake(_mutex, portMAX_DELAY);
    dismissLocked();
    xSemaphoreGive(_mutex);
}
void XiaozhiCameraPreview::noteSpeech(bool stopped)
{
    if (!_mutex) return;
    xSemaphoreTake(_mutex, portMAX_DELAY);
    if (_snapshot.phase == Phase::Complete || _snapshot.phase == Phase::Error) {
        if (stopped) _reply_stopped = _reply_started;
        else {
            _reply_started = true;
            _reply_stopped = false;
        }
    }
    xSemaphoreGive(_mutex);
}
bool XiaozhiCameraPreview::finishPlayback()
{
    if (!_mutex) return false;
    xSemaphoreTake(_mutex, portMAX_DELAY);
    const bool restore = _reply_stopped &&
        (_snapshot.phase == Phase::Complete || _snapshot.phase == Phase::Error);
    if (restore) dismissLocked();
    xSemaphoreGive(_mutex);
    return restore;
}
bool XiaozhiCameraPreview::read(uint32_t &revision, uint16_t *pixels, Snapshot &snapshot)
{
    if (!_mutex || xSemaphoreTake(_mutex, 0) != pdTRUE) return false;
    const bool changed = revision != _revision;
    if (changed) {
        snapshot = _snapshot;
        if (snapshot.has_frame && pixels) memcpy(pixels, _pixels, BYTES);
        if (!snapshot.has_frame || pixels) revision = _revision;
    }
    xSemaphoreGive(_mutex);
    return changed;
}
}
