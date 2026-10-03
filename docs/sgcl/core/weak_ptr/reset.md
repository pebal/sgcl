[sgcl](../../README.md) › [core](../README.md) › [weak_ptr](../weak_ptr.md)

# sgcl::weak_ptr\<T\>::reset

```cpp
void reset() noexcept;
```

Drops the cell: the `weak_ptr` is empty afterwards, expired. Copies that share the cell keep it.

## Parameters

None.

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
    tracked_ptr number = make_tracked<int>(1);
    weak_ptr weak = number;
    weak_ptr copy = weak;
    weak.reset();
    println("{} {}", weak.expired(), *copy.lock());
}
```

Output:

```text
true 1
```

## See also

- [operator=](operator_assign.md): assigns the pointer
- [sgcl::weak_ptr\<T\>](../weak_ptr.md)
