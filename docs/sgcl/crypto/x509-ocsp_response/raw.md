[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [ocsp_response](README.md)

# sgcl::crypto::x509::ocsp_response::raw

```cpp
slice<const byte> raw() const noexcept;
```

Returns the whole response, the DER it was read from: what a server staples.

## Parameters

None.

## Return value

A view of the response's bytes, which it keeps alive.

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
    string dir = "tests/crypto/data/revocation/";
    auto leaf = crypto::x509::certificate::from_pem(io::read_text(dir + "good.pem")).value();
    auto issuer = crypto::x509::certificate::from_pem(io::read_text(dir + "int.pem")).value();
    auto response = crypto::x509::ocsp_response::parse(
        io::read_file(dir + "ocsp_good.der").value()).value();
    println("{} bytes", response.raw().size());
}
```

Output:

```text
717 bytes
```

## See also

- [raw_tbs](raw_tbs.md): what its signature covers
- [sgcl::crypto::x509::ocsp_response](README.md)
