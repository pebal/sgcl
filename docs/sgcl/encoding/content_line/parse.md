[sgcl](../../README.md) › [encoding](../README.md) › [content_line](README.md)

# sgcl::encoding::content_line::parse

```cpp
static expected<content_line, error> parse(const string& line) noexcept;
```

One line, unfolded: `[group.]NAME[;PARAM=value,...]:value` by the [rules](README.md#rules).

## Parameters

| Parameter | Description |
|---|---|
| `line` | the line, without its CRLF |

## Return value

The line, or the [error](../error/README.md) with its place: `syntax` for a line that does not start with a name,
a parameter without `=` or its name, a `"` inside a value of a parameter, no `:`; `unexpected_end` for a quote not
closed; `invalid_character` for a control character; `invalid_utf8`.

## Complexity

Linear in the size of the line.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto line = encoding::content_line::parse("item1.EMAIL;TYPE=work,pref:jan@example.com").value();
    println("{} {} {}", line.group(), line.name(), line.params()[0].values.size());
    for (const char* bad : {":x", "NAME", "NAME;P=\"open:x"}) {
        println(encoding::content_line::parse(bad).error().message());
    }
}
```

Output:

```text
item1 EMAIL 2
1:1: a line that does not start with a name
1:5: no ':' before the value
1:8: a quoted parameter value without its closing quote
```

## See also

- [to_string](to_string.md)
- [sgcl::encoding::content_line](README.md)
