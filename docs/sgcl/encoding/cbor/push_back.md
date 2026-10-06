[sgcl](../../README.md) › [encoding](../README.md) › [cbor](README.md)

# sgcl::encoding::cbor::push_back

```cpp
cbor push_back(const cbor& value) const noexcept;
```

A new array with the value added at its end; of null, an array of the value alone; any other value as it is.

## Parameters

| Parameter | Description |
|---|---|
| `value` | the element |

## Return value

The new array.

## Complexity

Linear in the size of the array.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::cbor list;
    for (int i : range(3)) {
        list = list.push_back(i);
    }
    println(list.to_string());
}
```

Output:

```text
[0, 1, 2]
```

## See also

- [set](set.md)
- [sgcl::encoding::cbor](README.md)
