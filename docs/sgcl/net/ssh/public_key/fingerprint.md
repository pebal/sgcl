[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [public_key](README.md)

# sgcl::net::ssh::public_key::fingerprint

```cpp
string fingerprint() const noexcept;
```

The key's fingerprint as `ssh-keygen -l` and ssh's prompts print it: `SHA256:` and the unpadded base64 of the SHA-256 of its blob.

## Parameters

None.

## Return value

The fingerprint.

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
    println("{}", key.fingerprint());
}
```

Output:

```text
SHA256:PN89yHvZV5qRnQP3eclDnzJi7J8IfYfMitxf9jaYMn0
```

## See also

- [bytes](bytes.md)
- [sgcl::net::ssh::public_key](README.md)
