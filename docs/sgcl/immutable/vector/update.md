[sgcl](../../README.md) › [immutable](../README.md) › [vector](../vector.md)

# sgcl::immutable::vector\<T\>::update

```cpp
template<class F>
vector update(size_type i, F f) const;
```

Returns the vector with `f(element)` in place of the element at `i`: [set](set.md) of what `f` gives of the old
element. This vector is unchanged. `f` is called once, with the element as a `const T&`, and what it returns is made
into a `T` before the path is copied, so `f` may read this vector. Takes part only when `f` called with a `const T&`
gives something a `T` is made of.

## Parameters

| Parameter | Description |
|---|---|
| `i` | the position of the element to replace |
| `f` | what makes the new element of the old one |

## Return value

The new vector, of the same size.

## Complexity

A call of `f`; then as [set](set.md), logarithmic in `size()`, base 32.

## Exceptions

- `out_of_range` when `i >= size()`, before `f` is called.
- What `f` throws, and what the construction of `T` of its result and the move of `T` throw.

This vector is never changed, so an exception leaves it as it was; no new vector is made.

## Example

```cpp
#include "sgcl/immutable.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    immutable::vector<int> scores = {10, 20, 30};
    auto bonus = scores.update(1, [](int s) { return s + 5; });
    println("{} {}", scores[1], bonus[1]);
    try {
        (void)scores.update(3, [](int s) { return s + 5; });
    } catch (const out_of_range& e) {
        println("{}", e.what());
    }
}
```

Output:

```text
20 25
sgcl::immutable::vector::update
```

## See also

- [set](set.md): the vector with an element replaced
- [at](at.md): the element at a position
- [sgcl::immutable::vector\<T\>](../vector.md)
