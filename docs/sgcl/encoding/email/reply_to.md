[sgcl](../../README.md) › [encoding](../README.md) › [email](README.md)

# sgcl::encoding::email::reply_to

```cpp
vector<address> reply_to() const;
```

The addresses of every Reply-To field, in their order: the members of a group in its place, an empty group nothing.

## Parameters

None.

## Return value

The [addresses](../email-address/README.md); empty when there are none.

## Complexity

Linear in the size of the fields.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;


int main() {
    string head = "Reply-To: Team: a@example.com, B <b@example.com>;, c@example.com\r\n\r\n";
    auto m = encoding::email::parse(head).value();
    for (auto& a : m.reply_to()) {
        println("{}", a.to_string());
    }
}
```

Output:

```text
a@example.com
B <b@example.com>
c@example.com
```

## See also

- [add_reply_to](add_reply_to.md)
- [from](from.md)
- [email](README.md)
