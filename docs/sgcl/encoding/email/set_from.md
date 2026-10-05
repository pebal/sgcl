[sgcl](../../README.md) › [encoding](../README.md) › [email](README.md)

# sgcl::encoding::email::set_from

```cpp
email& set_from(const address& a);      // (1)
email& set_from(const string& text);    // (2)
```

The From field of one address.

1. The address given.
2. The address read from text, `"Alice <alice@example.com>"`.

A Message-ID the constructor made moves to the domain of the new address.

## Parameters

| Parameter | Description |
|---|---|
| `a` | the sender |
| `text` | the sender as text |

## Return value

`*this`.

## Complexity

Constant.

## Exceptions

- (2) `invalid_argument` when `text` is not one address; the message is unchanged.
- (1) None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;


int main() {
    encoding::email m;
    m.set_from(encoding::email::address("Doe, Jane", "jane@example.org"));
    println("{}", m.header("From"));
    m.set_from("Bob <bob@example.net>");
    println("{} {}", m.from()->name(), m.message_id().view().ends_with("@example.net>"));
}
```

Output:

```text
"Doe, Jane" <jane@example.org>
Bob true
```

## See also

- [from](from.md)
- [address](../email-address/README.md)
- [email](README.md)
