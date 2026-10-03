[sgcl](../../README.md) › [core](../README.md) › [vector](README.md)

# sgcl::vector\<T\>::empty

```cpp
bool empty() const noexcept;
```

Checks whether the vector has no elements, `size() == 0`. The capacity does not count: a vector emptied by
[clear](clear.md) is empty and keeps its buffer.

## Parameters

None.

## Return value

`true` when the vector has no elements, `false` otherwise.

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
    vector<int> v;
    println("{}", v.empty());
    v.push_back(1);
    println("{}", v.empty());
    v.clear();
    println("{}, capacity {}", v.empty(), v.capacity());
}
```

Output:

```text
true
false
true, capacity 4
```

## See also

- [size](size.md): the number of elements
- [clear](clear.md): destroys every element, keeps the buffer
- [sgcl::vector\<T\>](README.md)
