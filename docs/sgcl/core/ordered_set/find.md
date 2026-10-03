[sgcl](../../README.md) › [core](../README.md) › [ordered_set](../ordered_set.md)

# sgcl::ordered_set\<Key, Hash, KeyEqual\>::find

```cpp
iterator find(const key_type& key) noexcept;                                            // (1)
const_iterator find(const key_type& key) const noexcept;                                // (2)
template<class K> iterator find(const K& key) noexcept(/* see below */);                // (3)
template<class K> const_iterator find(const K& key) const noexcept(/* see below */);    // (4)
```

Finds the element equal to `key`: a walk of the key's bucket, reading raw pointers only, the cached hash of each
node compared before the element. The order plays no part in it.

- (1–2) The key is of the key type.
- (3–4) The key is of any type the hash and the equality take. Take part only when `Hash` and `KeyEqual` both
  declare `is_transparent`, as `std::hash` and `std::equal_to` of a [string](../string.md) do: a `string_view`,
  a literal or a [string_slice](../string.md) finds a `string` element with no string made for the search.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the element to find |

## Return value

An iterator to the element, or [end()](end.md) when it is not there.

## Complexity

Constant on average, linear in `size()` in the worst case.

## Exceptions

- (1–2) None.
- (3–4) None when the calls of `Hash` and `KeyEqual` with a `K` are noexcept or they are the function objects of
  `std`; otherwise what those calls throw.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <string_view>

using namespace sgcl;

int main() {
    ordered_set<string> s = {"apple", "pear"};

    auto it = s.find("pear");  // a literal: no string made
    println("{} {}", *it, std::next(it) == s.end());

    string line = "apple pie";
    string_slice word = line.as_slice(0, 5);
    println("{}", s.find(word) == s.begin());

    std::string_view name = "plum";
    println("{}", s.find(name) == s.end());
}
```

Output:

```text
pear true
true
true
```

## See also

- [contains](contains.md): checks whether an element is there
- [sgcl::ordered_set\<Key, Hash, KeyEqual\>](../ordered_set.md)
