[sgcl](../../README.md) › [crypto](../README.md) › [jose](../jose.md) › [jwe](README.md)

# sgcl::crypto::jose::jwe::jwe

```cpp
explicit jwe(const string& compact);    // (1)
jwe(const jwe& other) noexcept;         // (2)
```

1. The encryption of a text the program spells: [parse](parse.md)'s value. Input is parsed; a text the program itself
   wrote is constructed.
2. The same encryption: a copy of the handle. A move is this copy.

## Parameters

| Parameter | Description |
|---|---|
| `compact` | a JWE, compact |
| `other` | another jwe |

## Complexity

- (1) Linear in the length of the text.
- (2) Constant.

## Exceptions

- (1) `bad_expected_access<crypto::error>` with [parse](parse.md)'s error for a text that does not read.
- (2) None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::jose::jwk::generate(crypto::jose::algorithm::a256gcmkw);
    crypto::jose::jwe j(crypto::jose::jwe::encrypt("payload", key));
    println("{}", string(j.decrypt(key).value()));
}
```

Output:

```text
payload
```

## See also

- [parse](parse.md): the text that may not read, as an expected
- [sgcl::crypto::jose::jwe](README.md)
