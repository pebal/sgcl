[sgcl](../../README.md) › [txt](../README.md) › [message_format](README.md)

# sgcl::txt::message_format::message_format

```cpp
message_format() noexcept = default;                                // (1)
explicit message_format(const string& pattern, const locale& l);    // (2)
```

Constructs a message.

1. The empty message: it writes nothing.
2. The message a literal of the program spells, read for the locale `l` as [parse](parse.md) reads it.

## Parameters

| Parameter | Description |
|---|---|
| `pattern` | the message in the syntax of ICU's MessageFormat |
| `l` | the locale it writes for |

## Complexity

Linear in the length of the pattern.

## Exceptions

`bad_expected_access<message_error>` with the error of [parse](parse.md) when `pattern` is not a message (2).

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::message_format m("{n, plural, one {# plik} few {# pliki} other {# plików}} w {dir}",
                          txt::locale("pl"));
    println("{}", m.format(txt::object{{"n", 5}, {"dir", "src"}}));
    println("[{}]", txt::message_format().format(txt::object{}));
}
```

Output:

```text
5 plików w src
[]
```

## See also

- [parse](parse.md)
- [sgcl::txt::message_format](README.md)
