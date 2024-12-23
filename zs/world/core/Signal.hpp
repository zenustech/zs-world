#pragma once
#include <atomic>
#include <cassert>
#include <map>
#include <vector>

#include "world/WorldExport.hpp"
#include "zensim/ZpcBuiltin.hpp"
#include "zensim/execution/ConcurrencyPrimitive.hpp"

namespace zs {

  template <typename T = u32> struct ConnectionIDGenerator {
    T nextId() noexcept {
      _mutex.lock();
      if (_recycled.size() > 0) {
        auto ret = _recycled.back();
        _recycled.pop_back();
        _mutex.unlock();
        return ret;
      }
      _mutex.unlock();
      return _curId++;
    }
    /// @note user must ensure this is the distributed one
    void recycleId(T id) {
      _mutex.lock();
      _recycled.push_back(id);
      _mutex.unlock();
    }

    std::atomic<T> _curId{1};  //, _size{0};
    std::vector<T> _recycled{};
    zs::Mutex _mutex;
  };

  ZS_WORLD_EXPORT u32 next_connection_id();
  ZS_WORLD_EXPORT void recycle_connection_id(u32 id);

  template <typename SignalT> struct Connection {
    using signal_type = SignalT;
    using callable_t = typename SignalT::callable_t;
    using handle_t = typename SignalT::handle_t;

    template <typename F> Connection(signal_type &signal, F &&f, handle_t id)
        : _signal{signal}, _slot{zs::forward<F>(f)}, _handle{id} {}

    void disconnect() {
      if (_linked) {
        _handle = 0;
        _linked = false;
      }
    }
    Connection(Connection &&o) = delete;
    Connection(const Connection &o) = delete;
    Connection &operator=(Connection &&o) = delete;
    Connection &operator=(const Connection &o) = delete;
    ~Connection();

    handle_t getConnectionId() const noexcept { return _handle; }
    decltype(auto) getSlot() noexcept { return _slot; }

    signal_type &_signal;
    callable_t _slot;
    handle_t _handle{0};
    bool _linked{true};
  };

  template <typename Signature> struct Signal;

  template <typename... Args> struct Signal<void(Args...)> {
    using handle_t = u32;
    using callable_t = function<void(Args...)>;
    using callables_t = std::vector<callable_t>;
    using R = void;

    auto newConnection(callable_t slot) { return addConnection(zs::move(slot)); }
    bool removeConnection(handle_t connectionId) {
      if (auto it = _connections.find(connectionId); it != _connections.end()) {
        it->second->disconnect();
        _connections.erase(it);
        return true;
      }
      return false;
    }
    ///
    /// unnamed callables
    ///
    void connect(callable_t slot) { addSlot(zs::move(slot)); }

    template <typename R_, typename... As_> void connect(R_ (*method)(As_...)) {
      addSlot([method](As_... args) -> R { return (*method)(static_cast<As_ &&>(args)...); });
    }

    /// mem func
    template <typename Object, typename Class, typename R_, typename... As_>
    void connect(Object &obj, R_ (Class::*memfn)(As_...)) {
      static_assert(is_convertible_v<R_, R> && (is_convertible_v<Args, As_> && ...),
                    "member func return/param type should be convertible to the signature "
                    "return/param type.");
      addSlot(
          [&obj, memfn](Args... args) -> R { return (obj.*memfn)(static_cast<Args &&>(args)...); });
    }

    template <typename Object, typename Class, typename R_, typename... As_>
    void connect(Object *obj, R_ (Class::*memfn)(As_...)) {
      static_assert(is_convertible_v<R_, R> && (is_convertible_v<Args, As_> && ...),
                    "member func return/param type should be convertible to the signature "
                    "return/param type.");
      addSlot([&obj = *obj, memfn](Args... args) -> R {
        return (obj.*memfn)(static_cast<Args &&>(args)...);
      });
    }

    /// mem object (callable)
    template <typename Object, typename Class, typename MemObj>
    void connect(Object &obj, MemObj(Class::*memobj)) {
      addSlot([&obj, memobj](Args... args) -> R {
        return (obj.*memobj)(static_cast<Args &&>(args)...);
      });
    }

    template <typename Object, typename Class, typename MemObj>
    void connect(Object *obj, MemObj(Class::*memobj)) {
      addSlot([&obj = *obj, memobj](Args... args) -> R {
        return (obj.*memobj)(static_cast<Args &&>(args)...);
      });
    }

    ///
    ///
    ///
    void assign(handle_t hd, callable_t slot) { assignSlot(hd, zs::move(slot)); }

    template <typename R_, typename... As_> void assign(handle_t hd, R_ (*method)(As_...)) {
      assignSlot(hd,
                 [method](As_... args) -> R { return (*method)(static_cast<As_ &&>(args)...); });
    }

    /// mem func
    template <typename Object, typename Class, typename R_, typename... As_>
    void assign(handle_t hd, Object &obj, R_ (Class::*memfn)(As_...)) {
      static_assert(is_convertible_v<R_, R> && (is_convertible_v<Args, As_> && ...),
                    "member func return/param type should be convertible to the signature "
                    "return/param type.");
      assignSlot(hd, [&obj, memfn](Args... args) -> R {
        return (obj.*memfn)(static_cast<Args &&>(args)...);
      });
    }

    template <typename Object, typename Class, typename R_, typename... As_>
    void assign(handle_t hd, Object *obj, R_ (Class::*memfn)(As_...)) {
      static_assert(is_convertible_v<R_, R> && (is_convertible_v<Args, As_> && ...),
                    "member func return/param type should be convertible to the signature "
                    "return/param type.");
      assignSlot(hd, [&obj = *obj, memfn](Args... args) -> R {
        return (obj.*memfn)(static_cast<Args &&>(args)...);
      });
    }

    /// mem object (callable)
    template <typename Object, typename Class, typename MemObj>
    void assign(handle_t hd, Object &obj, MemObj(Class::*memobj)) {
      assignSlot(hd, [&obj, memobj](Args... args) -> R {
        return (obj.*memobj)(static_cast<Args &&>(args)...);
      });
    }

    template <typename Object, typename Class, typename MemObj>
    void assign(handle_t hd, Object *obj, MemObj(Class::*memobj)) {
      assignSlot(hd, [&obj = *obj, memobj](Args... args) -> R {
        return (obj.*memobj)(static_cast<Args &&>(args)...);
      });
    }

    bool removeSlot(handle_t hd) { return _taggedCallables.erase(hd); }

    ///
    /// call
    ///
    void operator()(Args... args) {
      for (auto &callable : _callables) callable(zs::forward<Args>(args)...);
      for (auto &&[hd, callable] : _taggedCallables) callable(zs::forward<Args>(args)...);
      if (_connections.size()) {
        auto connections = _connections;  // _connections may be deleted
        for (auto &&[hd, connection] : connections)
          connection->getSlot()(zs::forward<Args>(args)...);
      }
    }
    void emit(Args... args) { operator()(FWD(args)...); }

  protected:
    template <typename F> void addSlot(F &&f) { _callables.emplace_back(FWD(f)); }
    template <typename F> void assignSlot(handle_t hd, F &&f) { _taggedCallables[hd] = FWD(f); }
    template <typename F> Connection<Signal> *addConnection(F &&f) {
      auto id = next_connection_id();
      auto ret = new Connection<Signal>(*this, FWD(f), id);
      _connections.insert_or_assign(id, ret);
      // _connections[ret->_handle] = ret;
      return ret;
    }

    callables_t _callables;
    std::map<handle_t, callable_t> _taggedCallables;
    std::map<handle_t, Connection<Signal> *> _connections;
  };

  template <typename SignalT> Connection<SignalT>::~Connection() {
    if (_linked) {
      bool res = _signal.removeConnection(_handle);
      assert(res);
      disconnect();
    }
  }

}  // namespace zs