[sgcl](../../README.md) › [crypto](../README.md) › [jose](../jose.md) › [jws](README.md)

# sgcl::crypto::jose::jws::unverified_payload

```cpp
vector<byte> unverified_payload() const;
```

The payload as the JWS carries it, decoded, before any verification: to look at what a token says before its key is
chosen (an issuer, a tenant), never to act on.

## Parameters

None.

## Return value

The payload.

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
    auto key = crypto::jose::jwk::symmetric("a secret of thirty-two bytes ...", {.kid = "s1"});
    auto token = crypto::jose::jws::sign_json("hello", crypto::jose::jwk_set{key});
    auto j = crypto::jose::jws::parse(token);
    println("{}", string(j->unverified_payload()));
}
```

Output:

```text
hello
```

## See also

- [verify](verify.md): the payload verified
- [sgcl::crypto::jose::jws](README.md)
