[sgcl](../../README.md) › [core](../README.md) › [tracked_ptr](../tracked_ptr.md)

# sgcl::tracked_ptr\<T\>::if_alive

```cpp
tracked_ptr if_alive() const noexcept;
```

For destructors. Returns a copy of the pointer, or null when its target is dying in the same sweep as the object
being destroyed. An object dies together with everything reachable only from it, in no particular order and on
several threads, so a `tracked_ptr` member of a dying object may point at an object destroyed already, and the
collector does not null such members beforehand. Outside a sweep, `if_alive()` is a plain copy: a `tracked_ptr` in a
live object points at a live object.

## Parameters

None.

## Return value

A copy of the pointer when its target lives, null when the target dies in the same sweep or the pointer is null.

## Complexity

Constant.

## Exceptions

None.

## Notes

A pointer to a dying object is never handed out; a live one may be used and stored freely, in the middle of a sweep
too ([Pointer maps](../../../garbage_collector/overview.md#pointer-maps); [The rules](../README.md#the-rules), 5).
A destructor reads its `unique_ptr` members freely: what they own is not garbage of the sweep.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Peer {
    int id;
};

struct Holder {
    ~Holder() {
        if (tracked_ptr p = peer.if_alive()) {
            println("holder {}: peer {} lives", id, p->id);
        } else {
            println("holder {}: the peer dies with it", id);
        }
    }
    int id;
    tracked_ptr<Peer> peer;
};

int main() {
    tracked_ptr shared = make_tracked<Peer>(1);
    tracked_ptr first = make_tracked<Holder>(1, shared);
    tracked_ptr second = make_tracked<Holder>(2, make_tracked<Peer>(2));

    first = nullptr;
    collector::force_collect(true);  // optional, for the demonstration: the cycle runs ~Holder
    second = nullptr;
    collector::force_collect(true);
    println("peer {} still here", shared->id);
}
```

Output:

```text
holder 1: peer 1 lives
holder 2: the peer dies with it
peer 1 still here
```

## See also

- [weak_ptr](../weak_ptr.md): a pointer that keeps nothing alive
- [collector](../collector.md): `force_collect`
- [sgcl::tracked_ptr\<T\>](../tracked_ptr.md)
