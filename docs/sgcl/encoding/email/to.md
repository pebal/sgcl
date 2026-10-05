[sgcl](../../README.md) › [encoding](../README.md) › [email](README.md)

# sgcl::encoding::email::to

```cpp
vector<address> to() const;
```

The addresses of every To field, in their order: the members of a group in its place, an empty group nothing.

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
    string head = "To: Team: a@example.com, B <b@example.com>;, c@example.com\r\n\r\n";
    auto m = encoding::email::parse(head).value();
    for (auto& a : m.to()) {
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

- [add_to](add_to.md)
- [from](from.md)
- [email](README.md)
