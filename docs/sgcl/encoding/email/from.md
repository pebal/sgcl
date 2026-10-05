[sgcl](../../README.md) › [encoding](../README.md) › [email](README.md)

# sgcl::encoding::email::from

```cpp
optional<address> from() const;
```

The first mailbox of the From field ([address](../email-address/README.md)): its name decoded, its addr-spec.

## Parameters

None.

## Return value

The address, or nothing when there is no From or it holds no address.

## Complexity

Linear in the size of the field.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;


int main() {
    string head = "From: =?utf-8?q?=C5=81ucja?= <lucja@example.pl>\r\n\r\n";
    auto m = encoding::email::parse(head).value();
    println("{} {}", m.from()->name(), m.from()->addr());
    println("{}", encoding::email::parse("").value().from().has_value());
}
```

Output:

```text
Łucja lucja@example.pl
false
```

## See also

- [set_from](set_from.md)
- [to](to.md)
- [email](README.md)
