[sgcl](../../README.md) › [core](../README.md) › [unique_ptr](../unique_ptr.md)

# sgcl::unique_ptr\<T\>::swap

```cpp
void swap(std::unique_ptr<T, deleter_type>& other) noexcept;
```

Exchanges the objects of two owners: the `swap` of `std::unique_ptr`, which takes a `unique_ptr<T>` as its base.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the owner to exchange with |

## Return value

None.

## Complexity

Constant.

## Exceptions

None.

## Notes

`swap(a, b)`, found in `std` through the base, does the same.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    unique_ptr a = make_tracked<int>(1);
    unique_ptr b = make_tracked<int>(2);
    a.swap(b);
    println("{} {}", *a, *b);

    swap(a, b);
    println("{} {}", *a, *b);
}
```

Output:

```text
2 1
1 2
```

## See also

- [reset](reset.md): destroys the object, or replaces it
- [sgcl::unique_ptr\<T\>](../unique_ptr.md)
