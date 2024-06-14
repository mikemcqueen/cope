// cope_msg.h

#ifndef INCLUDE_COPE_MSG_H
#define INCLUDE_COPE_MSG_H

#include "tuple.h"

namespace cope {
  // TODO: move to... context/handler/??
  enum class operation : int {
    yield,
    await,
    complete
  };

  namespace msg {
    using id_t = int; // HACK TODO

    template<typename MsgT, typename StateT>
    struct start_txn_t {
      MsgT msg;
      StateT state;
    }; // start_txn_t

    template<typename... Ts>
    // TODO: requires Ts declare: in_tuple_t, out_tuple_t
    struct type_bundle_t {
      using in_concat_type = tuple::concat_t<typename Ts::in_tuple_t...>;
      using in_tuple_type = tuple::distinct_t<in_concat_type>;
      using in_msg_type = tuple::to_variant_t<in_tuple_type>;

      using out_concat_type = tuple::concat_t<std::tuple<std::monostate>,
        typename Ts::out_tuple_t...>;
      using out_tuple_type = tuple::distinct_t<out_concat_type>;
      using out_msg_type = tuple::to_variant_t<out_tuple_type>;
    }; // type_bundle_t
  } // namespace msg
} // namespace cope

#endif // INCLUDE_COPE_MSG_H
