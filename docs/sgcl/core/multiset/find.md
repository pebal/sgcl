[sgcl](../../README.md) › [core](../README.md) › [multiset](README.md)

# sgcl::multiset\<Key, Hash, KeyEqual\>::find

```cpp
iterator find(const key_type& key) noexcept;                                            // (1)
const_iterator find(const key_type& key) const noexcept;                                // (2)
template<class K> iterator find(const K& key) noexcept(/* see below */);                // (3)
template<class K> const_iterator find(const K& key) const noexcept(/* see below */);    // (4)
```

Finds the first element with the key `key`, the first of its run: the walk of the key's bucket, the cached hash
of each node compared before its key. The walk reads raw pointers only and pays no write barrier.

- (3–4) The key is of any type the hash and the equality take, and no `Key` is built for the search. Take part
  only when `Hash` and `KeyEqual` both declare `is_transparent`, as `std::hash` and `std::equal_to` of a
  [string](../string/README.md) do: a literal, a `std::string_view` or a slice of another string finds a `string` key.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key of the element to find |

## Return value

An iterator to the first element with the key, or [end()](end.md) when the key is not there. The other
elements with the key follow it.

## Complexity

Constant on average, the walk of one bucket; linear in the size when every key falls into one bucket.

## Exceptions

- (1–2) None.
- (3–4) None when the calls of `Hash` and `KeyEqual` with a `K` are noexcept or they are std's function objects;
  otherwise what they throw.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <string_view>

using namespace sgcl;

int main() {
    multiset<string> s = {"pear", "apple", "pear"};

    auto it = s.find("pear");  // a literal: no string made
    println("{} {}", *it, *std::next(it));

    std::string_view name = "fig";
    println("{}", s.find(name) == s.end());
}
```

Output:

```text
pear pear
true
```

## See also

- [equal_range](equal_range.md): the run of the elements with a key
- [contains](contains.md): checks whether the multiset holds a key
- [sgcl::multiset\<Key, Hash, KeyEqual\>](README.md)
