#include "Signal.hpp"

namespace zs {

  static ConnectionIDGenerator<u32> g_connectionIdGenerator;

  u32 next_connection_id() { return g_connectionIdGenerator.nextId(); }
  void recycle_connection_id(u32 id) { return g_connectionIdGenerator.recycleId(id); }

}  // namespace zs