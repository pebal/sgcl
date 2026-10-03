[sgcl](../../README.md) › [crypto](../README.md) › [error](../error.md)

# sgcl::crypto::error::reason

```cpp
x509::reason reason() const noexcept;
```

Returns why a certificate chain does not verify, one of [x509::reason](../x509-reason.md), for an error whose
[code](code.md) is `errc::verification`; `x509::reason::none` for every other error. Go's `crypto/x509` tells these
apart by the types of its errors and their `InvalidReason`; here they are one list, read by a `switch`. The
certificate at fault is named in [message](message.md).

## Parameters

None.

## Return value

The reason, or `x509::reason::none`.

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
    crypto::error expired(crypto::x509::reason::expired, "expired: CN=example.com");
    println("{}", expired.code() == crypto::errc::verification);
    println("{}", expired.reason() == crypto::x509::reason::expired);

    crypto::error forged(crypto::errc::authentication);
    println("{}", forged.reason() == crypto::x509::reason::none);
}
```

Output:

```text
true
true
true
```

## See also

- [x509::reason](../x509-reason.md): the reasons
- [x509](../x509.md): certificates and their verification
- [sgcl::crypto::error](../error.md)
