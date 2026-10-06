[sgcl](../../README.md) › [crypto](../README.md) › [otp_key](README.md)

# sgcl::crypto::otp_key::clone

```cpp
otp_key clone() const;
```

A second key with the same secret, issuer, account, options and counter: a copy of a secret, made on purpose and by
name.

## Parameters

None.

## Return value

The copy.

## Complexity

Linear in the lengths of the secret, the issuer and the account.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    crypto::otp_key key = crypto::otp_key::generate("Example", "alice");
    crypto::otp_key copy = key.clone();
    println("{}", copy.to_string() == key.to_string());
}
```

Output:

```text
true
```

## See also

- [(constructor)](otp_key.md): a key taken over
- [sgcl::crypto::otp_key](README.md)
