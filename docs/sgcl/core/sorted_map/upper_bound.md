[sgcl](../../README.md) › [core](../README.md) › [sorted_map](../sorted_map.md)

# sgcl::sorted_map\<Key, T, Compare\>::upper_bound

```cpp
/*(1)*/ iterator upper_bound(const key_type& key) noexcept;
/*(2)*/ const_iterator upper_bound(const key_type& key) const noexcept;
/*(3)*/ template<class K> iterator upper_bound(const K& key) noexcept(/* see below */);
/*(4)*/ template<class K> const_iterator upper_bound(const K& key) const noexcept(/* see below */);
```

Returns an iterator to the first element whose key is greater than `key`, as in `std::map`: the end of a range
that holds `key`.

- (3–4) Take part only when `Compare` declares `is_transparent`: the key is of any type the comparison takes, and
  no `Key` is built for the search.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key to compare the elements with |

## Return value

An iterator to the first element whose key is greater than `key`, or [end()](end.md) when there is none.

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

using namespace sgcl;

int main() {
    sorted_map<int, char> m = {{10, 'a'}, {20, 'b'}, {30, 'c'}};
    println("{} {}", m.upper_bound(10)->second, m.upper_bound(25)->second);
    println("{}", m.upper_bound(30) == m.end());

    sorted_map<string, int> words = {{"ant", 1}, {"bee", 2}, {"cat", 3}};
    for (auto it = words.begin(), to = words.upper_bound("b"); it != to; ++it) {
        println("{}", it->first);  // the words before "b"
    }
}
```

Output:

```text
b c
true
ant
```

## See also

- [lower_bound](lower_bound.md): the first element whose key is not less than a key
- [equal_range](equal_range.md): both, from one search
- [sgcl::sorted_map\<Key, T, Compare\>](../sorted_map.md)
