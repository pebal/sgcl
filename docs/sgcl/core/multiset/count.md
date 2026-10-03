[sgcl](../../README.md) › [core](../README.md) › [multiset](../multiset.md)

# sgcl::multiset\<Key, Hash, KeyEqual\>::count

```cpp
/*(1)*/ size_type count(const key_type& key) const noexcept;
/*(2)*/ template<class K> size_type count(const K& key) const noexcept(/* see below */);
```

Returns the number of elements with the key `key`: the length of the key's run, found by a lookup and walked to
its end.

- (2) The key is of any type the hash and the equality take, and no `Key` is built for the search. Takes part
  only when `Hash` and `KeyEqual` both declare `is_transparent`, as `std::hash` and `std::equal_to` of a
  [string](../string.md) do.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key of the elements to count |

## Return value

The number of elements with the key.

## Complexity

Constant on average, the walk of one bucket, plus the number of elements with the key.

## Exceptions

- (1) None.
- (2) None when the calls of `Hash` and `KeyEqual` with a `K` are noexcept or they are std's function objects;
  otherwise what they throw.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    multiset<string> s = {"a", "a", "b"};
    // no string built for the literals
    println("{} {} {}", s.count("a"), s.count("b"), s.count("c"));

    string text = "a b";
    println("{}", s.count(text.as_slice(0, 1)));  // nor for a slice of another string
}
```

Output:

```text
2 1 0
2
```

## See also

- [equal_range](equal_range.md): the run of the elements with a key
- [contains](contains.md): checks whether the multiset holds a key
- [sgcl::multiset\<Key, Hash, KeyEqual\>](../multiset.md)
