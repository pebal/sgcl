[sgcl](../../README.md) › [encoding](../README.md) › [content_line](README.md)

# sgcl::encoding::content_line::text

```cpp
static content_line text(const string& name, const string& text) noexcept;    // (1)
string text() const noexcept;                                                 // (2)
```

1. A line of a TEXT value: `\\`, `;`, `,` and line breaks escaped (`\n`), as SUMMARY, DESCRIPTION, NOTE are
   written.
2. The value read as TEXT: `\n` and `\N` a line break, `\\`, `\;`, `\,` the character (a backslash before
   any other character dropped).

## Parameters

| Parameter | Description |
|---|---|
| `name` | the property's name |
| `text` | the text, any characters |

## Return value

(1) The line; (2) the text.

## Complexity

Linear in the size of the value.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::content_line line = encoding::content_line::text("DESCRIPTION", "Agenda: one, two; three\nand more");
    print(line.to_string());
    println(line.text());
}
```

Output:

```text
DESCRIPTION:Agenda: one\, two\; three\nand more
Agenda: one, two; three
and more
```

## See also

- [list, components](list.md)
- [sgcl::encoding::content_line](README.md)
