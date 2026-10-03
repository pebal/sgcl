[sgcl](../../README.md) › [core](../README.md) › [sorted_map](README.md)

# sgcl::sorted_map\<Key, T, Compare\>::find

```cpp
iterator find(const key_type& key) noexcept;                                            // (1)
const_iterator find(const key_type& key) const noexcept;                                // (2)
template<class K> iterator find(const K& key) noexcept(/* see below */);                // (3)
template<class K> const_iterator find(const K& key) const noexcept(/* see below */);    // (4)
```

Finds the element under `key`. The search descends the tree reading raw pointers only: no write barrier, no
allocation.

- (3–4) Take part only when `Compare` declares `is_transparent`, as `std::less` of a [string](../string/README.md) does:
  a literal, a `std::string_view` or a `string_slice` (a piece of another string) looks up a `string` key
  without building one.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key of the element to find |

## Return value

An iterator to the element under `key`, or [end()](end.md) when there is none.

## Complexity

Logarithmic in the size of the map.

## Exceptions

- (1–2) None.
- (3–4) What the calls of `Compare` with a `K` throw; none when they are noexcept or `Compare` is a function
  object of `std`.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <string_view>

using namespace sgcl;

int main() {
    sorted_map<string, int> m = {{"apple", 1}, {"pear", 2}};
    auto it = m.find("apple");  // no string is built for the literal
    println("{} {}", it->second, m.find("plum") == m.end());

    string line = "pear pie";
    string_slice word = line.as_slice(0, 4);  // a piece of another string
    std::string_view view = "apple";
    println("{} {}", m.find(word)->second, m.find(view)->second);
}
```

Output:

```text
1 true
2 1
```

## See also

- [contains](contains.md): checks whether the map holds a key
- [at](at.md): the value under a key, with bounds checking
- [lower_bound](lower_bound.md): the first element whose key is not less than a key
- [sgcl::sorted_map\<Key, T, Compare\>](README.md)
