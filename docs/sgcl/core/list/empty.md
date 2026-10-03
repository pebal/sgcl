[sgcl](../../README.md) › [core](../README.md) › [list](README.md)

# sgcl::list\<T\>::empty

```cpp
bool empty() const noexcept;
```

Checks whether the list has no element, `size() == 0`.

## Parameters

None.

## Return value

`true` when the list is empty, `false` otherwise.

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
    list<int> l;
    println("{}", l.empty());

    l.push_back(1);
    println("{}", l.empty());

    l.clear();
    println("{}", l.empty());
}
```

Output:

```text
true
false
true
```

## See also

- [size](size.md): the number of elements
- [clear](clear.md): destroys every element
- [sgcl::list\<T\>](README.md)
