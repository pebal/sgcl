[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [public_key](README.md)

# sgcl::net::ssh::public_key::from_bytes

```cpp
static expected<public_key, io::error> from_bytes(const slice<const byte>& blob) noexcept;
```

A key from its blob, the wire form of RFC 4253 §6.6 (or a certificate's of PROTOCOL.certkeys): what [bytes](bytes.md) gives and an agent lists. No comment.

## Parameters

| Parameter | Description |
|---|---|
| `blob` | the key's wire form |

## Return value

The key. Or the [io::error](../../../io/error/README.md), `crypto::errc::malformed` for bytes that are not a key of a kind read here.

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
    net::ssh::public_key same = net::ssh::public_key::from_bytes(key.bytes());
    println("{} [{}]", same == key, same.comment());
}
```

Output:

```text
true []
```

## See also

- [bytes](bytes.md)
- [sgcl::net::ssh::public_key](README.md)
