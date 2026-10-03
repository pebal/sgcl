[sgcl](../../README.md) › [crypto](../README.md) › [hkdf](../hkdf/README.md) › [prk](README.md)

# sgcl::crypto::hkdf\<H\>::prk::clone

```cpp
prk clone() const noexcept;
```

A second object with the same key: the copy a PRK has no copy constructor for, asked for by name, since a copy of a
secret should be meant. The clone zeroes its own bytes when it dies, as any PRK does.

## Parameters

None.

## Return value

The new PRK.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector<byte> ikm(22, byte(0x0b));
    auto master = crypto::hkdf_sha256::extract("", ikm);
    auto copy = master.clone();
    auto a = crypto::hkdf_sha256::expand(master, "info", 32);
    auto b = crypto::hkdf_sha256::expand(copy, "info", 32);
    println("{}", a == b);
}
```

Output:

```text
true
```

## See also

- [(constructor)](hkdf-prk.md): the move constructor; no copy
- [sgcl::crypto::hkdf\<H\>::prk](README.md)
