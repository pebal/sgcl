[sgcl](../../README.md) › [core](../README.md) › [sorted_multimap](../sorted_multimap.md)

# sgcl::sorted_multimap\<Key, T, Compare\>::find

```cpp
iterator find(const key_type& key) noexcept;                                            // (1)
const_iterator find(const key_type& key) const noexcept;                                // (2)
template<class K> iterator find(const K& key) noexcept(/* see below */);                // (3)
template<class K> const_iterator find(const K& key) const noexcept(/* see below */);    // (4)
```

Finds the first element under `key`, the one inserted first; `++` from it walks the others under the key, in the
order they were inserted. The search descends the tree reading raw pointers only: no write barrier, no
allocation.

- (3–4) Take part only when `Compare` declares `is_transparent`, as `std::less` of a [string](../string.md) does:
  a literal, a `std::string_view` or a `string_slice` (a piece of another string) looks up a `string` key
  without building one.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key of the element to find |

## Return value

An iterator to the first element under `key`, or [end()](end.md) when there is none.

## Complexity

Logarithmic in the size of the multimap.

## Exceptions

- (1–2) None.
- (3–4) What the calls of `Compare` with a `K` throw; none when they are noexcept or `Compare` is a function
  object of `std`.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    sorted_multimap<string, int> m = {{"a", 1}, {"a", 2}, {"b", 3}};
    string text = "a b";
    auto first = m.find(text.as_slice(0, 1));  // a piece of another string: nothing built
    println("{} {}", *first, m.find("c") == m.end());
}
```

Output:

```text
("a", 1) true
```

## See also

- [equal_range](equal_range.md): the range of the elements under a key
- [contains](contains.md): checks whether the multimap holds a key
- [sgcl::sorted_multimap\<Key, T, Compare\>](../sorted_multimap.md)
