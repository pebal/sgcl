[sgcl](../../README.md) › [encoding](../README.md) › [email](README.md)

# sgcl::encoding::email::remove_header

```cpp
email& remove_header(const string& name);
```

Every field of the name taken out; a name with no field changes nothing.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the field's name |

## Return value

`*this`.

## Complexity

Linear in the number of fields.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;


int main() {
    encoding::email m("a@example.com", "b@example.com", "s", "t");
    m.remove_header("Message-ID");
    println("{}", m.has_header("message-id"));
}
```

Output:

```text
false
```

## See also

- [set_header](set_header.md)
- [email](README.md)
