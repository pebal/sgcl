[sgcl](../../README.md) › [encoding](../README.md) › [email](../email/README.md) › [address](README.md)

# sgcl::encoding::email::address::parse

```cpp
static expected<address, error> parse(const string& text);
```

One address read from text: a name-addr (`Name <addr>`, a route before the addr-spec skipped) or an addr-spec,
with comments and white space anywhere RFC 5322 §4 allows them.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the address |

## Return value

The address, or the [error](../error/README.md) `errc::syntax` at the offset where the text stops being one ("more than one address" for a list).

## Complexity

Linear in the size of the text.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;


int main() {
    string text = "Pete(A nice \\) chap) <pete(his account)@silly.test(his host)>";
    auto a = encoding::email::address::parse(text);
    println("{} {}", a->name(), a->addr());
    println("{}", encoding::email::address::parse("Ann <ann@x").error().message());
    println("{}", encoding::email::address::parse("a@x, b@y").error().message());
}
```

Output:

```text
Pete pete@silly.test
offset 10: invalid address
offset 0: more than one address
```

## See also

- [parse_list](parse_list.md)
- [address](README.md)
