[sgcl](../../README.md) › [immutable](../README.md) › [vector](README.md)

# sgcl::immutable::vector\<T\>::set

```cpp
vector set(size_type i, const T& value) const;    // (1)
vector set(size_type i, T&& value) const;         // (2)
```

Returns the vector with the element at `i` replaced. This vector is unchanged. The branches on the path to the
element and its leaf are copied, the new element in its place, and everything else is shared: `depth()` branches
and a leaf, four objects for a million elements.

1. The new element is a copy of `value`.
2. The new element is `value`, moved.

## Parameters

| Parameter | Description |
|---|---|
| `i` | the position of the element to replace |
| `value` | the value of the new element |

## Return value

The new vector, of the same size.

## Complexity

Logarithmic in `size()`, base 32: `depth()` branches and a leaf of 32 elements copied; for the last 32 elements,
the tail alone.

## Exceptions

- `out_of_range` when `i >= size()`.
- What the copy or the move of `T` throws.

This vector is never changed, so an exception leaves it as it was; no new vector is made.

## Notes

`set` is the change the trie is made for, and has no counterpart in a `std::vector`, which writes in place: 185 ns
for a random position of a hundred thousand `int`s; over a million `long`s 131 ns, where immer's vector, the same
trie over reference counts, takes 259 to 429 ns ([Benchmarks](../benchmarks.md#against-immer)). The branches
are copied without the write barrier, each source shaded once
([tracked_ptr: shade](../../core/tracked_ptr/shade.md)).

## Example

```cpp
#include "sgcl/immutable.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    immutable::vector<int> v = {1, 2, 3};
    auto w = v.set(1, 20);
    println("{} {}", v, w);

    try {
        auto x = v.set(3, 40);
    } catch (const out_of_range& e) {
        println("out of range: {}", e.what());
    }
}
```

Output:

```text
[1, 2, 3] [1, 20, 3]
out of range: sgcl::immutable::vector::set
```

## See also

- [at](at.md), [operator[]](operator_at.md): the element at a position
- [push_back](push_back.md): the vector with one more element at the end
- [update](update.md): the vector with a function of an element in its place
- [sgcl::immutable::vector\<T\>](README.md)
