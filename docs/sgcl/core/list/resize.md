[sgcl](../../README.md) › [core](../README.md) › [list](README.md)

# sgcl::list\<T\>::resize

```cpp
void resize(size_type count) noexcept(std::is_nothrow_default_constructible_v<T>);    // (1)
void resize(size_type count, const T& value)                                          // (2)
    noexcept(std::is_nothrow_copy_constructible_v<T>);
```

Changes the number of elements to `count`. A list longer than `count` erases the elements past it; a shorter one
appends new elements up to it, built as a chain of nodes and linked in at once.

1. Appends value-initialized elements.
2. Appends copies of `value`.

## Parameters

| Parameter | Description |
|---|---|
| `count` | the new number of elements |
| `value` | the value the new elements are copied from |

## Return value

None.

## Complexity

Linear in the difference between `size()` and `count`; a shrink also walks to the element at `count`, from
whichever end of the list is nearer.

## Exceptions

What the default constructor (1) or the copy constructor (2) of `T` throws; none when it is noexcept. If an
exception is thrown, the list is as it was before the call.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    list l = {1, 2, 3};
    l.resize(5);
    println("{}", l);

    l.resize(2);
    println("{}", l);

    l.resize(4, 7);
    println("{}", l);
}
```

Output:

```text
[1, 2, 3, 0, 0]
[1, 2]
[1, 2, 7, 7]
```

## See also

- [assign](assign.md): replaces the contents
- [erase](erase.md): erases elements at a position or in a range
- [sgcl::list\<T\>](README.md)
