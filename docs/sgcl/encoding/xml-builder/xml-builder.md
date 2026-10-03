[sgcl](../../README.md) › [encoding](../README.md) › [xml](../xml.md) › [builder](../xml-builder.md)

# sgcl::encoding::xml::builder::builder

```cpp
explicit builder(const string& name);
```

Constructs a builder of the element `name`, with no attributes and no children yet. The name is a qualified name of
Namespaces in XML, not of the prefix `xmlns`, or `invalid_argument` is thrown.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the name of the element, with its prefix |

## Complexity

Linear in the length of `name`, which is checked.

## Exceptions

`invalid_argument` when `name` is not a qualified name or is of the prefix `xmlns`.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::xml::builder table("table");
    println(table.build().to_string());
    try {
        encoding::xml::builder bad("<table>");
    } catch (const invalid_argument& e) {
        println(e.what());
    }
}
```

Output:

```text
<table/>
sgcl::encoding::xml: '<table>' is not a qualified name
```

## See also

- [build](build.md): the element made
- [sgcl::encoding::xml::builder](../xml-builder.md)
