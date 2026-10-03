[sgcl](../../README.md) › [core](../README.md) › [multimap](../multimap.md)

# sgcl::multimap\<Key, T, Hash, KeyEqual\>::find

```cpp
/*(1)*/ iterator find(const key_type& key) noexcept;
/*(2)*/ const_iterator find(const key_type& key) const noexcept;
/*(3)*/ template<class K> iterator find(const K& key) noexcept(/* see below */);
/*(4)*/ template<class K> const_iterator find(const K& key) const noexcept(/* see below */);
```

Finds the first element under `key`, the start of the key's run: the walk of the key's bucket, the cached hash
of each node compared before its key, reading raw pointers only. The first of the run is the one inserted last.

- (1–2) The key is of the key type.
- (3–4) The key is of any type the hash and the equality take. Take part only when `Hash` and `KeyEqual` both
  declare `is_transparent`, as `std::hash` and `std::equal_to` of a [string](../string.md) do: a `string_view`,
  a [string_slice](../string.md) or a literal finds a `string` key with no string made for the search.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key of the element to find |

## Return value

An iterator to the first element under the key, or [end()](end.md) when the key is not there.

## Complexity

Constant on average, linear in the size when every key falls into one bucket.

## Exceptions

- (1–2) None.
- (3–4) None when the calls of `Hash` and `KeyEqual` with a `K` are noexcept or they are the function objects of
  `std`; otherwise what they throw.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    multimap<string, int> m = {{"pear", 1}};
    m.emplace("pear", 2);
    auto it = m.find("pear");  // a literal: no string is built
    println("{} {}", it->second, std::next(it)->second);

    string line = "pear pie";
    string_slice key = line.as_slice(0, 4);  // a piece of another string
    println("{} {}", m.find(key) == it, m.find("plum") == m.end());
}
```

Output:

```text
2 1
true true
```

## See also

- [equal_range](equal_range.md): the run of the elements under a key
- [count](count.md): the number of elements under a key
- [sgcl::multimap\<Key, T, Hash, KeyEqual\>](../multimap.md)
