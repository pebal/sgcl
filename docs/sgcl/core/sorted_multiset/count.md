[sgcl](../../README.md) › [core](../README.md) › [sorted_multiset](README.md)

# sgcl::sorted_multiset\<Key, Compare\>::count

```cpp
size_type count(const key_type& key) const noexcept;                                // (1)
template<class K> size_type count(const K& key) const noexcept(/* see below */);    // (2)
```

Returns the number of elements equivalent to `key`: the range of [equal_range](equal_range.md), walked.

- (2) The key is of any type the comparison takes with a `Key`, and no `Key` is built for the search. Takes part
  only when `Compare` declares `is_transparent`, as `std::less` of a [string](../string/README.md) does.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key of the elements to count |

## Return value

The number of elements with the key.

## Complexity

Logarithmic in the size of the multiset, plus the number of elements counted.

## Exceptions

- (1) None.
- (2) What the calls of `Compare` with a `K` throw; none when they are noexcept, or when `Compare` is a function
  object of `std`.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    sorted_multiset<string> votes = {"yes", "no", "yes", "yes"};
    println("{} {} {}", votes.count("yes"), votes.count("no"), votes.count("maybe"));

    string text = "yes, sure";
    println("{}", votes.count(text.as_slice(0, 3)));  // a slice of another string: nothing built
}
```

Output:

```text
3 1 0
3
```

## See also

- [contains](contains.md): checks whether a key is there
- [equal_range](equal_range.md): the elements with a key
- [sgcl::sorted_multiset\<Key, Compare\>](README.md)
