[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::html_attribute

```cpp
#include "sgcl/txt/html.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    struct html_attribute {
        string name;
        string value;
    };
}
```

**Requires [rooted](../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::txt::html_attribute` is an attribute of an element of a parsed document.

## Member objects

| Field | Description |
|---|---|
| `name` | the name as the tokenizer lowercased it; an SVG or MathML one adjusted (`viewBox`, `definitionURL`), a foreign prefix kept (`xlink:href`) |
| `value` | the value, its character references read |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    auto doc =
        txt::html_document::parse("<svg ViewBox='0 0 1 1' XLINK:HREF=#a><a TITLE='x &amp; y'>");
    for (const auto &e : doc.body().elements()) {
        for (const auto &a : e.attributes()) {
            println("{}: {}={}", e.name(), a.name, a.value);
        }
    }
}
```

Output:

```text
svg: viewBox=0 0 1 1
svg: xlink:href=#a
a: title=x & y
```

## See also

- [html_node::attributes](html_node/attributes.md)
- [sgcl::txt](README.md)
