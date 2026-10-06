[sgcl](../../README.md) › [encoding](../README.md) › [yaml](README.md)

# sgcl::encoding::yaml::push_back

```cpp
yaml push_back(const yaml& value) const noexcept;
```

A new sequence with the value added at its end; of null, a sequence of the value alone; any other node as it is.

## Parameters

| Parameter | Description |
|---|---|
| `value` | the element |

## Return value

The new sequence.

## Complexity

Linear in the size of the sequence.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::yaml list;
    for (int i : range(3)) {
        list = list.push_back(i);
    }
    print(list.to_string());
}
```

Output:

```text
- 0
- 1
- 2
```

## See also

- [set](set.md)
- [sgcl::encoding::yaml](README.md)
