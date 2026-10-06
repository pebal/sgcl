[sgcl](../../README.md) › [txt](../README.md) › [code_error](README.md)

# sgcl::txt::code_error::code_error

```cpp
constexpr code_error(kind what, size_t offset) noexcept;
```

Constructs the error of a text read as a code of the kind `what`, which stopped at the byte `offset`. The parse
functions make it; a program has no need to.

## Parameters

| Parameter | Description |
|---|---|
| `what` | the kind of code: `kind::currency`, `kind::region`, `kind::script` |
| `offset` | the byte the reading stopped on |

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
    txt::code_error e(txt::code_error::kind::currency, 2);
    println("{} {}", e.offset(), e.message());
}
```

Output:

```text
2 not a currency code: three ASCII letters expected
```

## See also

- [message](message.md)
- [sgcl::txt::code_error](README.md)
