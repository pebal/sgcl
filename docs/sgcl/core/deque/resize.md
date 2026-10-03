[sgcl](../../README.md) › [core](../README.md) › [deque](../deque.md)

# sgcl::deque\<T\>::resize

```cpp
void resize(size_type count) requires std::default_initializable<T>;    // (1)
void resize(size_type count, const value_type& value);                  // (2)
```

Changes the number of elements to `count`: pops from the back down to it, or pushes elements at the back up to
it.

1. The elements pushed are value-initialized.
2. The elements pushed are copies of `value`.

## Parameters

| Parameter | Description |
|---|---|
| `count` | the number of elements |
| `value` | the value the new elements are copied from |

## Return value

None.

## Complexity

Linear in the difference between `size()` and `count`.

## Exceptions

- `length_error` when `count > max_size()`, before anything changes.
- What the default constructor (1) or the copy constructor (2) of `T` throws.

If an element's constructor throws, the elements pushed before it are popped again: the deque is as it was, as
`std::deque`'s.

## Notes

A growth invalidates the iterators and keeps the references valid; a shrink invalidates the erased elements and
`end()` ([Iterator invalidation](../deque.md#iterator-invalidation)).

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    deque d = {1, 2, 3};
    d.resize(5);
    println("{}", d);

    d.resize(2);
    println("{}", d);

    d.resize(4, 7);
    println("{}", d);
}
```

Output:

```text
[1, 2, 3, 0, 0]
[1, 2]
[1, 2, 7, 7]
```

## See also

- [size](size.md): the number of elements
- [push_back](push_back.md), [pop_back](pop_back.md): append, remove the last element
- [sgcl::deque\<T\>](../deque.md)
