[sgcl](../../README.md) › [encoding](../README.md) › [email](README.md)

# sgcl::encoding::email::header_all

```cpp
vector<string> header_all(const string& name) const;
```

Every value of the field `name`, in their order, each as [header](header.md) gives it: Received, the fields that
come more than once.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the field's name |

## Return value

The values; empty when there is none.

## Complexity

Linear in the number of fields and the size of the values.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;


int main() {
    auto m = encoding::email::parse("Received: from a\r\nReceived: from b\r\n\r\n").value();
    for (auto& r : m.header_all("received")) {
        println("{}", r);
    }
}
```

Output:

```text
from a
from b
```

## See also

- [header](header.md)
- [add_header](add_header.md)
- [email](README.md)
