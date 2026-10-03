[sgcl](../../README.md) › [encoding](../README.md) › [xml](../xml.md) › [token](../xml-token.md)

# sgcl::encoding::xml::token::local_name

```cpp
const string& local_name() const noexcept;
```

The name of the element of a start or an end without its prefix (`rect` of `svg:rect`). Empty for the other
tokens, an instruction included: its target is in [name](name.md).

## Parameters

None.

## Return value

The local name, or an empty string.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::xml::reader r("<d:doc xmlns:d='urn:d'><d:para/></d:doc>");
    while (auto t = r.next()) {
        println("{} {}", t->name(), t->local_name());
    }
}
```

Output:

```text
d:doc doc
d:para para
d:para para
d:doc doc
```

## See also

- [name](name.md), [namespace_uri](namespace_uri.md)
- [sgcl::encoding::xml::token](../xml-token.md)
