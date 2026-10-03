[sgcl](../../README.md) › [immutable](../README.md) › [set](../set/README.md) › [builder](README.md)

# sgcl::immutable::set\<Key, Hash, KeyEqual\>::builder::contains

```cpp
bool contains(const Key& key) const noexcept;                                     // (1)
template<class K> bool contains(const K& key) const noexcept(/* see below */);    // (2)
```

Checks whether the builder holds an element equal to `key` now.

1. Looks up `key`.
2. Looks up a key of another type without building a `Key`. Takes part only when `Hash::is_transparent` and
   `KeyEqual::is_transparent` are both types.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key to look up |

## Return value

`true` when the builder has an element equal to the key, `false` otherwise.

## Complexity

Logarithmic in `size()`, base 32, and one comparison of keys; one per element of a chain when hashes collide to
the last bit.

## Exceptions

- (1) None.
- (2) What `Hash` and `KeyEqual` called with a `K` throw; none when their calls are noexcept, as those of the
  function objects of `std` are taken to be.

## Example

```cpp
#include "sgcl/immutable.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    immutable::set<string>::builder b;
    b.insert("alice");
    println("{} {}", b.contains("alice"), b.contains("bob"));
}
```

Output:

```text
true false
```

## See also

- [insert](insert.md): adds an element
- [sgcl::immutable::set\<Key, Hash, KeyEqual\>::builder](README.md)
