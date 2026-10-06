[sgcl](../../README.md) › [txt](../README.md) › [message_format](README.md)

# sgcl::txt::message_format::pattern

```cpp
string pattern() const noexcept;
```

Returns the text the message was read from.

## Parameters

None.

## Return value

The pattern; an empty text for the empty message.

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
    txt::message_format m("{n, plural, one {# plik} few {# pliki} other {# plików}} w {dir}",
                          txt::locale("pl"));
    println("{}", m.pattern());
}
```

Output:

```text
{n, plural, one {# plik} few {# pliki} other {# plików}} w {dir}
```

## See also

- [where](where.md)
- [sgcl::txt::message_format](README.md)
