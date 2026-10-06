[sgcl](../../README.md) › [txt](../README.md) › [message_format](README.md)

# sgcl::txt::message_format::where

```cpp
locale where() const noexcept;
```

Returns the locale the message writes for.

## Parameters

None.

## Return value

The locale; the root locale for the empty message.

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
    println("{}", m.where().to_string());
}
```

Output:

```text
pl
```

## See also

- [pattern](pattern.md)
- [sgcl::txt::message_format](README.md)
