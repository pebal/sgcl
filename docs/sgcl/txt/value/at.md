[sgcl](../../README.md) › [txt](../README.md) › [value](README.md)

# sgcl::txt::value::at

```cpp
const value* at(size_t index) const noexcept;
```

The element at `index` of a list, or null where this is not a list or has no such element.

## Parameters

| Parameter | Description |
|---|---|
| `index` | the index, from 0 |

## Return value

The element, or null.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::value xs = txt::list{10, "twenty", 30.5};
    println("{} {} {}", *xs.at(0), *xs.at(1), xs.at(3) == nullptr);
    return 0;
}
```

Output:

```text
10 twenty true
```

## See also

- [find](find.md): a value of a mapping
- [size](size.md)
- [sgcl::txt::value](README.md)
