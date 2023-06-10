// cope_proxy.h

#ifndef INCLUDE_COPE_PROXY_H
#define INCLUDE_COPE_PROXY_H

#include <memory>

namespace cope::proxy {
  // TODO: what if i want msg.members to be moved?
  //   maybe it just works. test it with state that deletes
  //   copy constructor.
  template<typename T>
  struct raw_ptr_t {
    using type = T;

    raw_ptr_t() = default;
    // beware lifetime issues.
    raw_ptr_t(const T& msg) : msg_(&msg) {}
    raw_ptr_t(T&& msg) = delete;

    const auto& get() const { return *msg_; }
    // ugly, yes. but this is the magic that makes it all work.
    auto& get() { return const_cast<T&>(*msg_); }

    auto& get_moveable() const { return *msg_; }
    void emplace(const T& msg) { msg_ = &msg; }

  private:
    const T* msg_;
  };

  template<typename T>
  struct unique_ptr_t {
    using type = T;
    using ptr_type = std::unique_ptr<T>;

    unique_ptr_t() = default;
    unique_ptr_t(const ptr_type& ptr) = delete;
    unique_ptr_t(ptr_type&& ptr) : ptr_(std::move(ptr)) {}

    // const auto& get() const {}?
    auto& get() { return *ptr_.get(); }
    [[nodiscard]] auto&& get_moveable() { return std::move(ptr_); }
    void emplace(ptr_type&& ptr) { ptr_ = std::move(ptr); }

  private:
    ptr_type ptr_;
  };
} // namespace cope::proxy

#endif // INCLUDE_COPE_PROXY_H