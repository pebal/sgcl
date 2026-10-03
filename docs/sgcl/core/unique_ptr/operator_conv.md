[sgcl](../../README.md) › [core](../README.md) › [unique_ptr](../unique_ptr.md)

# sgcl::unique_ptr\<T\>::operator unique_ptr\<void\>&

```cpp
/*(1)*/ operator unique_ptr<void>&() noexcept;
/*(2)*/ operator const unique_ptr<void>&() const noexcept;
```

Every `unique_ptr<T>` is a `unique_ptr<void>&`: the same owner seen without its type. [type](type.md),
[is](is.md) and [as](as.md) still work on it, since the type is the object's, not the owner's.

## Parameters

None.

## Return value

This owner, as a reference to a `unique_ptr<void>`.

## Complexity

Constant.

## Exceptions

None.

## Notes

The containers keep owners of any type this way: one type for every `T`.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    unique_ptr number = make_tracked<int>(1);
    unique_ptr<void>& any = number;
    println("{} {}", any.is<int>(), any.is<double>());

    unique_ptr back = any.as<int>();  // the object moves into back
    println("{} {}", *back, bool(number));
}
```

Output:

```text
true false
1 false
```

## See also

- [as](as.md): moves the object into an owner of a given type
- [sgcl::unique_ptr\<T\>](../unique_ptr.md)
