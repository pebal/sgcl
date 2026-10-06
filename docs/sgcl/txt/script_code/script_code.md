[sgcl](../../README.md) › [txt](../README.md) › [script_code](README.md)

# sgcl::txt::script_code::script_code

```cpp
constexpr script_code() noexcept = default;    // (1)
explicit script_code(const string& code);      // (2)
```

Constructs a script.

1. None: its [operator bool](operator_bool.md) is `false` and its [code](code.md) empty.
2. The one of the code `code`, as [parse](parse.md) reads it: a code the program itself writes.

## Parameters

| Parameter | Description |
|---|---|
| `code` | the code: `"Latn"`, `"Hans"` |

## Complexity

Constant.

## Exceptions

`bad_expected_access<code_error>` with the error of [parse](parse.md) when `code` is not one (2).

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"
#include "sgcl/txt/names/de.h"
#include "sgcl/txt/names/pl.h"

using namespace sgcl;

int main() {
    txt::script_code x("latn");
    println("{} {}", x.code(), bool(txt::script_code()));
}
```

Output:

```text
Latn false
```

## See also

- [parse](parse.md)
- [sgcl::txt::script_code](README.md)
