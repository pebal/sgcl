[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [public_key](README.md)

# sgcl::net::ssh::public_key::bytes

```cpp
vector<byte> bytes() const noexcept;
```

The blob: the key's wire form (RFC 4253 §6.6), what the base64 of its line holds and what is hashed for its fingerprint.

## Parameters

None.

## Return value

The blob.

## Complexity

Linear in the blob.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/ssh.h"

using namespace sgcl;

int main() {
    net::ssh::public_key key = net::ssh::public_key::parse(io::read_text("tests/net/ssh/testdata/ed25519.pub"));
    println("{} bytes", key.bytes().size());
}
```

Output:

```text
51 bytes
```

## See also

- [from_bytes](from_bytes.md), [fingerprint](fingerprint.md)
- [sgcl::net::ssh::public_key](README.md)
