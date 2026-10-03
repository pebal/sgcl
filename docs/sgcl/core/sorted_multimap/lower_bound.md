[sgcl](../../README.md) › [core](../README.md) › [sorted_multimap](../sorted_multimap.md)

# sgcl::sorted_multimap\<Key, T, Compare\>::lower_bound

```cpp
/*(1)*/ iterator lower_bound(const key_type& key) noexcept;
/*(2)*/ const_iterator lower_bound(const key_type& key) const noexcept;
/*(3)*/ template<class K> iterator lower_bound(const K& key) noexcept(/* see below */);
/*(4)*/ template<class K> const_iterator lower_bound(const K& key) const noexcept(/* see below */);
```

Returns an iterator to the first element whose key is not less than `key`, as in `std::multimap`: the first of the
run under `key` when there is one.

- (3–4) Take part only when `Compare` declares `is_transparent`: the key is of any type the comparison takes, and
  no `Key` is built for the search.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key to compare the elements with |

## Return value

An iterator to the first element whose key is not less than `key`, or [end()](end.md) when there is none.

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
    println("{} {}", m.lower_bound(20)->second, m.lower_bound(15)->second);
    println("{}", m.lower_bound(31) == m.end());
}
```

Output:

```text
b b
true
```

## See also

- [upper_bound](upper_bound.md): the first element whose key is greater than a key
- [equal_range](equal_range.md): both, from one search
- [sgcl::sorted_multimap\<Key, T, Compare\>](../sorted_multimap.md)
