[sgcl](../../README.md) › [crypto](../README.md) › [error](README.md)

# sgcl::crypto::operator== (sgcl::crypto::error)

```cpp
friend bool operator==(const error& a, const error& b) noexcept;
```

Compares two errors: equal when the codes, the reasons, the offsets and the texts are all equal. `!=` is made from it
by the compiler. To ask what kind of error it is, compare its [code](code.md): two errors of one code but different
texts are not equal.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the errors compared |

## Return value

Whether the errors are equal.

## Complexity

Linear in the length of the shorter text.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    crypto::error a(crypto::errc::malformed, 17);
    crypto::error b(crypto::errc::malformed, 17);
    crypto::error c(crypto::errc::malformed, 17, "DER: length past the end");
    println("{} {}", a == b, a == c);
    println("{}", a.code() == c.code());
}
```

Output:

```text
true false
true
```

## See also

- [code](code.md): the kind of the error
- [sgcl::crypto::error](README.md)
