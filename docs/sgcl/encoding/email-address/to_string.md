[sgcl](../../README.md) › [encoding](../README.md) › [email](../email/README.md) › [address](README.md)

# sgcl::encoding::email::address::to_string

```cpp
string to_string() const;
```

The address as a field writes it: the addr-spec alone when there is no name; the name as a phrase of atoms when it
is one, a quoted string when it holds specials, encoded words (UTF-8) when it holds what neither can; a domain
past ASCII by IDNA.

## Parameters

None.

## Return value

The text, ASCII.

## Complexity

Linear in the size of the address.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;


int main() {
    println("{}", encoding::email::address("Alice", "alice@example.com").to_string());
    println("{}", encoding::email::address("Doe, Alice", "alice@example.com").to_string());
    println("{}", encoding::email::address("Zoë", "zoe@exämple.fr").to_string());
    println("{}", encoding::email::address("", "alice@example.com").to_string());
}
```

Output:

```text
Alice <alice@example.com>
"Doe, Alice" <alice@example.com>
=?utf-8?q?Zo=C3=AB?= <zoe@xn--exmple-cua.fr>
alice@example.com
```

## See also

- [parse](parse.md)
- [address](README.md)
