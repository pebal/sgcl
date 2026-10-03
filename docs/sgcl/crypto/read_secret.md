[sgcl](../README.md) › [crypto](README.md)

# sgcl::crypto::read_secret

```cpp
#include "sgcl/crypto/read_secret.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto {
    expected<secret_bytes, io::error> read_secret(const string& path);
}
```

Reads a whole file into a [secret_bytes](secret_bytes/README.md): a private key's PEM, a password, a key file. The bytes go
straight from the file into the secret, never through managed memory, where [io::read_file](../io/file/README.md) would
give a managed `vector<byte>` that the collector frees without zeroing; every block the secret leaves as it grows is
zeroed. A private key's PEM read this way goes on to `from_pem`, which decodes it into a `secret_bytes` in turn:
`ed25519::private_key::from_pem(crypto::read_secret(path))`, where an error of the read is a
failed conversion, `bad_expected_access<io::error>`. The file is read to its end on this thread and closed before
the call returns, after a read that failed too; what was read before the failure is zeroed.

## Parameters

| Parameter | Description |
|---|---|
| `path` | the file |

## Return value

The bytes of the file, or the [io::error](../io/error/README.md) of the open or of a read that failed.

## Complexity

Linear in the size of the file.

## Exceptions

None for a regular file. A file whose reads wait (a pipe, a FIFO) waits through the reactor, and what starting its
thread throws, `std::system_error`, passes through. A file that cannot be opened or read is not an exception: it is
the error of the result.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // the Ed25519 key of the tree's TLS tests
    auto pem = crypto::read_secret("tests/net/tls_testdata/ed25519.key");
    println("{} bytes", pem->size());
    auto key = crypto::ed25519::private_key::from_pem(pem);
    println("{}", encoding::hex::encode(key->public_key().bytes()));

    auto none = crypto::read_secret("tests/net/tls_testdata/no.key");
    println("{}", none.error().message());
}
```

Output:

```text
119 bytes
30a8ffa91ff77a72f53ecc05cb416d0bde553dc5529692ce7c610e9f384bf23d
open tests/net/tls_testdata/no.key: No such file or directory
```

## See also

- [secret_bytes](secret_bytes/README.md): the secret it reads into
- [ed25519::private_key](ed25519-private_key/README.md), [x25519::private_key](x25519-private_key/README.md): `from_pem`
- [encoding::pem](../encoding/pem/README.md): PEM that is not a secret, certificates
