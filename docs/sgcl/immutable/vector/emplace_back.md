[sgcl](../../README.md) › [immutable](../README.md) › [vector](README.md)

# sgcl::immutable::vector\<T\>::emplace_back

```cpp
template<class... A>
vector emplace_back(A&&... a) const
    noexcept(std::is_nothrow_copy_constructible_v<T> && std::is_nothrow_constructible_v<T, A...>);
```

Returns the vector with one more element at the end, constructed from `a...` in its place in the new tail. This
vector is unchanged. The rest is as [push_back](push_back.md): the tail copied, a full tail hung on the trie
first.

## Parameters

| Parameter | Description |
|---|---|
| `a` | the arguments the element is constructed from |

## Return value

The new vector, `size() + 1` elements.

## Complexity

Constant in practice: a copy of the tail, at most 32 elements of `T`, and once in 32 pushes a path of `depth()`
branches, logarithmic in `size()`, base 32.

## Exceptions

What the constructor of `T` from `a...` and the copy constructor of `T` throw; none when they are noexcept.

This vector is never changed, so an exception leaves it as it was; no new vector is made.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/immutable.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    immutable::vector<pair<string, int>> scores;
    auto one = scores.emplace_back("Ada", 3);
    auto two = one.emplace_back("Grace", 5);
    println("{} {} {}", scores.size(), one.back().first, two.back().second);
}
```

Output:

```text
0 Ada 5
```

## See also

- [push_back](push_back.md): the vector with a copy of a value at the end
- [sgcl::immutable::vector\<T\>](README.md)
