[sgcl](../../README.md) › [encoding](../README.md) › [xml](README.md)

# sgcl::encoding::xml::exists

```cpp
bool exists() const noexcept;
```

Checks whether the node is a node: `false` only for `xml()`, which [child](child.md) gives when there is no such
child. A chain of `child` calls needs no check at each step, and `exists()` at its end tells whether the element
was there.

## Parameters

None.

## Return value

`true` for an element, a text, a comment or an instruction; `false` for `xml()`.

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
    auto doc = encoding::xml::parse("<config><server><port>80</port></server></config>").value();
    println(doc.child("server").child("port").exists());
    println(doc.child("client").child("port").exists());
    println(encoding::xml().exists());
}
```

Output:

```text
true
false
false
```

## See also

- [type](type.md): what the node is
- [child](child.md): the first element of a name
- [sgcl::encoding::xml](README.md)
