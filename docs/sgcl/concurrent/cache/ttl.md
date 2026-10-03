[sgcl](../../README.md) › [concurrent](../README.md) › [cache](../cache.md)

# sgcl::concurrent::cache\<Key, T, Hash, KeyEqual\>::ttl

```cpp
duration ttl() const noexcept;
```

Returns the time to live of an entry, the one given to the constructor: an entry put more than `ttl()` ago is
absent. Zero when the cache has none.

## Parameters

None.

## Return value

The time to live, or `duration::zero()`.

## Complexity

Constant.

## Exceptions

None.

## Notes

The time of an entry runs from its last `put`, not from its last `get`. A stale entry is erased by the `get` that
finds it, which misses, and by an eviction pass that walks past it; `get_or_compute` computes it again. The time is
[sgcl::clock](../../core/clock.md)'s, read only when the cache has a time to live: under an installed
[manual_clock](../../async/manual_clock.md) the entries age only as the test advances it, as in the example.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;
using namespace std::chrono_literals;

int main() {
    async::manual_clock clock;
    clock.install();  // the time moves only by advance

    concurrent::cache<string, string> tokens(100, 5min);
    tokens.put("ada", "7f3a");
    tokens.put("grace", "c01d");
    println("{}", tokens.ttl() == 5min);

    clock.advance(3min);
    tokens.put("ada", "9e2b");  // renewed: five minutes from now
    clock.advance(3min);
    for (const char* user : {"ada", "grace"}) {
        println("{} {}", user, tokens.get(user).has_value());
    }
    println("{} entry, {} missed", tokens.size(), tokens.misses());
}
```

Output:

```text
true
ada true
grace false
1 entry, 1 missed
```

## See also

- [get](get.md): misses an entry past its time, and erases it
- [put](put.md): renews the time of an entry
- [sgcl::concurrent::cache\<Key, T, Hash, KeyEqual\>](../cache.md)
