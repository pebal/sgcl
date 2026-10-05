[sgcl](../../../README.md) › [net](../../README.md) › [tls](../README.md) › [revocation_cache](README.md)

# sgcl::net::tls::revocation_cache::revocation_cache

```cpp
explicit revocation_cache(size_t capacity = 256) noexcept;    // (1)
revocation_cache(const revocation_cache& other) noexcept;     // (2), implicitly declared
revocation_cache(revocation_cache&& other) noexcept;          // (3), implicitly declared
```

1. An empty cache of at most `capacity` entries, 256 by default; a capacity of 0 keeps none, every check then
   fetching again.
2. A handle of the same cache as `other`.
3. The same; `other` is still the cache, since the move of the word inside is its copy.

## Parameters

| Parameter | Description |
|---|---|
| `capacity` | the entries held at most, OCSP answers and CRLs together |
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
    net::tls::revocation_cache answers;
    net::tls::revocation_cache small(2);
    net::tls::revocation_cache copy = small;
    println("{} {} {}", answers.capacity(), small.capacity(), copy.capacity());
    println("{}", net::tls::revocation_cache(0).capacity());
}
```

Output:

```text
256 2 2
0
```

## See also

- [capacity](capacity.md): the capacity given
- [sgcl::net::tls::revocation_cache](README.md)
