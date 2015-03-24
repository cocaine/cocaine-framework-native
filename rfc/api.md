- Start Date: 25.01.2015

# Summary

Stabilize Framework API

# Motivation

To prevent spontaneous API changes just because someone wants it.

# Detailed design

The lowest level class is the connection (may be renamed to session). It does almost all low level network logic. It's very likely that it will be hidden from the userscope.

The main class you will deal with - is a service. It represents a proxy with auto resolving and RAII style.

It's very unlikely that you will create service class instances manually. Some entity called service manager should be designed to create it for you.

RAII style means that its destructor will block until all internal pending callbacks are called, which results in asynchronous invocation of all futures that you have gotten (via `then`).

A service can be detached, then its destructor won't block.

All you need to create a service class instance via service manager it its name and prototype. 

```
auto node = manager.create<T>("node");
```

The obtained service is initially in disconnected state. To resolve and connect manually you need to call `connect()` method, which returns flagged future. You can always check is the service is in connected state by calling `connected()` method. Note, that this method does passive connection checking, and it can result in false-positive value if the real connection is already broken, but there are no internal callbacks attached for reading or writing at that time.

The service supports auto reconnection on new requests.

To invoke remote method you need to call `invoke<Event>(args...)` method with appropriate event protocol description and its parameters. This one returns you a future of pair of sender/receiver, by manipulating with each of them you can control an upstream and downstream.

```
template<class T>
class service {
    ~service();
  
    void detach();
  
    auto connected() const noexcept -> bool;
    auto connect() -> future<void>;

    template<class Event, class... Args>
    auto invoke(Args&&... args) -> future<std::tuple<sender<T>, receiver<T>>>;
};
```

Cocaine upstream and downstream are completely separated entities. In the common case when you send another event in the open channel you make one step forward your protocol transition graph. There are terminate leafs, which means you cannot send another event when you reach it. On the receiver side it is completely identical image - there are receiver state, after reaching which you cannot receive more chunks.

This complex graph structure requires more runtime checks which can be theoretically replaced with static checks, which gives us the following API.

Transmission channel does step forward after invoking `send` method, which accepts event type and its arguments described in protocol `dispatch_type`. It returns the following transmission state, which can be reassigned to the old `tx`, because it's useless now, which gives us an ability to chain requests:

After termination leaf reached you is unable send more chunks, because there won't be such method in terminated sender.

```
tx = tx.send<C>(args...).get(); // Simple send, can throw if unable to properly pack given arguments or any network error received.

tx.send<C1>(args1...).get().send<C2>(args2...).get().send<C3>(args3...).get(); // Chained.
```

In the opposite side there is a received, from which you can receive and extract incoming chunks. Unfortunately it should return both the new receiver type and chunk requested.

```
std::tie(rx, result) = rx.recv().get(); // Can throw on any network error.
```

In the common case result type is an algebraic data type, which consists of types described in Cocaine Service's protocol `upstream_type`.

At this moment there are only two possible result types: option and stream. They can be described as:

```
option<T> ::= T | tuple<int, string>.
stream<T> ::= T | tuple<int, string> | choke.
```

All described above means that after receiving a result from the receiver future all you need is to visit obtained variant type.

I've just described the commonest protocol description I can imagine. One have imagined it's extremely complex API.

The good news is it can be simplified for 95% cases.

If you work with primitive `upstream_type`and with void `dispatch_type` which the most services are, then the calling interface can be simplified to:

```
// Returns a value on success, throws `cocaine_error` on service error or `system_error` on network error.
try {
    service.invoke<Event>(args...).get(); 
} catch (const cocaine_error& err) {
    // ...
} catch (const std::system_error& err) {
    // ...
}
```

If you work with streaming `upstream_type` and with `streaming<std::string>` `dispatch_type` which all the user applications are, then the interface could be represented as:

```
std::tie(tx, rx) = service.invoke<cocaine::io::app::enqueue>("event");
tx.send<protocol::chunk>("le message").get()
  .send<protocol::error>(-1, "the user gone mad").get();

auto result = rx.recv();    // -> variant<T, E, C> where T == dispatch_type, E - error type, C - choke.
auto result = rx.recv<T>(); // -> optional<T> or throws `cocaine_error` on error.
```

# Drawbacks

Why should we *not* do this?

# Alternatives

What other designs have been considered? What is the impact of not doing this?

# Unresolved questions

What parts of the design are still TBD?

