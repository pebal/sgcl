[sgcl](../../README.md) › [concurrent](../README.md) › [sorted_set](README.md)

# sgcl::concurrent::sorted_set\<Key, Compare\>::find

```cpp
iterator find(const Key& key) noexcept;                                // (1)
const_iterator find(const Key& key) const noexcept;                    // (2)
template<class K> iterator find(const K& key) noexcept;                // (3)
template<class K> const_iterator find(const K& key) const noexcept;    // (4)
```

Finds the key of the set equivalent to `key`. The search descends from the top level in use, along each level
while the keys are less than `key`, stepping over erased nodes without touching them, down to the bottom list,
where the first node whose key is not less than `key` is the one sought when its key is not greater either.

- (3–4) Take part only when `Compare` declares `is_transparent`: the key is of any type the comparison takes, and
  no `Key` is built for the search (a `string_view` or a literal for a [string](../../core/string/README.md) key).

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key to find |

## Return value

An iterator to the key in the set, or [end()](end.md) when the set holds none equivalent to `key`.

## Complexity

Logarithmic in the size of the set, expected.

## Exceptions

None.

## Notes

Wait-free, and the search writes nothing: the erased nodes it meets are left to the insertions and erasures to
unlink. The iterator holds the node: when another thread erases the key after the search found it, the iterator
still reads it, and the key lives for as long as the iterator does. The key found is the set's own object, which
may differ from `key` in what the comparison ignores.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::sorted_set<string> users = {"ada", "grace", "linus"};

    if (auto it = users.find("grace"); it != users.end()) {  // a literal: no string made
        println("{}", *it);
    }
    println("{}", users.find("bjarne") == users.end());
}
```

Output:

```text
grace
true
```

## See also

- [contains](contains.md): checks whether the set holds a key
- [lower_bound](lower_bound.md): the first key from a key on
- [sgcl::concurrent::sorted_set\<Key, Compare\>](README.md)
