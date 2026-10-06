[sgcl](../../README.md) › [encoding](../README.md) › [ini](README.md)

# sgcl::encoding::ini::to_string

```cpp
string to_string() const;
```

The sections as an INI text: the section `""` first, without a header (and not at all when it has no entry), then
each section as `[name]` and its entries as `key = value` lines, a blank line between sections; a value of several
lines on lines indented by four spaces after its key.

## Parameters

None.

## Return value

The text.

## Complexity

Linear in the size of the sections.

## Exceptions

`invalid_argument` for what would read back otherwise: a name with a line
break; a key that is empty, has white space at its ends, holds `=`, `:` or a line break, starts with `;` or `#`, or starts
with `[` on a line holding a `]`; a value with white space at the ends of a line, an empty line after its first, or a
line after its first starting with `;` or `#`; a null character; a key of a byte order mark first at the start of the
text (the reading passes over one there).

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::ini config = encoding::ini().set("server", "host", "example.com").set("", "name", "app")
                                          .set("server", "motd", "line one\nline two").set("empty", "k", "");
    print(config.to_string());
}
```

Output:

```text
name = app

[server]
host = example.com
motd = line one
    line two

[empty]
k =
```

## See also

- [parse](parse.md)
- [save](save.md)
- [sgcl::encoding::ini](README.md)
