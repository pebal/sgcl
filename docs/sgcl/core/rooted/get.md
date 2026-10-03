[sgcl](../../README.md) › [core](../README.md) › [rooted](README.md)

# sgcl::rooted\<T\>::get

```cpp
T* get() const noexcept;
```

Returns the address of the value in its managed object.

## Parameters

None.

## Return value

The address of the value; null only for a `rooted` moved from.

## Complexity

Constant.

## Exceptions

None.

## Notes

The one member a `rooted` moved from may be read through: [operator\*, operator->](operator_deref.md) assert on
it in debug builds. Two `rooted`s that share a value give the same address.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    rooted<int> first(std::in_place, 7);
    rooted<int> second = first;
    println("{} {}", *first.get(), first.get() == second.get());

    rooted<int> third = std::move(second);
    println("{}", second.get() == nullptr);
}
```

Output:

```text
7 true
true
```

## See also

- [operator\*, operator->](operator_deref.md): the value
- [ptr](ptr.md): the value as a `tracked_ptr`
- [sgcl::rooted\<T\>](README.md)
