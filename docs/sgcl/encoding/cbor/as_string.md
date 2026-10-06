[sgcl](../../README.md) › [encoding](../README.md) › [cbor](README.md)

# sgcl::encoding::cbor::as_string

```cpp
optional<string> as_string() const noexcept;                // (1)
string as_string(const string& fallback) const noexcept;    // (2)
```

The text of a text string, shared, not copied; `nullopt` for every other kind (a byte string too).

1. The value, or `nullopt`.
2. The value, or `fallback` when there is none: `c["n"].as_string("?")`.

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
    println(*encoding::cbor("zażółć").as_string());
    println(encoding::cbor::bytes(vector<byte>(2)).as_string().has_value());
    println(encoding::cbor(5).as_string("?"));
}
```

Output:

```text
zażółć
false
?
```

## See also

- [as_bytes](as_bytes.md)
- [sgcl::encoding::cbor](README.md)
