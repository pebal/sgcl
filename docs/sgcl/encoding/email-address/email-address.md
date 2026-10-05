[sgcl](../../README.md) › [encoding](../README.md) › [email](../email/README.md) › [address](README.md)

# sgcl::encoding::email::address::address

```cpp
address() noexcept;                                          // (1)
explicit address(const string& text);                        // (2)
address(const string& name, const string& addr) noexcept;    // (3)
```

1. An empty address: no name, no addr-spec.
2. The address read from text, as [parse](parse.md) reads it: `"Alice <alice@example.com>"`, `"alice@example.com"`.
3. The name (any text, empty for none) and the addr-spec as given, unchecked.

## Parameters

| Parameter | Description |
|---|---|
| `text` | an address as RFC 5322 writes one |
| `name` | the display name |
| `addr` | the addr-spec |

## Complexity

Linear in the size of the text.

## Exceptions

- (2) `invalid_argument` when `text` is not one address.
- (1, 3) None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;


int main() {
    encoding::email::address a("Bob <bob@example.org>");
    encoding::email::address b("Bob", "bob@example.org");
    println("{} {}", a == b, encoding::email::address().addr().empty());
    try {
        encoding::email::address bad("Bob <bob@");
    } catch (const invalid_argument&) {
        println("invalid_argument");
    }
}
```

Output:

```text
true true
invalid_argument
```

## See also

- [parse](parse.md)
- [address](README.md)
