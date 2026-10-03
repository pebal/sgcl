[sgcl](../../README.md) › [net](../README.md) › [tls](README.md)

# sgcl::net::tls::category

```cpp
#include "sgcl/net/tls/error.h"   // or "sgcl/net/tls.h"

namespace sgcl::net::tls {
    const std::error_category& category() noexcept;
}
```

Returns the error category of TLS, named `"tls"`: the category of the code of every [io::error](../../io/error.md)
that a handshake or a record ends with, beside the system's for the transport's own failures (`ETIMEDOUT`,
`ECONNRESET`) and the generic one for a config refused (`EINVAL`). One object, made at the first call.

Its codes are four ranges, each worded as Go words it:

- an alert this side sent, the alert's number ([alert](alert.md)): `tls: bad certificate`;
- an alert the peer sent, 256 and its number: `remote error: tls: bad certificate`;
- a server's chain that did not verify, 512 and its [x509::reason](../../crypto/x509-reason.md):
  `tls: certificate signed by unknown authority`; the alert sent for it follows from the reason;
- an alert a server sent before its hello, in the clear, 1024 and its number: `remote error: tls: no application
  protocol, before the server's hello`. A `close_notify` or a `protocol_version` there, what a server without TLS
  1.3 answers a hello of 1.3 alone with, adds `(no TLS 1.3?)`.

A program asks with [alert_of](alert_of.md), [is_remote](is_remote.md) and
[certificate_reason](certificate_reason.md), or compares with an [alert](alert.md) for this side's, rather than with
the numbers. A number of no alert reads `tls: alert(N)`.

## Parameters

None.

## Return value

The category, the same object at every call.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"
#include "sgcl/net.h"
#include "sgcl/net/tls.h"

using namespace sgcl;

int main() {
    const std::error_category& tls_errors = net::tls::category();
    println("{}", tls_errors.name());
    println("{}", tls_errors.message(int(net::tls::alert::handshake_failure)));

    net::tls::config server_cfg;
    server_cfg.identities = {
        net::tls::identity(io::read_text("tests/net/tls_testdata/ecdsa.pem"),
                           crypto::read_secret("tests/net/tls_testdata/ecdsa.key"))};
    net::listener incoming = net::tls::listen("127.0.0.1:0", server_cfg);
    auto c = net::tls::connect("localhost:" + to_string(incoming.local_endpoint().port()));
    println("{}", c.error().code().category() == tls_errors);
    incoming.close();
}
```

Output:

```text
tls
tls: handshake failure
true
```

## See also

- [alert](alert.md), [make_error_code](make_error_code.md)
- [alert_of](alert_of.md), [is_remote](is_remote.md), [certificate_reason](certificate_reason.md)
- [io::error](../../io/error.md)
- [net::tls](README.md)
