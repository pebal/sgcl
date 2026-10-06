[sgcl](../../README.md) › [txt](../README.md) › [region](README.md)

# sgcl::txt::region::parse

```cpp
static expected<region, code_error> parse(const string& code) noexcept;
```

Reads a code from outside the program: two ASCII letters in any case (`"pl"` is PL) or three digits (`"419"`).

## Parameters

| Parameter | Description |
|---|---|
| `code` | the text |

## Return value

The value, or a [code_error](../code_error/README.md) with the byte the reading stopped on.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"
#include "sgcl/txt/names/de.h"
#include "sgcl/txt/names/pl.h"

using namespace sgcl;

int main() {
    for (auto code : {"de", "DE", "x1", ""}) {
        auto r = txt::region::parse(code);
        println("[{}] {}", code, r ? r->code() : r.error().message());
    }
}
```

Output:

```text
[de] DE
[DE] DE
[x1] not a region code: two ASCII letters or three digits expected
[] not a region code: two ASCII letters or three digits expected
```

## See also

- [(constructor)](region.md)
- [code_error](../code_error/README.md)
- [sgcl::txt::region](README.md)
