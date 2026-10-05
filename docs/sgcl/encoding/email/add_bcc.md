[sgcl](../../README.md) › [encoding](../README.md) › [email](README.md)

# sgcl::encoding::email::add_bcc

```cpp
email& add_bcc(const address& a);      // (1)
email& add_bcc(const string& list);    // (2)
```

Addresses added to the Bcc field (made when there is none).

1. One address.
2. The addresses of a list read from text, `"a@x, B <b@y>"`, groups' members among them. Bcc is written by [to_string](to_string.md) unless [write_options](../email-write_options.md) say otherwise, and never sent by [net::smtp](../../net/smtp/README.md).

## Parameters

| Parameter | Description |
|---|---|
| `a` | the address |
| `list` | an address list as text |

## Return value

`*this`.

## Complexity

Constant.

## Exceptions

- (2) `invalid_argument` when `list` is not an address list; the message is unchanged.
- (1) None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;


int main() {
    encoding::email m;
    m.add_bcc("a@example.com, B <b@example.com>");
    m.add_bcc(encoding::email::address("Cé", "c@example.com"));
    println("{}", m.header("Bcc"));
    println("{}", m.bcc().size());
}
```

Output:

```text
a@example.com, B <b@example.com>, Cé <c@example.com>
3
```

## See also

- [bcc](bcc.md)
- [email](README.md)
