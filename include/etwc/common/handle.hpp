#pragma once

#include <windows.h>

#include <utility>

namespace etwc {

// RAII cho HANDLE của Win32: tự CloseHandle, chống rò rỉ trên mọi đường thoát.
// NULL và INVALID_HANDLE_VALUE đều coi là "rỗng".
class UniqueHandle {
public:
    UniqueHandle() = default;
    explicit UniqueHandle(HANDLE h) : h_(normalize(h)) {}

    UniqueHandle(const UniqueHandle&) = delete;
    UniqueHandle& operator=(const UniqueHandle&) = delete;

    UniqueHandle(UniqueHandle&& o) noexcept : h_(std::exchange(o.h_, nullptr)) {}
    UniqueHandle& operator=(UniqueHandle&& o) noexcept {
        if (this != &o) {
            reset();
            h_ = std::exchange(o.h_, nullptr);
        }
        return *this;
    }

    ~UniqueHandle() { reset(); }

    void reset(HANDLE h = nullptr) {
        if (h_ != nullptr)
            ::CloseHandle(h_);
        h_ = normalize(h);
    }

    HANDLE get() const { return h_; }
    explicit operator bool() const { return h_ != nullptr; }
    HANDLE* put() { return &h_; }  // nhận HANDLE từ API ra tham số

private:
    static HANDLE normalize(HANDLE h) { return h == INVALID_HANDLE_VALUE ? nullptr : h; }
    HANDLE h_ = nullptr;
};

}  // namespace etwc
