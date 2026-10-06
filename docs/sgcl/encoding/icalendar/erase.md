[sgcl](../../README.md) › [encoding](../README.md) › [icalendar](README.md)

# sgcl::encoding::icalendar::erase

```cpp
icalendar erase(const string& name) const noexcept;
```

A new component without the properties of the name, in any case; the component as it is when there are none.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the properties' name |

## Return value

The new component.

## Complexity

Linear in the properties.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    println(encoding::icalendar().erase("prodid").properties().size());
}
```

Output:

```text
1
```

## See also

- [set](set.md)
- [sgcl::encoding::icalendar](README.md)
