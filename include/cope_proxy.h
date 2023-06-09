// cope_proxy.h

#ifndef INCLUDE_COPE_PROXY_H
#define INCLUDE_COPE_PROXY_H

#include <memory>
#include "cope_msg.h"

namespace cope {
  namespace proxy {
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
  } // namespace proxy

  namespace msg {
    // TODO: move to cope_msg.h, include cope_proxy.h
    template<typename T>
    //  requires requires (T x) { x.msg_id; }
    struct proxy_t : cope::proxy::raw_ptr_t<T> {
      using base_t = cope::proxy::raw_ptr_t<T>;

      proxy_t() = default;
      proxy_t(const T& msg) : base_t(msg) {}
      proxy_t(T&& msg) = delete;
    };

    template<>
    struct proxy_t<msg_ptr_t> : cope::proxy::unique_ptr_t<msg_t> {
      using base_t = cope::proxy::unique_ptr_t<msg_t>;

      proxy_t() = default;
      proxy_t(const msg_ptr_t& msg_ptr) = delete;
      proxy_t(msg_ptr_t&& msg_ptr) : base_t(std::move(msg_ptr)) {}
    };
  } // namespace msg

  // TODO: this whole thing probably not necessary if we add msg_proxyT
  //   as members in promise_type instead of inheriting
  namespace io {
    template<typename msg_proxyT>
      requires requires (msg_proxyT& p) {
        p.get(); p.get_moveable(); //TODO p.emplace();
      }
    struct proxy_t {
      auto& in() { return in_.get(); }
      [[nodiscard]] decltype(auto) moveable_in() { return in_.get_moveable(); }
      template<typename T> void emplace_in(T&& msg) {
        in_.emplace(std::forward<T>(msg));
      }

      auto& out() { return out_.get(); }
      [[nodiscard]] decltype(auto) moveable_out() { return out_.get_moveable(); }
      template<typename T> void emplace_out(T&& msg) {
        out_.emplace(std::forward<T>(msg));
      }

    private:
      msg_proxyT in_;
      msg_proxyT out_;
    };
  } // namespace io
} // namespace cope

#endif // INCLUDE_COPE_PROXY_H