[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [public_key](README.md)

# sgcl::net::ssh::public_key::public_key

```cpp
public_key() noexcept;                      // (1)
explicit public_key(const string& text);    // (2)
public_key(const public_key&) = default;    // (3), implicitly declared
```

1. An empty key: `operator bool` is `false`, and it equals no key but another empty one.
2. The key of a line, as [parse](parse.md) reads it; a text that is not one is `std::invalid_argument`.
3. A copy.

## Parameters

| Parameter | Description |
|---|---|
| `text` | an authorized_keys or .pub line |

## Complexity

- (1, 3) Constant.
- (2) Linear in the text.

## Exceptions

- (1, 3) None.
- (2) `std::invalid_argument` for a text that is not a key's line.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/ssh.h"

using namespace sgcl;

int main() {
    net::ssh::public_key none;
    net::ssh::public_key key("ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIO7ky0DPyKt2to+tNXhrGYihNJR5UIxJ5QQifpYnXOMl test-ed25519");
    println("{} {}", (bool)none, key.comment());
}
```

Output:

```text
false test-ed25519
```

## See also

- [parse](parse.md)
- [sgcl::net::ssh::public_key](README.md)
