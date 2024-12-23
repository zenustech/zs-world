#pragma once
#include <string>
#include <vector>

#include "zensim/ZpcIterator.hpp"

namespace zs {

  constexpr size_t g_max_serialization_size_limit = detail::deduce_numeric_max<u32>();

  using SerializationBuffer = std::vector<char>;
  /// @note a contiguous char array
  using SerializationInputBuffer
      = decltype(zs::range(zs::declval<const char *>(), zs::declval<const char *>()));

}  // namespace zs

#if ZS_ENABLE_SERIALIZATION
#  include "zensim/zpc_tpls/bitsery/adapter/buffer.h"
#  include "zensim/zpc_tpls/bitsery/bitsery.h"
#  include "zensim/zpc_tpls/bitsery/ext/inheritance.h"
#  include "zensim/zpc_tpls/bitsery/ext/pointer.h"
#  include "zensim/zpc_tpls/bitsery/ext/std_map.h"
#  include "zensim/zpc_tpls/bitsery/ext/std_smart_ptr.h"
#  include "zensim/zpc_tpls/bitsery/ext/std_tuple.h"
#  include "zensim/zpc_tpls/bitsery/traits/string.h"
#  include "zensim/zpc_tpls/bitsery/traits/vector.h"

namespace zs {

  using BitseryPointerObserver = bitsery::ext::PointerObserver;
  using BitseryPointerOwner = bitsery::ext::PointerOwner;
  using BitseryPointerType = bitsery::ext::PointerType;
  using BitseryReferencedByPointer = bitsery::ext::ReferencedByPointer;
  using BitseryStdSmartPtr = bitsery::ext::StdSmartPtr;

  using BitseryOutputAdapter = bitsery::OutputBufferAdapter<SerializationBuffer>;
  using BitseryWriter = BitseryOutputAdapter;
  using BitseryInputAdapter = bitsery::InputBufferAdapter<SerializationInputBuffer>;
  using BitseryReader = BitseryInputAdapter;

  // using BitseryContext;
  // bitsery::ext::PointerLinkingContext;
  // bitsery::ext::PolymorphicContext<bitsery::ext::StandardRTTI>;
  using BitserySerializer = bitsery::Serializer<BitseryWriter, bitsery::ext::PointerLinkingContext>;
  using BitseryDeserializer
      = bitsery::Deserializer<BitseryReader, bitsery::ext::PointerLinkingContext>;

}  // namespace zs

#endif