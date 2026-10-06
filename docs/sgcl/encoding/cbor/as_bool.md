[sgcl](../../README.md) › [encoding](../README.md) › [cbor](README.md)

# sgcl::encoding::cbor::as_bool

```cpp
optional<bool> as_bool() const noexcept;       // (1)
bool as_bool(bool fallback) const noexcept;    // (2)
```

The value of a boolean; `nullopt` for every other kind.

1. The value, or `nullopt`.
2. The value, or `fallback` when there is none: `c["n"].as_bool(false)`.

## Parameters

| Parameter | Description |
|---|---|
| `fallback` | what (2) gives when there is no value |

## Return value

(1) The value, or `nullopt`; (2) the value, or `fallback`.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    println(encoding::cbor(true).as_bool());
    println(encoding::cbor(1).as_bool());
    println(encoding::cbor(1).as_bool(false));
}
```

Output:

```text
true
nullopt
false
```

## See also

- [sgcl::encoding::cbor](README.md)
