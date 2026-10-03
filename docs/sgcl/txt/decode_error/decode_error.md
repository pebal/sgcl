[sgcl](../../README.md) › [txt](../README.md) › [decode_error](../decode_error.md)

# sgcl::txt::decode_error::decode_error

```cpp
decode_error(size_t offset, encoding from) noexcept;
```

Constructs the error of the byte at `offset` in the encoding `from`. The strict [decode](../decode.md) makes them; a function of the program that reads an encoding may make its own.

## Parameters

| Parameter | Description |
|---|---|
| `offset` | the byte that means nothing, counted from the start of the bytes |
| `from` | the encoding |

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
    txt::decode_error e(7, txt::encoding::koi8_r);
    println("{} {}", e.offset(), e.message());
}
```

Output:

```text
7 not koi8-r
```

## See also

- [sgcl::txt::decode_error](../decode_error.md)
