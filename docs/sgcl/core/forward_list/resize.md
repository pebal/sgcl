[sgcl](../../README.md) › [core](../README.md) › [forward_list](../forward_list.md)

# sgcl::forward_list\<T\>::resize

```cpp
/*(1)*/ void resize(size_type count) noexcept(std::is_nothrow_default_constructible_v<T>)
            requires std::default_initializable<T>;
/*(2)*/ void resize(size_type count, const value_type& value)
            noexcept(std::is_nothrow_copy_constructible_v<T>);
```

Changes the number of elements to `count`, in one walk of the list. A list longer than `count` erases the elements
past it; a shorter one appends new elements up to it, built as a chain of nodes and linked in at once.

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

Linear in the larger of `count` and the number of elements: the list is walked to the element at `count`.

## Exceptions

What the default constructor (1) or the copy constructor (2) of `T` throws; none when it is noexcept. If an
exception is thrown, the list is as it was before the call.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    forward_list l = {1, 2, 3};
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
- [erase_after](erase_after.md): erases elements after a position
- [sgcl::forward_list\<T\>](../forward_list.md)
