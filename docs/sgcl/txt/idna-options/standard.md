[sgcl](../../README.md) › [txt](../README.md) › [idna](../idna.md) › [options](../idna-options.md)

# sgcl::txt::idna::options::standard

```cpp
static constexpr options standard() noexcept;
```

Returns the strict profile, `options{}`: everything checked, the name held to what the DNS will carry, nontransitional
processing, the ASCII of STD3 not enforced. What the defaults of [to_ascii](../idna/to_ascii.md) and
[to_unicode](../idna/to_unicode.md) are.

## Parameters

None.

## Return value

The options with every field at its default.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    constexpr auto o = txt::idna::options::standard();
    println("{} {} {}", o.check_hyphens, o.verify_dns_length, o.transitional);
    auto host = txt::idna::to_ascii("a..c", o);
    println("{}", host ? *host : host.error().message());
}
```

Output:

```text
true true false
a label with nothing in it
```

## See also

- [whatwg](whatwg.md): the profile of a URL parser
- [sgcl::txt::idna::options](../idna-options.md)
