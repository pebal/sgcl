[sgcl](../../README.md) › [core](../README.md) › [forward_list](../forward_list.md)

# sgcl::forward_list\<T\>::assign

```cpp
/*(1)*/ void assign(size_type count, const T& value);
/*(2)*/ template<std::input_iterator InputIt> void assign(InputIt first, InputIt last);
/*(3)*/ void assign(std::initializer_list<T> ilist);
```

Replaces the contents of the list.

1. With `count` copies of `value`.
2. With the elements of the range `[first, last)`.
3. With the elements of `ilist`.

The elements the list has are assigned over in their nodes, from the first on; the surplus is erased, and the
missing ones are appended in nodes of their own. A reference to an element that stays names its new value.

## Parameters

| Parameter | Description |
|---|---|
| `count` | the number of elements |
| `value` | the value the elements are copied from |
| `first`, `last` | the range the elements are copied from |
| `ilist` | the list the elements are copied from |

## Return value

None.

## Complexity

Linear in the number of elements of the list and in the number of new elements.

## Exceptions

What the copy assignment and the copy constructor of `T` throw.

If an exception is thrown, the list stays valid: the elements assigned over before it keep their new values, and
none of the missing ones is appended.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    forward_list l = {1, 2, 3};
    int& first = l.front();

    l.assign(2, 9);  // in the first two nodes
    println("{}, first {}", l, first);

    l.assign({7, 8, 9, 10});
    println("{}, first {}", l, first);
}
```

Output:

```text
[9, 9], first 9
[7, 8, 9, 10], first 7
```

## See also

- [operator=](operator_assign.md): assigns another list or a list of values
- [resize](resize.md): changes the number of elements
- [sgcl::forward_list\<T\>](../forward_list.md)
