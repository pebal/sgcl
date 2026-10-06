[sgcl](../../README.md) › [encoding](../README.md) › [toml](README.md)

# sgcl::encoding::toml::push_back

```cpp
toml push_back(const toml& value) const noexcept;
```

A new array with the value added at its end; any other value as it is.

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
    encoding::toml list = encoding::toml::array({});
    for (int i : range(3)) {
        list = list.push_back(i);
    }
    print(encoding::toml::table({{"list", list}}).to_string());
}
```

Output:

```text
list = [0, 1, 2]
```

## See also

- [set](set.md)
- [sgcl::encoding::toml](README.md)
