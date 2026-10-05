[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [ocsp_response](README.md)

# sgcl::crypto::x509::ocsp_response::produced_at

```cpp
time::datetime produced_at() const noexcept;
```

Returns when the responder signed the response, its producedAt, in UTC. A response is current by the times of its single response, not by this one ([ocsp_single_response](../x509-ocsp_single_response.md)).

## Parameters

None.

## Return value

The time.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"
#include "sgcl/time.h"

using namespace sgcl;

int main() {
    string dir = "tests/crypto/data/revocation/";
    auto leaf = crypto::x509::certificate::from_pem(io::read_text(dir + "good.pem")).value();
    auto issuer = crypto::x509::certificate::from_pem(io::read_text(dir + "int.pem")).value();
    auto response = crypto::x509::ocsp_response::parse(
        io::read_file(dir + "ocsp_good.der").value()).value();
    println("{}", response.produced_at() <= response.responses()[0].next_update.value());
}
```

Output:

```text
true
```

## See also

- [responses](responses.md): the times each status is current for
- [sgcl::crypto::x509::ocsp_response](README.md)
