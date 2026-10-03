[sgcl](../../README.md) › [concurrent](../README.md) › [cache](../cache.md)

# sgcl::concurrent::cache\<Key, T, Hash, KeyEqual\>::get

```cpp
optional<T> get(const Key& key) noexcept(std::is_nothrow_copy_constructible_v<T>);    // (1)
template<class K> optional<T> get(const K& key)                                       // (2)
    noexcept(std::is_nothrow_copy_constructible_v<T>);
```

Returns a copy of the value under `key`, or nothing: the key is absent, or its entry is older than the time to
live, in which case the `get` erases it. A hit stores the cache's current tick into the entry, the stamp the
evictions compare, and only when it differs from the one there.

1. Looks up `key`.
2. Looks up a key of another type, with no `Key` built for the search: a `string_view` or a literal for a `string`
   key. Takes part only when `Hash` and `KeyEqual` both have `is_transparent`.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key to look up |

## Return value

A copy of the value, or `nullopt` when the key is absent or stale. `optional` is the alias of `std::optional`
([aliases](../../core/aliases.md)).

## Complexity

Constant on average: the map's search, a load of the entry's box, the tick, the stamp and the thread's stripe.

## Exceptions

What the copy constructor of `T` throws; none when it is noexcept.

When the copy throws, the cache is as the `get` found it, but for the stamp of the entry and the count of the
hit.

## Notes

A `get` that finds the entry fresh, or no entry, is wait-free and writes nothing shared but the stamp, one relaxed
store, and the count of the hit or the miss on the thread's own stripe. A `get` that finds the entry stale erases
it, the map's lock-free erasure, and counts a miss. The value is copied out because another thread may evict the
entry the moment after: the copy is the caller's, whatever happens to the entry.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <string_view>

using namespace sgcl;

int main() {
    concurrent::cache<string, int> ages(10);
    ages.put("Ada", 36);

    if (auto age = ages.get("Ada")) {  // a literal: no string made for the search
        println("Ada is {}", *age);
    }
    std::string_view name = "Grace";
    println("{}", ages.get(name).has_value());
    println("{} hit, {} missed", ages.hits(), ages.misses());
}
```

Output:

```text
Ada is 36
false
1 hit, 1 missed
```

## See also

- [get_or_compute](get_or_compute.md): computes and puts the value the `get` did not find
- [put](put.md): inserts or replaces a value
- [hits](hits.md), [misses](misses.md): the counts of the gets
- [sgcl::concurrent::cache\<Key, T, Hash, KeyEqual\>](../cache.md)
