[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [public_key](README.md)

# sgcl::net::ssh::public_key::parse

```cpp
static expected<public_key, io::error> parse(const string& text) noexcept;
```

A key from its line, as authorized_keys and .pub files write it: its type's name, its blob in base64 and a comment
(the rest of the line). The type named must be the blob's. Options in front, as authorized_keys allows them, are
[authorized_keys](../authorized_keys/README.md)'s to read.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the line |

## Return value

The key. Or the [io::error](../../../io/error/README.md), op `ssh key`, `crypto::errc::malformed` for a text that is not a key's line (a type not read here, base64 that does not decode, a blob that is not of the type named).

## Complexity

Linear in the text.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/ssh.h"

using namespace sgcl;

int main() {
    auto key = net::ssh::public_key::parse("ecdsa-sha2-nistp256 AAAAE2VjZHNhLXNoYTItbmlzdHAyNTYAAAAIbmlzdHAyNTYAAABBBM+eckEpfCOeJZMcX1FmptBgIfy2B/rWV4LgnFmSc9kibUF9HN7kPDVjolnArYhYABe/AgxoIE51x6SlDpIayds= test-p256");
    println("{} {}", key->type_name(), key->comment());
    println("{}", net::ssh::public_key::parse("ssh-dss AAAA").error().code() == crypto::errc::malformed);
}
```

Output:

```text
ecdsa-sha2-nistp256 test-p256
true
```

## See also

- [to_string](to_string.md)
- [from_bytes](from_bytes.md)
- [sgcl::net::ssh::public_key](README.md)
