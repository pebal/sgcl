[sgcl](../../README.md) › [encoding](../README.md) › [cbor](README.md)

# sgcl::encoding::cbor::to_string

```cpp
string to_string() const;
```

The diagnostic notation of §8, what RFC 8949 writes its examples in: an integer and a float in decimal (a float
with its point, `1.0`, `Infinity`, `NaN`), a byte string `h'0102'`, a text string in quotes, `[1, 2]`, `{1: 2}`, a
tag `1(1363896240)`, `true`, `false`, `null`, `undefined`, `simple(16)`, an extension `extension(7, h'01')`. Written
without recursion, however deep.

## Parameters

None.

## Return value

The text.

## Complexity

Linear in the size of the value.

## Exceptions

`length_error` for a text past 4 GiB.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector<byte> bytes = encoding::hex::decode("8301a26161f5616282f93e00f6c11a514b67b0").value();
    println(encoding::cbor::parse(bytes)->to_string());
}
```

Output:

```text
[1, {"a": true, "b": [1.5, null]}, 1(1363896240)]
```

## See also

- [to_bytes](to_bytes.md)
- [sgcl::encoding::cbor](README.md)
