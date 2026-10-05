[sgcl](../../README.md) › [encoding](../README.md) › [email](README.md)

# sgcl::encoding::email::add_to

```cpp
email& add_to(const address& a);      // (1)
email& add_to(const string& list);    // (2)
```

Addresses added to the To field (made when there is none).

1. One address.
2. The addresses of a list read from text, `"a@x, B <b@y>"`, groups' members among them.

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
    m.add_to("a@example.com, B <b@example.com>");
    m.add_to(encoding::email::address("Cé", "c@example.com"));
    println("{}", m.header("To"));
    println("{}", m.to().size());
}
```

Output:

```text
a@example.com, B <b@example.com>, Cé <c@example.com>
3
```

## See also

- [to](to.md)
- [email](README.md)
