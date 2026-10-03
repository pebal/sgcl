[sgcl](../../README.md) › [core](../README.md) › [sorted_multimap](README.md)

# sgcl::sorted_multimap\<Key, T, Compare\>::upper_bound

```cpp
iterator upper_bound(const key_type& key) noexcept;                                            // (1)
const_iterator upper_bound(const key_type& key) const noexcept;                                // (2)
template<class K> iterator upper_bound(const K& key) noexcept(/* see below */);                // (3)
template<class K> const_iterator upper_bound(const K& key) const noexcept(/* see below */);    // (4)
```

Returns an iterator to the first element whose key is greater than `key`, as in `std::multimap`: the end of the
run under `key`.

- (3–4) Take part only when `Compare` declares `is_transparent`: the key is of any type the comparison takes, and
  no `Key` is built for the search.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key to compare the elements with |

## Return value

An iterator to the first element whose key is greater than `key`, or [end()](end.md) when there is none.

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
    sorted_multimap<int, char> m = {{10, 'a'}, {20, 'b'}, {20, 'c'}, {30, 'd'}};
    for (auto it = m.begin(), to = m.upper_bound(20); it != to; ++it) {
        println("{} {}", it->first, it->second);  // the keys up to 20, both 20s
    }
    println("{}", m.upper_bound(30) == m.end());
}
```

Output:

```text
10 a
20 b
20 c
true
```

## See also

- [lower_bound](lower_bound.md): the first element whose key is not less than a key
- [equal_range](equal_range.md): both, from one search
- [sgcl::sorted_multimap\<Key, T, Compare\>](README.md)
