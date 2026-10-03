[sgcl](../../README.md) › [core](../README.md) › [dynamic_array](README.md)

# sgcl::dynamic_array\<T\>::back

```cpp
reference back() noexcept;                // (1)
const_reference back() const noexcept;    // (2)
```

Returns a reference to the last element, `(*this)[size() - 1]`. The array must not be empty.

## Parameters

None.

## Return value

A reference to the last element.

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
    dynamic_array<string> stages = {"parse", "check", "emit"};
    println("{}", stages.back());

    stages.back() = "link";
    println("{}", stages);
}
```

Output:

```text
emit
["parse", "check", "link"]
```

## See also

- [front](front.md): the first element
- [operator[]](operator_at.md): the element at a position
- [sgcl::dynamic_array\<T\>](README.md)
