[sgcl](../../README.md) › [encoding](../README.md) › [xml](../xml.md) › [token](../xml-token.md)

# sgcl::encoding::xml::token::namespace_uri

```cpp
const string& namespace_uri() const noexcept;
```

The namespace of the element of a start or an end: what its prefix, or the default namespace when it has none,
stands for where the tag is. Empty for an element in no namespace and for the other tokens.

## Parameters

None.

## Return value

The namespace, or an empty string.

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
    encoding::xml::reader r("<feed xmlns='http://www.w3.org/2005/Atom'><entry xmlns=''/></feed>");
    while (auto t = r.next()) {
        if (t->type() == encoding::xml::token::kind::start_element) {
            println("{} [{}]", t->name(), t->namespace_uri());
        }
    }
}
```

Output:

```text
feed [http://www.w3.org/2005/Atom]
entry []
```

## See also

- [name](name.md), [local_name](local_name.md)
- [is_start](is_start.md): a start matched by `{namespace}local`
- [sgcl::encoding::xml::token](../xml-token.md)
