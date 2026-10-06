[sgcl](../../README.md) › [txt](../README.md) › [html_document](README.md)

# sgcl::txt::html_document::quirks

```cpp
bool quirks() const noexcept;            // (1)
bool limited_quirks() const noexcept;    // (2)
```

Checks the mode the DOCTYPE set (13.2.6.4.1): (1) quirks mode — no DOCTYPE, a DOCTYPE that is not `html`, or one of
the old public identifiers — (2) limited-quirks mode (the XHTML 1.0 Transitional and Frameset identifiers). In
quirks mode a `table` does not close an open `p`.

## Parameters

None.

## Return value

`true` in that mode.

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

    println("{} {}", txt::html_document::parse("<p>").quirks(),
            txt::html_document::parse("<!DOCTYPE html>").quirks());
}
```

Output:

```text
true false
```

## See also

- [parse](parse.md)
- [sgcl::txt::html_document](README.md)
