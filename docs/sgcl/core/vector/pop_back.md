[sgcl](../../README.md) › [core](../README.md) › [vector](README.md)

# sgcl::vector\<T\>::pop_back

```cpp
void pop_back() noexcept;
```

Removes the last element: the size goes down by one and the element is destroyed at once. The vector must not be
empty. The capacity stays.

## Parameters

None.

## Return value

None.

## Complexity

Constant.

## Exceptions

None.

## Notes

`pop_back` on an empty vector is undefined behaviour, as with `std::vector`. A reference, a pointer or an
iterator to the removed element, and `end()`, are invalid after the call.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector<string> path = {"home", "user", "docs"};
    path.pop_back();
    println("{}, capacity kept: {}", path, path.capacity() >= 3);
}
```

Output:

```text
["home", "user"], capacity kept: true
```

## See also

- [push_back](push_back.md): appends an element
- [back](back.md): access the last element
- [sgcl::vector\<T\>](README.md)
