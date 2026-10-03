[sgcl](../../README.md) › [concurrent](../README.md) › [cache](../cache.md)

# sgcl::concurrent::cache\<Key, T, Hash, KeyEqual\>::cache

```cpp
explicit cache(size_type capacity, duration ttl = duration::zero(),           // (1)
               unsigned sample = DefaultSample, const Hash& hash = Hash(),
               const KeyEqual& equal = KeyEqual())
    noexcept(std::is_nothrow_copy_constructible_v<Hash> &&
             std::is_nothrow_copy_constructible_v<KeyEqual>);
cache(const cache&) = delete;                                                 // (2)
```

1. An empty cache that keeps `capacity` entries, none older than `ttl` (no time to live when `ttl` is zero),
   looking at `sample` entries per eviction; `hash` and `equal` go to the map underneath, which gets its buckets
   for the capacity up front (16 at least).
2. The cache is not copyable, and not movable: a structure shared by threads has one place.

## Parameters

| Parameter | Description |
|---|---|
| `capacity` | the number of entries the cache keeps; 0 keeps nothing, every `put` inserts and evicts |
| `ttl` | the time to live of an entry, from its last `put`; zero for none, and a negative one makes every entry stale from its `put`. Any `std::chrono::duration` of whole units converts to `duration` |
| `sample` | the number of entries an eviction looks at, `DefaultSample` (8) by default; 0 is taken as 1 |
| `hash` | the hash of the keys |
| `equal` | the equality of the keys |

## Complexity

Linear in the number of buckets made for the capacity.

## Exceptions

What the copy of `Hash` or `KeyEqual` throws; none when it is noexcept.

## Notes

The capacity, the time to live and the sample are fixed for the life of the cache, and read by
[capacity](capacity.md), [ttl](ttl.md) and [sample_size](sample_size.md). A sample of 16 approximates the order of
an exact LRU closer than the default 8, at a longer eviction: a `put` at capacity measured 1000 ns against 740
([Benchmarks: The cost of a cache operation](../benchmarks.md#the-cost-of-a-cache-operation)). A capacity whose
buckets the managed heap cannot give, up to `SIZE_MAX`, ends the program as any refused managed allocation does
([collector](../../core/collector.md#the-memory-limit)): the map gets its buckets for the capacity up front, as
[map](../map/map.md) does.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <type_traits>

using namespace sgcl;
using namespace std::chrono_literals;

struct Session {
    int user;
};

struct Service {
    concurrent::cache<string, string> tokens{1000, 5min};  // a member of a managed object
};

int main() {
    concurrent::cache<int, tracked_ptr<Session>> sessions(10000);  // no expiry
    tracked_ptr service = make_tracked<Service>();
    concurrent::cache<string, int> precise(100, {}, 16);  // sixteen entries per eviction

    println("{} {} {}", sessions.capacity(), sessions.ttl() == 0s, sessions.sample_size());
    println("{} {}", service->tokens.capacity(), service->tokens.ttl() == 5min);
    println("{}", precise.sample_size());
    println("{}", std::is_copy_constructible_v<concurrent::cache<int, int>>);
}
```

Output:

```text
10000 true 8
1000 true
16
false
```

## See also

- [put](put.md): inserts a value, and evicts down to the capacity
- [sgcl::concurrent::cache\<Key, T, Hash, KeyEqual\>](../cache.md)
