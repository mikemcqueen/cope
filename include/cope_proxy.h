// cope_proxy.h

#ifndef INCLUDE_COPE_PROXY_H
#define INCLUDE_COPE_PROXY_H

#include <memory>
#include "cope_msg.h"

namespace cope {
  
  namespace msg {
    // msg stored as T
    template<typename T>
    //  requires requires (T x) { x.msg_id; }
    struct proxy_t {
      using msg_type = T;

      proxy_t() = default;
      proxy_t(T& msg) : msg_(msg) {}
      proxy_t(T&& msg) : msg_(std::move(msg)) {}

      auto& get() { return msg_; }
      [[nodiscard]] auto&& get_moveable() { return std::move(msg_); }
      void emplace(T&& msg) { msg_ = std::move(msg); }

    private:
      T msg_;
    };

    // msg stored as msg_ptr_t
    template<>
    struct proxy_t<msg_ptr_t> {
      using msg_type = msg_ptr_t;

      proxy_t() = default;
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
      [[nodiscard]] auto&& moveable_in() { return in_.get_moveable(); }
      void emplace_in(msg_proxyT& from) { in_.emplace(from.get_moveable()); }
      void emplace_in(typename msg_proxyT::msg_type&& msg) {
        in_.emplace(std::move(msg));
      }
      //    [[nodiscard]] msg_ptr_t&& in_ptr() { return std::move(in_ptr_); }

      auto& out() { return out_.get(); }
      [[nodiscard]] auto&& moveable_out() { return out_.get_moveable(); }
      void emplace_out(msg_proxyT& from) { out_.emplace(from.get_moveable()); }
      void emplace_out(typename msg_proxyT::msg_type&& msg) {
        out_.emplace(std::move(msg));
      }
      //    [[nodiscard]] msg_ptr_t&& out_ptr() { return std::move(out_ptr_); }

    private:
      msg_proxyT in_;
      msg_proxyT out_;
    };

/*
    // io stored as msg_ptr_t
    template<>
    struct proxy_t<msg_ptr_t> {
      auto& in() const { return *in_ptr_.get(); }
      // TODO: moveable_in
      [[nodiscard]] msg_ptr_t&& in_ptr() { return std::move(in_ptr_); }
      void emplace_in(msg_ptr_t&& in_ptr) { in_ptr_ = std::move(in_ptr); }

      auto& out() const { return *out_ptr_.get(); }
      // TODO: moveable_out
      [[nodiscard]] msg_ptr_t&& out_ptr() { return std::move(out_ptr_); }
      void emplace_out(msg_ptr_t&& out_ptr) { out_ptr_ = std::move(out_ptr); }

    private:
      msg_ptr_t in_ptr_;
      msg_ptr_t out_ptr_;
    };
*/

    /*
      // stored as T
      struct copyable_t {
        auto& in() const { return *in_ptr_.get(); }
        void emplace_in(msg_ptr_t in_ptr) { in_ptr_ = std::move(in_ptr); }
        [[nodiscard]] msg_ptr_t&& in_ptr() { return std::move(in_ptr_); }

        auto& out() const { return *out_ptr_.get(); }
        void emplace_out(msg_ptr_t out_ptr) { out_ptr_ = std::move(out_ptr); }
        [[nodiscard]] msg_ptr_t&& out_ptr() { return std::move(out_ptr_); }

        T in_;
        T out_;
      };
    */
  } // namespace io
} // namespace cope::proxy

#endif // INCLUDE_COPE_PROXY_H