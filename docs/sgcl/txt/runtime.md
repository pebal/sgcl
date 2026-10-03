[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::runtime

```cpp
#include "sgcl/txt/format.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    runtime_pattern runtime(const string& text) noexcept;
}
```

A pattern the compiler never saw, asked for by name: `txt::format(txt::runtime(entry), n)`. A text handed to
[format](format.md) as it is would be taken for a pattern to be read where the program is compiled, which a text
that is not a constant cannot be; wrapped by `runtime`, it is read where the program runs, and the forms that take
it answer an `optional` — `nullopt` when the pattern does not fit the values.

The result is a [runtime_pattern](runtime_pattern.md), which keeps the string rather than pointing into it.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the pattern |

## Return value

The pattern, `runtime_pattern(text)`.

## Complexity

Constant: the string is shared, not copied.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    string pl = "{1} {0}";  // the order of a translation
    println("{}", txt::format(txt::runtime(pl), "Ada", "Lovelace").value_or("?"));
    println("{}", txt::format(txt::runtime("{} {} {}"), 1, 2).value_or("?"));
    return 0;
}
```

Output:

```text
Lovelace Ada
?
```

## See also

- [format](format.md), [format_to](format_to.md): the forms that take it
- [fits](fits.md): whether it fits, asked where it is loaded
- [runtime_pattern](runtime_pattern.md): what it makes
