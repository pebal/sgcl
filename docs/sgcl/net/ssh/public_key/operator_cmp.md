[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [public_key](README.md)

# sgcl::net::ssh::operator== (sgcl::net::ssh::public_key)

```cpp
friend bool operator==(const public_key& a, const public_key& b) noexcept;
```

Whether `a` and `b` are the same key: the same blob, their comments aside. `!=` is its negation.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the keys compared |

## Return value

Whether the blobs are the same.

## Complexity

Linear in the blobs.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/ssh.h"

using namespace sgcl;

int main() {
    net::ssh::public_key key = net::ssh::public_key::parse(io::read_text("tests/net/ssh/testdata/ed25519.pub"));
    net::ssh::public_key renamed = key.with_comment("another");
    println("{} {}", key == renamed, key != net::ssh::private_key::generate().public_key());
}
```

Output:

```text
true true
```

## See also

- [sgcl::net::ssh::public_key](README.md)
