// traits.h

#pragma once

namespace cope {
  template <class, template <class...> class>
  struct is_instance_of : std::false_type {};

  template <class... Ts, template <class...> class U>
  struct is_instance_of<U<Ts...>, U> : std::true_type {};
}
