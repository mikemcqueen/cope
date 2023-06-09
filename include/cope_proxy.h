// cope_proxy.h

#ifndef INCLUDE_COPE_PROXY_H
#define INCLUDE_COPE_PROXY_H

#include <memory>
#include "cope_msg.h"

namespace cope {
  namespace msg {
    // msg stored as T*. beware lifetime issues.
    template<typename T>
    //  requires requires (T x) { x.msg_id; }
    struct proxy_t {
      using msg_type = T;

      proxy_t() = default;
      proxy_t(const T& msg) : msg_(&msg) {}
      proxy_t(T&& msg) = delete;

      const auto& get() const { return *msg_; }
      auto& get() { return const_cast<T&>(*msg_); }

      decltype(auto) get_moveable() const { return (*msg_); }//std::move(*msg_); }
      void emplace(const T& msg) { msg_ = &msg; }

    private:
      const T* msg_;
    };

    // msg stored as msg_ptr_t
    template<>
    struct proxy_t<msg_ptr_t> {
      using msg_type = msg_ptr_t;

      proxy_t() = default;
      proxy_t(const msg_ptr_t& msg_ptr) = delete;
      proxy_t(msg_ptr_t&& msg_ptr) : msg_ptr_(std::move(msg_ptr)) {}

      // const auto& get() const {}
      auto& get() { return *msg_ptr_.get(); }
      [[nodiscard]] auto&& get_moveable() { return std::move(msg_ptr_); }
      void emplace(msg_ptr_t&& msg_ptr) { msg_ptr_ = std::move(msg_ptr); }

    private:
      msg_type msg_ptr_;
    };
  } // namespace msg

  namespace io {
    template<typename msg_proxyT>
      requires requires (msg_proxyT& p) {
        p.get(); p.get_moveable(); //TODO p.emplace();
      }
    struct proxy_t {
      auto& in() { return in_.get(); }
      [[nodiscard]] //auto&&
      decltype(auto) moveable_in() { return in_.get_moveable(); }
//      void emplace_in(msg_proxyT& msg_proxy) { in_.emplace(msg_proxy.get_moveable()); }
      template<typename T>
      void emplace_in(/*typename msg_proxyT::msg_type*/T&& msg) {
        in_.emplace(std::forward<T>(msg));
      }
/*
template<>
      void emplace_in(msg_proxyT&& msg_proxy) {
        in_.emplace(std::forward<msg_proxyT>(msg_proxy).get_moveable());
      }
*/
      auto& out() { return out_.get(); }
      [[nodiscard]] //auto&&
      decltype(auto) moveable_out() { return out_.get_moveable(); }
/*
      void emplace_out(msg_proxyT& from) { out_.emplace(from.get_moveable()); }
*/
      template<typename T>
      void emplace_out(/*typename msg_proxyT::msg_type*/T&& msg) {
        out_.emplace(std::forward<T>(msg));
      }

    private:
      msg_proxyT in_;
      msg_proxyT out_;
    };
  } // namespace io
} // namespace cope::proxy

#endif // INCLUDE_COPE_PROXY_H