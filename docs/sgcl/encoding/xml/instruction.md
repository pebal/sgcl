[sgcl](../../README.md) › [encoding](../README.md) › [xml](../xml.md)

# sgcl::encoding::xml::instruction

```cpp
static xml instruction(const string& target, const string& data = {});
```

A processing instruction, `<?target data?>` (`<?target?>` without data). The target is a name without a colon;
`xml` in any case is reserved for the XML declaration, which is not a node here ([style](../xml-style.md)
`::declaration` writes it). Data holding `?>`, which would end the instruction, is refused, and so is data
starting with white space, which a reader takes for the separator after the target and drops. Each of these is a
mistake of the program, `invalid_argument`.

[name](name.md) and [local_name](local_name.md) of an instruction are its target, [text](text.md) its data.

## Parameters

| Parameter | Description |
|---|---|
| `target` | the target of the instruction |
| `data` | what follows the target |

## Return value

The instruction, of the kind `instruction`.

## Complexity

Linear in the length of `target` and `data`, which are checked.

## Exceptions

`invalid_argument` when `target` is not a name without a colon or is `xml` in any case, or when `data` holds `?>` or
starts with white space.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::xml style = encoding::xml::instruction("xml-stylesheet", R"(href="a.css")");
    println("{} | {} | {}", style.to_string(), style.name(), style.text());
    println(encoding::xml::instruction("break").to_string());
    try {
        encoding::xml::instruction("XML");
    } catch (const invalid_argument& e) {
        println(e.what());
    }
}
```

Output:

```text
<?xml-stylesheet href="a.css"?> | xml-stylesheet | href="a.css"
<?break?>
sgcl::encoding::xml: 'XML' cannot be the target of an instruction
```

## See also

- [text_node](text_node.md), [comment](comment.md): the other nodes of an element's content
- [style](../xml-style.md): the XML declaration in front of a node written
- [sgcl::encoding::xml](../xml.md)
