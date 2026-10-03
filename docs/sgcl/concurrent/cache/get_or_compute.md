[sgcl](../../README.md) › [concurrent](../README.md) › [cache](README.md)

# sgcl::concurrent::cache\<Key, T, Hash, KeyEqual\>::get_or_compute

```cpp
template<class F>
T get_or_compute(const Key& key, F&& f);
```

Returns the value under `key`, or, when the key is absent or its entry stale, `f()` computed, put under the key and
returned. The lookup is a [get](get.md), counted as a hit or a miss; the value computed is put as by [put](put.md),
with the eviction it owes.

Two threads that miss the same key at once both call `f`; one insertion wins and both return the value that won,
so `f` is a function of the key alone, called once or more, and never while anything is locked. Guava's
`get(key, loader)` blocks the second thread instead; this one blocks nothing, at the price of the second
computation. When the entry another thread put meanwhile is stale already, the value computed replaces it.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key of the entry |
| `f` | the function that computes the value, called with no arguments, returning a `T` or what converts to one |

## Return value

A copy of the value under `key`: the one found, the one computed, or the one another thread put meanwhile.

## Complexity

A `get`; on a miss, the call of `f` and a `put`.

## Exceptions

What `f`, the copy of `Key` or the copy or the move of `T` throws.

If `f` throws, nothing is put and the cache is as the `get` left it. The copy of `Key` and of the value into the
entry come before the value is put: when one throws, nothing is put either. The copy or the move of the value
returned may throw after the value is put: the value is then in the cache.

## Notes

Lock-free as `get` and `put` are, and `f` is called outside of every exchange: a slow computation holds no other
thread back.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::cache<int, int> squares(100);
    int computed = 0;
    auto square = [&](int n) {
        return squares.get_or_compute(n, [&] {
            ++computed;  // the slow part
            return n * n;
        });
    };

    int first = square(12);
    int second = square(12);
    println("{} {}", first, second);
    println("{} computed, {} hit, {} missed", computed, squares.hits(), squares.misses());
}
```

Output:

```text
144 144
1 computed, 1 hit, 1 missed
```

## See also

- [get](get.md): the lookup alone
- [put](put.md): inserts or replaces a value
- [sgcl::concurrent::cache\<Key, T, Hash, KeyEqual\>](README.md)
