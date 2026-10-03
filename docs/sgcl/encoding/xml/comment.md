[sgcl](../../README.md) › [encoding](../README.md) › [xml](README.md)

# sgcl::encoding::xml::comment

```cpp
static xml comment(const string& text);
```

A comment, `<!--text-->`. A comment cannot hold `--` or end with `-`, which would end it or make it ill formed: such
a `text` is a mistake of the program, `invalid_argument`. A character XML cannot hold is written as U+FFFD.

A tree read by [parse](parse.md) holds the document's comments only with
[options::keep_comments](../xml-options.md).

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text of the comment, between `<!--` and `-->` |

## Return value

The comment, of the kind `comment`.

## Complexity

Linear in the length of `text`, which is checked.

## Exceptions

`invalid_argument` when `text` holds `--` or ends with `-`.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::xml c = encoding::xml::comment(" generated ");
    println("{} [{}]", c.to_string(), c.text());
    println(encoding::xml("config").push_back(c).to_string());
    try {
        encoding::xml::comment("a -- b");
    } catch (const invalid_argument& e) {
        println(e.what());
    }
}
```

Output:

```text
<!-- generated --> [ generated ]
<config><!-- generated --></config>
sgcl::encoding::xml: a comment cannot hold "--" or end with '-'
```

## See also

- [text_node](text_node.md), [instruction](instruction.md): the other nodes of an element's content
- [options](../xml-options.md): `keep_comments`
- [sgcl::encoding::xml](README.md)
