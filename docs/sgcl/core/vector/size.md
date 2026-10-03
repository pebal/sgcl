[sgcl](../../README.md) › [core](../README.md) › [vector](../vector.md)

# sgcl::vector\<T\>::size

```cpp
size_type size() const noexcept;
```

Returns the number of elements, `end() - begin()`. The vector keeps the number in its own object, beside the
pointer to the buffer and the capacity: reading it reads no memory of the buffer.

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
    vector<string> names = {"Ada", "Grace"};
    names.push_back("Barbara");
    println("{} names", names.size());

    names.resize(1);
    println("{} name: {}", names.size(), names);
}
```

Output:

```text
3 names
1 name: ["Ada"]
```

## See also

- [empty](empty.md): checks whether the vector is empty
- [capacity](capacity.md): the number of elements the buffer holds
- [resize](resize.md): changes the number of elements
- [sgcl::vector\<T\>](../vector.md)
