[sgcl](../README.md) › [encoding](README.md) › [xml](xml.md)

# sgcl::encoding::xml::style

```cpp
#include "sgcl/encoding/xml.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    class xml {
    public:
        struct style {
            uint8_t indent = 0;
            bool declaration = false;
        };

        static const style compact;   // {0, false}
        static const style pretty;    // {2, false}
    };
}
```

`sgcl::encoding::xml::style` is how a node is written by [to_string](xml/to_string.md),
[stringify](xml/stringify.md) and a [writer](xml-writer.md): on one line, or indented by `indent` spaces a level,
with or without the XML declaration in front. `xml::compact` and `xml::pretty` are the two constants of `xml` for
the common cases. Go's `MarshalIndent(v, prefix, indent)` takes a prefix too, which has no counterpart here.

## Member objects

| Member | Description |
|---|---|
| `indent` | the spaces of a level; 0 by default: no line is broken. Each element stands on a line of its own, indented a level deeper than its parent, but inside an element that holds text, whose white space is content: no line is broken there from the text on |
| `declaration` | `<?xml version="1.0" encoding="UTF-8"?>` written first; `false` by default |

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto doc = encoding::xml::parse("<menu><item>Tea</item><item>Milk</item><sep/></menu>").value();
    println(doc.to_string());
    println(doc.to_string(encoding::xml::style{4, true}));
}
```

Output:

```text
<menu><item>Tea</item><item>Milk</item><sep/></menu>
<?xml version="1.0" encoding="UTF-8"?>
<menu>
    <item>Tea</item>
    <item>Milk</item>
    <sep/>
</menu>
```

## See also

- [to_string](xml/to_string.md), [stringify](xml/stringify.md), [writer](xml-writer.md): what takes a style
- [sgcl::encoding::xml](xml.md)
