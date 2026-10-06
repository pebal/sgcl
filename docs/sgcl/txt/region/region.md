[sgcl](../../README.md) › [txt](../README.md) › [region](README.md)

# sgcl::txt::region::region

```cpp
constexpr region() noexcept = default;    // (1)
explicit region(const string& code);      // (2)
```

Constructs a region.

1. None: its [operator bool](operator_bool.md) is `false` and its [code](code.md) empty.
2. The one of the code `code`, as [parse](parse.md) reads it: a code the program itself writes.

## Parameters

| Parameter | Description |
|---|---|
| `code` | the code: `"PL"`, `"419"` |

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
    txt::region x("de");
    println("{} {}", x.code(), bool(txt::region()));
}
```

Output:

```text
DE false
```

## See also

- [parse](parse.md)
- [sgcl::txt::region](README.md)
