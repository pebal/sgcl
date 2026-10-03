[sgcl](../../README.md) › [concurrent](../README.md) › [set](../set.md)

# sgcl::concurrent::set\<Key, Hash, KeyEqual\>::find

```cpp
iterator find(const Key& key) noexcept;                                // (1)
const_iterator find(const Key& key) const noexcept;                    // (2)
template<class K> iterator find(const K& key) noexcept;                // (3)
template<class K> const_iterator find(const K& key) const noexcept;    // (4)
```

Finds the element equal to `key`: from the dummy node of the key's bucket along the list while the split keys are
less than the key's, then through the elements of the same split key for the key itself, stepping over erased
nodes without touching them.

- (1–2) The key is of the key type.
- (3–4) The key is of any type the hash and the equality take. Take part only when `Hash` and `KeyEqual` both
  declare `is_transparent`, as `std::hash` and `std::equal_to` of a [string](../../core/string.md) do: a
  `string_view` or a literal finds a `string` key with no string made for the search.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key to find |

## Return value

An iterator to the element, or [end()](end.md) when the key is not there.

## Complexity

Constant on average: the walk over the elements of one bucket, one on average at a load factor of one; linear in
the size when every key falls into one bucket.

## Exceptions

None.

## Notes

Wait-free once the key's bucket has its dummy node, and then it writes nothing. A bucket gets its dummy on its
first use after the array grew: from the first insertion into it, or from the first lookup, which makes it as an
insertion does (an allocation and a compare-exchange, lock-free, once per bucket for the life of the array). A
lookup that walked from the nearest initialized ancestor's dummy instead, writing nothing, took 7000 ns where 6
were due, with the array doubled late in a run of insertions ([map::find](../map/find.md) has the account).

The iterator holds the element's node: an element another thread erases after the search found it stays alive and
is read as it was.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <string_view>

using namespace sgcl;

int main() {
    concurrent::set<string> hosts = {"example.com", "example.org"};

    auto it = hosts.find("example.org");  // a literal: no string made
    println("{}", *it);

    std::string_view name = "example.net";
    println("{}", hosts.find(name) == hosts.end());

    hosts.erase("example.org");
    println("{} {}", *it, hosts.find("example.org") == hosts.end());
}
```

Output:

```text
example.org
true
example.org true
```

## See also

- [contains](contains.md): checks whether a key is there
- [count](count.md): the same answer as a number
- [sgcl::concurrent::set\<Key, Hash, KeyEqual\>](../set.md)
