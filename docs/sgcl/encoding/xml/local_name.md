[sgcl](../../README.md) › [encoding](../README.md) › [xml](../xml.md)

# sgcl::encoding::xml::local_name

```cpp
string local_name() const noexcept;
```

The name of an element without its prefix (`rect` of `svg:rect`); the target of an instruction; an empty string
for a text, a comment and `xml()`. With [namespace_uri](namespace_uri.md) it names the element whatever prefix
the document chose, as Go's `xml.Name{Space, Local}` does.

## Parameters

None.

## Return value

The local name, the target, or an empty string.

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
    auto a = encoding::xml::parse("<a:item xmlns:a='urn:shop'/>").value();
    auto b = encoding::xml::parse("<item xmlns='urn:shop'/>").value();
    println("{} {} {}", a.name(), a.local_name(), a.namespace_uri());
    println("{} {} {}", b.name(), b.local_name(), b.namespace_uri());
}
```

Output:

```text
a:item item urn:shop
item item urn:shop
```

## See also

- [name](name.md), [namespace_uri](namespace_uri.md)
- [sgcl::encoding::xml](../xml.md)
