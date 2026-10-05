[sgcl](../../../README.md) › [net](../../README.md) › [tls](../README.md) › [identity](README.md)

# sgcl::net::tls::identity::ocsp_staple

```cpp
vector<byte> ocsp_staple() const noexcept;
```

Returns the OCSP response a server staples to the identity's leaf now: the one [set_ocsp_staple](set_ocsp_staple.md)
set, or the last one a server with `config::ocsp_stapling` fetched. A staple past its `nextUpdate`, which is not sent,
is not returned either.

## Parameters

None.

## Return value

A copy of the response's DER; empty when there is none.

## Complexity

Linear in the size of the response.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"
#include "sgcl/net/tls.h"

using namespace sgcl;

int main() {
    string dir = "tests/crypto/data/revocation/";
    net::tls::identity good(io::read_text(dir + "good.pem").value() +
                                io::read_text(dir + "int.pem").value(),
                            crypto::read_secret(dir + "good.key"));
    println("{}", good.ocsp_staple().size());
    good.set_ocsp_staple(io::read_file(dir + "ocsp_good.der").value());
    auto staple = crypto::x509::ocsp_response::parse(good.ocsp_staple()).value();
    println("{}", staple.responses()[0].status == crypto::x509::revocation_status::good);
    good.set_ocsp_staple({});
    println("{}", good.ocsp_staple().size());
}
```

Output:

```text
0
true
0
```

## See also

- [set_ocsp_staple](set_ocsp_staple.md): the staple set or cleared
- [sgcl::net::tls::identity](README.md)
