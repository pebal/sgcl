[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::html_namespace

```cpp
#include "sgcl/txt/html.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    enum class html_namespace : uint8_t {
        html,
        svg,
        mathml,
    };
}
```

The namespace of an element of a parsed document: an `<svg>` and what it holds are SVG, a `<math>` MathML, until an
integration point (`foreignObject`, `annotation-xml` of HTML) returns to HTML.

| Value | Description |
|---|---|
| `html` | an HTML element |
| `svg` | an SVG element |
| `mathml` | a MathML element |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    auto doc = txt::html_document::parse(
        "<svg><foreignObject><p>x</p></foreignObject></svg><math><mi>y</mi></math>");
    for (const auto &e : doc.body().elements()) {
        println("{} {}", e.name(), int(e.ns()));
    }
}
```

Output:

```text
svg 1
foreignObject 1
p 0
math 2
mi 2
```

## See also

- [html_node](html_node/README.md)
- [sgcl::txt](README.md)
