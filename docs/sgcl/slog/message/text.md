[sgcl](../../README.md) › [slog](../README.md) › [message](README.md)

# sgcl::slog::message::text

```cpp
slice<const char> text() const noexcept;
```

Returns the text of the message, a view of what it was made from.

## Parameters

None.

## Return value

A slice of the text; empty for a null `const char*`.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/slog.h"

using namespace sgcl;

int main() {
    string s = "server started";
    slog::message m(s);
    println("{} ({} bytes)", m.text(), m.text().size());
}
```

Output:

```text
server started (14 bytes)
```

## See also

- [where](where.md)
- [record::message](../record/message.md): the text as a handler reads it
- [sgcl::slog::message](README.md)
