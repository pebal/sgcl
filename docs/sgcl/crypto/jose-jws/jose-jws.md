[sgcl](../../README.md) › [crypto](../README.md) › [jose](../jose.md) › [jws](README.md)

# sgcl::crypto::jose::jws::jws

```cpp
explicit jws(const string& text);    // (1)
jws(const jws& other) noexcept;      // (2)
```

1. The signature of a text the program spells: [parse](parse.md)'s value. Input is parsed; a text the program itself
   wrote is constructed.
2. The same signature: a copy of the handle. A move is this copy.

## Parameters

| Parameter | Description |
|---|---|
| `text` | a JWS, compact or JSON |
| `other` | another jws |

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
    auto key = crypto::jose::jwk::generate(crypto::jose::algorithm::es256);
    crypto::jose::jws j(crypto::jose::jws::sign("payload", key));
    println("{}", string(j.verify(key).value()));
}
```

Output:

```text
payload
```

## See also

- [parse](parse.md): the text that may not read, as an expected
- [sgcl::crypto::jose::jws](README.md)
