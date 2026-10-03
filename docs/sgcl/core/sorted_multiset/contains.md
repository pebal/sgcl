[sgcl](../../README.md) › [core](../README.md) › [sorted_multiset](../sorted_multiset.md)

# sgcl::sorted_multiset\<Key, Compare\>::contains

```cpp
bool contains(const key_type& key) const noexcept;                                // (1)
template<class K> bool contains(const K& key) const noexcept(/* see below */);    // (2)
```

Checks whether the multiset holds an element equivalent to `key`, by a search of the tree. It is the multiset's own
and takes the place of [mixin::enumerable](../mixin/enumerable.md)'s `contains`, which would walk every element.

- (2) The key is of any type the comparison takes with a `Key`, and no `Key` is built for the search. Takes part
  only when `Compare` declares `is_transparent`, as `std::less` of a [string](../string.md) does.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key to look for |

## Return value

`true` when an element with the key is there, `false` otherwise.

## Complexity

Logarithmic in the size of the multiset.

## Exceptions

- (1) None.
- (2) What the calls of `Compare` with a `K` throw; none when they are noexcept, or when `Compare` is a function
  object of `std`.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <string_view>

using namespace sgcl;

int main() {
    sorted_multiset<string> words = {"alpha", "beta", "beta"};

    println("{}", words.contains("beta"));  // a literal: no string made
    std::string_view probe = "gamma";
    println("{}", words.contains(probe));
}
```

Output:

```text
true
false
```

## See also

- [find](find.md): the first element with a key
- [count](count.md): the number of elements with a key
- [sgcl::sorted_multiset\<Key, Compare\>](../sorted_multiset.md)
