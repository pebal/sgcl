[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [certificate_pool](README.md)

# sgcl::crypto::x509::certificate_pool::system, async_system

```cpp
static expected<certificate_pool, error> system();                                // (1)
static async::task<expected<certificate_pool, error>> async_system() noexcept;    // (2)
```

Returns the system's roots. On macOS they are the Keychain's trust settings, read through Security.framework: the
system's anchors (`SecTrustCopyAnchorCertificates`), less those an administrator or the user distrusted for TLS
servers, with those they trusted added (`SecTrustSettingsCopyCertificates` of the admin and user domains, and each
certificate's settings, the user's over the administrator's); a root a user or an administrator added to the
Keychain is seen, one distrusted is not. When Security cannot be read there, or `SSL_CERT_FILE` or `SSL_CERT_DIR`
is set, and on every other system, they are the file the environment variable `SSL_CERT_FILE` names, else the first
of the bundles Go looks for — `/etc/ssl/certs/ca-certificates.crt`, `/etc/pki/tls/certs/ca-bundle.crt`, `/etc/ssl/ca-bundle.pem`,
`/etc/pki/tls/cacert.pem`, `/etc/pki/ca-trust/extracted/pem/tls-ca-bundle.pem`, `/etc/ssl/cert.pem` (the last is
macOS's) — else every file of the directories `SSL_CERT_DIR` names (separated by `:`), or of `/etc/ssl/certs` and
`/etc/pki/tls/certs`. They are read once per process, at the first call, and each call is given a clone: a change to the pool
returned touches no other. The roots are kept until the program ends, nothing of them runs at exit.

1. Reads the files on this thread, the first time.
2. The same, the files read on the blocking pool, for a task.

A [verification](../x509-certificate/verify.md) without roots of its own takes these, and so does a TLS client
([net::tls::config](../../net/tls/config.md)). On macOS the library links Security and CoreFoundation for it (the
CMake target does); their headers are never included: the library declares the few calls it makes itself.

## Parameters

None.

## Return value

A pool of the roots, or a [crypto::error](../error/README.md) `errc::unsupported` when none of the places holds a
certificate.

## Complexity

The first call reads and parses the Keychain's anchors or the bundle, a few hundred certificates; every call clones the pool, linear in its
size.

## Exceptions

- (1) None of its own: a file that cannot be read is passed over, the reading being [io](../../io/README.md)'s.
- (2) None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto roots = crypto::x509::certificate_pool::system();
    if (!roots) {
        eprintln(roots.error().message());
        return 1;
    }
    println("{} roots", roots->size());
}
```

Sample output:

```text
128 roots
```

In a task:

```cpp
#include "sgcl/async.h"
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<int> program() {
    auto roots = co_await crypto::x509::certificate_pool::async_system();
    if (!roots) {
        eprintln(roots.error().message());
        co_return 1;
    }
    println("{} roots", roots->size());
    co_return 0;
}

int main() {
    return async::run(program());
}
```

Sample output:

```text
128 roots
```

## See also

- [verify](../x509-certificate/verify.md): takes these roots when none are given
- [from_file](from_file.md): roots of the program's own
- [sgcl::crypto::x509::certificate_pool](README.md)
