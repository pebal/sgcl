[sgcl](../../README.md) › [encoding](../README.md) › [xml](../xml/README.md) › [writer](README.md)

# sgcl::encoding::xml::writer::node

```cpp
writer& node(const xml& n) noexcept;
```

Writes a node of a [tree](../xml/README.md) whole, where the writer is: an element with everything inside it, a text, a
comment or an instruction, as [to_string](../xml/to_string.md) writes it, indented in the writer's style. `xml()`
writes nothing. The tree is walked with a loop of its own, not recursion. Go's `Encoder.Encode` of a value is
[value](value.md).

## Parameters

| Parameter | Description |
|---|---|
| `n` | the node |

## Return value

`*this`.

## Complexity

Linear in the size of the subtree.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto item = encoding::xml::parse("<item><name>Tea</name></item>").value();
    encoding::xml::writer w(io::stdout, encoding::xml::pretty);
    w.start("order").node(item).node(item.set("count", "2")).end();
    w.flush().value();
    println();
}
```

Output:

```text
<order>
  <item>
    <name>Tea</name>
  </item>
  <item count="2">
    <name>Tea</name>
  </item>
</order>
```

## See also

- [value](value.md): a value of a program's type as an element
- [xml::to_string](../xml/to_string.md): a tree written without a stream
- [sgcl::encoding::xml::writer](README.md)
