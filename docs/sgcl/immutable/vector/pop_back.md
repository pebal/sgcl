[sgcl](../../README.md) › [immutable](../README.md) › [vector](README.md)

# sgcl::immutable::vector\<T\>::pop_back

```cpp
vector pop_back() const noexcept(std::is_nothrow_copy_constructible_v<T>);
```

Returns the vector without its last element. This vector is unchanged. The tail is copied one element shorter;
when the tail held one element, the trie's last leaf is taken out along a copied path to serve as the tail, and
the trie loses a level when its root is left with one child. A vector of one element gives an empty vector, which
holds no node.

## Parameters

None.

## Return value

The new vector, `size() - 1` elements.

## Complexity

Constant in practice: a copy of the tail, at most 32 elements of `T`, and once in 32 pops a path of `depth()`
branches, logarithmic in `size()`, base 32.

## Exceptions

What the copy constructor of `T` throws; none when it is noexcept.

This vector is never changed, so an exception leaves it as it was; no new vector is made. `pop_back` on an empty
vector is undefined; debug builds assert.

## Notes

Nothing is destroyed: this vector still holds the element, and the collector destroys it with its leaf once no
version reaches the leaf.

## Example

```cpp
#include "sgcl/immutable.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    immutable::vector<int> v = {1, 2, 3};
    auto shorter = v.pop_back();
    println("{} {} {}", v, shorter, shorter.pop_back().pop_back().empty());
}
```

Output:

```text
[1, 2, 3] [1, 2] true
```

## See also

- [push_back](push_back.md): the vector with one more element at the end
- [back](back.md): the last element
- [sgcl::immutable::vector\<T\>](README.md)
