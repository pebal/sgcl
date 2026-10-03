[sgcl](../../README.md) › [encoding](../README.md) › [xml](README.md)

# sgcl::encoding::xml::name

```cpp
string name() const noexcept;
```

The name of an element as the document writes it, its prefix with it (`svg:rect`); the target of an instruction;
an empty string for a text, a comment and `xml()`. The string is returned by value, which is one word: `xml()` has
no string to refer to.

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
    auto svg = encoding::xml::parse(
        "<svg:svg xmlns:svg='http://www.w3.org/2000/svg'><svg:rect/><?render fast?>text</svg:svg>");
    for (auto& node : svg->children()) {
        println("[{}]", node.name());
    }
}
```

Output:

```text
[svg:rect]
[render]
[]
```

## See also

- [local_name](local_name.md), [namespace_uri](namespace_uri.md): the parts of a name
- [sgcl::encoding::xml](README.md)
