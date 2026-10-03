[sgcl](../../README.md) › [core](../README.md) › [forward_list](README.md)

# sgcl::forward_list\<T\>::swap

```cpp
void swap(forward_list& other) noexcept;
```

Exchanges the contents of the list with those of `other`: the chains the two sentinels link change places, and no
element or node is touched. Iterators and references to the elements stay valid and name the same elements, now in
the other list; `before_begin()` stays with its list object, as the sentinel does.

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
    forward_list a = {1, 2, 3};
    forward_list b = {4, 5};
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
- [splice_after](splice_after.md): moves nodes between lists
- [sgcl::forward_list\<T\>](README.md)
