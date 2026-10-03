[sgcl](../../README.md) › [immutable](../README.md) › [vector](README.md)

# sgcl::immutable::vector\<T\>::push_back

```cpp
vector push_back(const T& value) const noexcept(std::is_nothrow_copy_constructible_v<T>);    // (1)
vector push_back(T&& value) const                                                            // (2)
    noexcept(std::is_nothrow_copy_constructible_v<T> &&
             std::is_nothrow_move_constructible_v<T>);
```

Returns the vector with one more element at the end. This vector is unchanged.

1. The new element is a copy of `value`.
2. The new element is `value`, moved.

The tail is copied with the element appended. When the tail was full, it first goes into the trie as it is, as
the trie's next leaf, along a copied path of `depth()` branches (a new level on top when the trie is full at its
height), and the new tail holds the element alone. Everything else is shared with this vector.

## Parameters

| Parameter | Description |
|---|---|
| `value` | the value of the element to append |

## Return value

The new vector, `size() + 1` elements.

## Complexity

Constant in practice: a copy of the tail, at most 32 elements of `T`, and once in 32 pushes a path of `depth()`
branches, logarithmic in `size()`, base 32.

## Exceptions

What the copy constructor of `T`, and (2) its move constructor, throw; none when they are noexcept.

This vector is never changed, so an exception leaves it as it was; no new vector is made.

## Notes

21 ns per push of an `int`, a hundred thousand times over, each version let go of
([Benchmarks](../benchmarks.md#against-immer)). A tail that is not full is a leaf of a type of its own, on pages of
its own, and the tail a push fills is made a leaf of the trie's: the tails the pushes copy and drop do not hold
the pages of the leaves that stay ([Benchmarks: Memory of a version](../benchmarks.md#memory-of-a-version)).

## Example

```cpp
#include "sgcl/immutable.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    immutable::vector<string> names = {"Ada"};
    string grace = "Grace";
    auto two = names.push_back(grace);
    auto three = two.push_back("Linus");
    println("{} {} {}", names, two, three);
}
```

Output:

```text
["Ada"] ["Ada", "Grace"] ["Ada", "Grace", "Linus"]
```

## See also

- [emplace_back](emplace_back.md): the element constructed from arguments
- [pop_back](pop_back.md): the vector without its last element
- [sgcl::immutable::vector\<T\>](README.md)
