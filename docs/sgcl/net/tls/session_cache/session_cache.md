[sgcl](../../../README.md) › [net](../../README.md) › [tls](../README.md) › [session_cache](README.md)

# sgcl::net::tls::session_cache::session_cache

```cpp
explicit session_cache(size_t capacity = 64) noexcept;    // (1)
session_cache(const session_cache& other) noexcept;       // (2), implicitly declared
session_cache(session_cache&& other) noexcept;            // (3), implicitly declared
```

1. An empty cache of at most `capacity` sessions, 64 by default as Go's `tls.NewLRUClientSessionCache(0)`; a
   capacity of 0 keeps none, a cache that turns resumption off while its config still names one.
2. A handle of the same cache as `other`.
3. The same; `other` is still the cache, since the move of the word inside is its copy.

## Parameters

| Parameter | Description |
|---|---|
| `capacity` | the sessions held at most, of every server together |
| `other` | the handle whose cache this one shares |

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/tls.h"

using namespace sgcl;

int main() {
    net::tls::session_cache sessions;
    net::tls::session_cache small(2);
    net::tls::session_cache copy = small;
    println("{} {} {}", sessions.capacity(), small.capacity(), copy.capacity());
    println("{}", net::tls::session_cache(0).capacity());
}
```

Output:

```text
64 2 2
0
```

## See also

- [capacity](capacity.md): the capacity given
- [sgcl::net::tls::session_cache](README.md)
