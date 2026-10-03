[sgcl](../../README.md) › [core](../README.md) › [list](../list.md)

# sgcl::list\<T\>::swap

```cpp
void swap(list& other) noexcept;
```

Exchanges the contents of the list with those of `other`: the sentinels and the counts change places, and no
element or node is touched. Iterators and references stay valid and name the same elements, now in the other list;
the `end()` of each list goes with its sentinel.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the list to exchange the contents with |

## Return value

None.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    list a = {1, 2, 3};
    list b = {4, 5};
    int& one = a.front();

    a.swap(b);
    one = 10;
    println("{} {}", a, b);
}
```

Output:

```text
[4, 5] [10, 2, 3]
```

## See also

- [swap](swap2.md): the non-member function
- [splice](splice.md): moves nodes between lists
- [sgcl::list\<T\>](../list.md)
