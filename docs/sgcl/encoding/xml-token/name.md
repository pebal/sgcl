[sgcl](../../README.md) › [encoding](../README.md) › [xml](../xml.md) › [token](../xml-token.md)

# sgcl::encoding::xml::token::name

```cpp
const string& name() const noexcept;
```

The name of the element of a start or an end as written, its prefix with it (`svg:rect`); the target of an
instruction (`xml` for the XML declaration); the root's name a DOCTYPE declaration gives. Empty for a text and a
comment. The prefix is always resolved as well: there is no raw token, as Go's `RawToken` gives.

## Parameters

None.

## Return value

The name, the target, or an empty string.

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
    encoding::xml::reader r(
        "<?xml version='1.0'?><!DOCTYPE svg:svg><svg:svg xmlns:svg='http://www.w3.org/2000/svg'/>");
    while (auto t = r.next()) {
        println(t->name());
    }
}
```

Output:

```text
xml
svg:svg
svg:svg
svg:svg
```

## See also

- [local_name](local_name.md), [namespace_uri](namespace_uri.md): the parts of an element's name
- [sgcl::encoding::xml::token](../xml-token.md)
