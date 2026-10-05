[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [public_key](README.md)

# sgcl::net::ssh::public_key::comment

```cpp
const string& comment() const noexcept;
```

The comment its line had after the base64 (`user@host`); empty for a key from a blob.

## Parameters

None.

## Return value

The comment.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/ssh.h"

using namespace sgcl;

int main() {
    net::ssh::public_key key = net::ssh::public_key::parse(io::read_text("tests/net/ssh/testdata/ed25519.pub"));
    println("{}", key.comment());
}
```

Output:

```text
test-ed25519
```

## See also

- [with_comment](with_comment.md)
- [sgcl::net::ssh::public_key](README.md)
