[sgcl](../../README.md) › [encoding](../README.md) › [xml](../xml/README.md) › [builder](README.md)

# sgcl::encoding::xml::builder::build

```cpp
xml build() noexcept;
```

The element of the name, the attributes and the children given so far, made once. The namespace of the element and
of each attribute is found now, from what the element itself tells: its `xmlns` declarations and its own prefix. The builder is left
empty, with the same name, and the next `build()` gives another element of that name.

## Parameters

None.

## Return value

The element.

## Complexity

Linear in the number of attributes and children.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::xml::builder row("row");
    encoding::xml::builder table("table");
    for (int r : range(2)) {
        for (int c : range(3)) {
            row.push_back(encoding::xml("cell", to_string(r * 3 + c)));
        }
        table.push_back(row.build());
    }
    println(table.build().to_string(encoding::xml::pretty));
    println(row.build().to_string());
}
```

Output:

```text
<table>
  <row>
    <cell>0</cell>
    <cell>1</cell>
    <cell>2</cell>
  </row>
  <row>
    <cell>3</cell>
    <cell>4</cell>
    <cell>5</cell>
  </row>
</table>
<row/>
```

## See also

- [push_back](push_back.md), [set](set.md): what the element is made of
- [sgcl::encoding::xml::builder](README.md)
