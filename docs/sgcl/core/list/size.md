[sgcl](../../README.md) › [core](../README.md) › [list](README.md)

# sgcl::list\<T\>::size

```cpp
size_type size() const noexcept;
```

Returns the number of elements. The count is a word of the list object, kept through every insertion, erasure and
splice, so nothing is walked.

## Parameters

None.

## Return value

The number of elements.

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
    a.splice(a.end(), b, b.begin(), b.end());  // the counts follow the nodes
    println("{} {}", a.size(), b.size());
}
```

Output:

```text
5 0
```

## See also

- [empty](empty.md): checks whether the list is empty
- [max_size](max_size.md): the largest number of elements
- [sgcl::list\<T\>](README.md)
