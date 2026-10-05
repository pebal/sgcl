[sgcl](../../README.md) › [encoding](../README.md) › [email](../email/README.md)

# sgcl::encoding::email::address

```cpp
#include "sgcl/encoding/email.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    class email {
    public:
        class address;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::encoding::email::address` is an address of mail (RFC 5322 §3.4): a display name and an addr-spec,
`Alice Doe <alice@example.com>`. Go's `mail.Address`, Python's `email.headerregistry.Address`. A value of two
strings: [name](name.md), decoded, and [addr](addr.md); [to_string](to_string.md) writes it as a field does
(the name quoted as it needs, encoded words for a name past ASCII), [parse](parse.md) and
[parse_list](parse_list.md) read one or a list with the obsolete forms of RFC 5322 §4.4 (a route, comments, words
of a local part joined by dots with white space between), groups' members in their place.

## Rules

- A value: copied, compared ([operator==](operator_cmp.md): the name and the addr-spec), held anywhere a string is.
- The addr-spec is kept as read: a local part that is not a dot-atom stays quoted (`"john doe"@example.com`), a
  quoted dot-atom loses its quotes, a domain literal keeps its brackets.
- A name comes from the phrase before the angle brackets, its encoded words decoded, its words joined by one
  space; or, for an addr-spec alone, from the comment after it (`jdoe@example.org (John Doe)`).
- The text form never holds a byte past ASCII: a name past ASCII is written as encoded words (UTF-8, Q or B, the
  shorter), a domain past ASCII by IDNA; [email](../email/README.md) writes UTF-8 where the transport takes it.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](email-address.md) | an empty address, one read from text, or a name and an addr-spec |
| [name](name.md) | the display name |
| [addr](addr.md) | the addr-spec |
| [local_part](local_part.md) | the addr-spec before its last `@` |
| [domain](domain.md) | the addr-spec after its last `@` |
| [to_string](to_string.md) | the address as a field writes it |
| [parse](parse.md) | one address read from text (static) |
| [parse_list](parse_list.md) | an address list read from text (static) |

## Non-member functions

| Function | Description |
|---|---|
| [operator==](operator_cmp.md) | whether two addresses are the same |

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;


int main() {
    encoding::email::address a("\"Doe, Jane\" <jane@example.org>");
    println("{} | {} | {}", a.name(), a.addr(), a.to_string());
    auto list = encoding::email::address::parse_list("Team: a@x.example, B <b@x.example>;");
    for (auto& b : list.value()) {
        println("{}", b.to_string());
    }
}
```

Output:

```text
Doe, Jane | jane@example.org | "Doe, Jane" <jane@example.org>
a@x.example
B <b@x.example>
```

## See also

- [email](../email/README.md): the messages whose fields hold them
- RFC 5322 §3.4, §4.4
