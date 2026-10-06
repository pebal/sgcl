[sgcl](../../README.md) › [txt](../README.md)

# sgcl::txt::message_error

```cpp
#include "sgcl/txt/message_format.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    class message_error;
}
```

`sgcl::txt::message_error` is where a message stopped being one: the byte and why.

## Member functions

| Function | Description |
|---|---|
| [offset](offset.md) | the byte the reading stopped on |
| [message](message.md) | why |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    auto r = txt::format_message("{n, number, ::bogus}", txt::locale("en"), txt::object{{"n", 1}});
    println("at {}: {}", r.error().offset(), r.error().message());
}
```

Output:

```text
at 4: a number skeleton this formatter does not read
```

## See also

- [message_format](../message_format/README.md)
- [format_message](../format_message.md)
- [sgcl::txt](../README.md)
