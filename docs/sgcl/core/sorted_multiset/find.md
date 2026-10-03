[sgcl](../../README.md) › [core](../README.md) › [sorted_multiset](README.md)

# sgcl::sorted_multiset\<Key, Compare\>::find

```cpp
iterator find(const key_type& key) noexcept;                                            // (1)
const_iterator find(const key_type& key) const noexcept;                                // (2)
template<class K> iterator find(const K& key) noexcept(/* see below */);                // (3)
template<class K> const_iterator find(const K& key) const noexcept(/* see below */);    // (4)
```

Finds an element equivalent to `key`, neither less nor greater than it by `Compare`: the first of them in the
order, the one inserted first. The search walks down the tree from its root, reading raw pointers only.

- (1–2) The key is of the key type.
- (3–4) The key is of any type the comparison takes with a `Key`, and no `Key` is built for the search. Take part
  only when `Compare` declares `is_transparent`, as `std::less` of a [string](../string/README.md) does: a
  `string_view`, a literal or a slice of another string finds a `string` key.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key of the element to find |

## Return value

An iterator to the first element with the key, or [end()](end.md) when there is none.

## Complexity

Logarithmic in the size of the multiset.

## Exceptions

- (1–2) None.
- (3–4) What the calls of `Compare` with a `K` throw; none when they are noexcept, or when `Compare` is a function
  object of `std`.

## Notes

`std::multiset::find` may return any of the equivalent elements; this one returns the first, the one
[lower_bound](lower_bound.md) returns when the key is there.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <iterator>

using namespace sgcl;

int main() {
    sorted_multiset<string> fruit = {"apple", "fig", "fig", "pear"};

    auto it = fruit.find("fig");  // a literal: no string made
    println("{} {}", *it, std::distance(fruit.begin(), it));

    string line = "pear tree";
    println("{}", fruit.find(line.as_slice(0, 4)) != fruit.end());  // a slice of another string
    println("{}", fruit.find("plum") == fruit.end());
}
```

Output:

```text
fig 1
true
true
```

## See also

- [contains](contains.md): checks whether a key is there
- [equal_range](equal_range.md): every element with a key
- [sgcl::sorted_multiset\<Key, Compare\>](README.md)
