[sgcl](../../README.md) › [txt](../README.md) › [script_code](README.md)

# sgcl::txt::script_code::parse

```cpp
static expected<script_code, code_error> parse(const string& code) noexcept;
```

Reads a code from outside the program: four ASCII letters in any case (`"latn"` is Latn).

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
    for (auto code : {"latn", "Latn", "x1", ""}) {
        auto r = txt::script_code::parse(code);
        println("[{}] {}", code, r ? r->code() : r.error().message());
    }
}
```

Output:

```text
[latn] Latn
[Latn] Latn
[x1] not a script code: four ASCII letters expected
[] not a script code: four ASCII letters expected
```

## See also

- [(constructor)](script_code.md)
- [code_error](../code_error/README.md)
- [sgcl::txt::script_code](README.md)
