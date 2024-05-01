// cope_msg.h

#ifndef INCLUDE_COPE_MSG_H
#define INCLUDE_COPE_MSG_H

using namespace std::literals;

namespace cope {
  namespace msg {
    using id_t = int; // HACK TODO

    template<typename MsgT, typename StateT>
    struct start_txn_t {
      MsgT msg;
      StateT state;
    };
  } // namespace msg

} // namespace cope

#endif // INCLUDE_COPE_MSG_H
