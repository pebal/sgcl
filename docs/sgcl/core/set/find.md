[sgcl](../../README.md) › [core](../README.md) › [set](README.md)

# sgcl::set\<Key, Hash, KeyEqual\>::find

```cpp
iterator find(const key_type& key) noexcept;                                            // (1)
const_iterator find(const key_type& key) const noexcept;                                // (2)
template<class K> iterator find(const K& key) noexcept(/* see below */);                // (3)
template<class K> const_iterator find(const K& key) const noexcept(/* see below */);    // (4)
```

Finds the element with the key `key`: the walk of the key's bucket, the cached hash of each node compared before
its key. The walk reads raw pointers only and pays no write barrier.

- (3–4) The key is of any type the hash and the equality take, and no `Key` is built for the search. Take part
  only when `Hash` and `KeyEqual` both declare `is_transparent`, as `std::hash` and `std::equal_to` of a
  [string](../string/README.md) do: a literal, a `std::string_view` or a slice of another string finds a `string` key.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key of the element to find |

## Return value

An iterator to the element, or [end()](end.md) when the key is not there.

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
    set<string> s = {"apple", "pear"};

    auto it = s.find("pear");  // a literal: no string made
    println("{}", *it);

    std::string_view name = "fig";
    println("{}", s.find(name) == s.end());

    string line = "apple pie";
    println("{}", *s.find(line.as_slice(0, 5)));  // a slice of another string
}
```

Output:

```text
pear
true
apple
```

## See also

- [contains](contains.md): checks whether the set holds a key
- [equal_range](equal_range.md): the range of the elements with a key
- [sgcl::set\<Key, Hash, KeyEqual\>](README.md)
