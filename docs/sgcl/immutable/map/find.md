[sgcl](../../README.md) › [immutable](../README.md) › [map](README.md)

# sgcl::immutable::map\<Key, T, Hash, KeyEqual\>::find

```cpp
const_iterator find(const Key& key) const noexcept;                                     // (1)
template<class K> const_iterator find(const K& key) const noexcept(/* see below */);    // (2)
```

Returns an iterator to the element under `key`, or [end()](end.md) when the key is absent, as every `find` of the
library does. The bits of the hash are walked down, and the key is compared at the entry they lead to; `++` on
the iterator goes on from there in the order [begin()](begin.md) walks.

1. Looks up `key`.
2. Looks up a key of another type, a `string_view` or a literal for a `string` key, without building a `Key`.
   Takes part only when `Hash::is_transparent` and `KeyEqual::is_transparent` are both types.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key to look up |

## Return value

An iterator to the element, `end()` when there is none.

## Complexity

Logarithmic in `size()`, base 32, and one comparison of keys; one per element of a chain when hashes collide to
the last bit.

## Exceptions

- (1) None.
- (2) What `Hash` and `KeyEqual` called with a `K` throw; none when their calls are noexcept, as those of the
  function objects of `std` are taken to be.

## Notes

An iterator carries the path of nodes from the root; when only the value is wanted, `try_get` of
[mixin::lookup](../../core/mixin/lookup/README.md) is the cheaper read, a pointer to the value or null.

## Example

```cpp
#include "sgcl/immutable.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    immutable::map<string, int> ports = {{"http", 80}, {"https", 443}};
    auto it = ports.find("https");  // a literal: no string made
    if (it != ports.end()) {
        println("{} {}", it->first, it->second);
    }
    println("{}", ports.find("ftp") == ports.end());
}
```

Output:

```text
https 443
true
```

## See also

- [at](at.md): the value under a key, with bounds checking
- [contains](contains.md): checks whether the map has an element under a key
- [sgcl::immutable::map\<Key, T, Hash, KeyEqual\>](README.md)
