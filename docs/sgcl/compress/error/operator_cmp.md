[sgcl](../../README.md) › [compress](../README.md) › [error](README.md)

# sgcl::compress::operator== (sgcl::compress::error)

```cpp
friend bool operator==(const error& a, const error& b) noexcept;
```

Compares two errors whole: the same code at the same offset (and whether the error has a place in the data at all),
with the same detail and the same stream error. `!=` is made from it by the compiler.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the errors compared |

## Return value

`true` when the code, the place, the detail and the stream's error are equal.

## Complexity

Linear in the length of the details.

## Exceptions

None.

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    compress::error a(compress::errc::checksum, 12);
    println("{}", a == compress::error(compress::errc::checksum, 12));
    println("{}", a == compress::error(compress::errc::checksum, 13));
    println("{}", a == compress::error(compress::errc::checksum, 12, "gzip: CRC-32 mismatch"));
}
```

Output:

```text
true
false
false
```

## See also

- [code](code.md): to compare the code alone
- [sgcl::compress::error](README.md)
