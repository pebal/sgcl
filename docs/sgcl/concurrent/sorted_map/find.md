[sgcl](../../README.md) › [concurrent](../README.md) › [sorted_map](../sorted_map.md)

# sgcl::concurrent::sorted_map\<Key, T, Compare\>::find

```cpp
/*(1)*/ iterator find(const Key& key) noexcept;
/*(2)*/ const_iterator find(const Key& key) const noexcept;
/*(3)*/ template<class K> iterator find(const K& key) noexcept;
/*(4)*/ template<class K> const_iterator find(const K& key) const noexcept;
```

Finds the element whose key is equivalent to `key`. The search descends from the top level in use, along each
level while the keys are less than `key`, stepping over erased nodes without touching them, down to the bottom
list, where the first node whose key is not less than `key` is the element when its key is not greater either.

- (3–4) Take part only when `Compare` declares `is_transparent`: the key is of any type the comparison takes, and
  no `Key` is built for the search (a `string_view` or a literal for a [string](../../core/string.md) key).

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key of the element to find |

## Return value

An iterator to the element, or [end()](end.md) when the map holds no element under `key`.

## Complexity

Logarithmic in the size of the map, expected.

## Exceptions

None.

## Notes

Wait-free, and the search writes nothing: the erased nodes it meets are left to the insertions and erasures to
unlink. The iterator holds the node: when another thread erases the element after the search found it, the
iterator still reads it as it was, and the element lives for as long as the iterator does.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::sorted_map<string, int> ports = {{"http", 80}, {"https", 443}, {"ssh", 22}};

    if (auto it = ports.find("https"); it != ports.end()) {  // a literal: no string made
        println("{} {}", it->first, it->second);
    }
    println("{}", ports.find("ftp") == ports.end());

    auto ssh = ports.find("ssh");
    ports.erase("ssh");
    println("{} {}", ssh->first, ssh->second);  // the iterator holds the erased element
}
```

Output:

```text
https 443
true
ssh 22
```

## See also

- [contains](contains.md): checks whether the map holds a key
- [value_or](value_or.md): a copy of the value under a key, or a fallback
- [lower_bound](lower_bound.md): the first element from a key on
- [sgcl::concurrent::sorted_map\<Key, T, Compare\>](../sorted_map.md)
