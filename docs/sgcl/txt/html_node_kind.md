[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::html_node_kind

```cpp
#include "sgcl/txt/html.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    enum class html_node_kind : uint8_t {
        document,
        doctype,
        element,
        text,
        comment,
        fragment,
    };
}
```

What an [html_node](html_node/README.md) is.

| Value | Description |
|---|---|
| `document` | the document node, the root of a parsed document |
| `doctype` | the DOCTYPE |
| `element` | an element of HTML, SVG or MathML |
| `text` | a run of text |
| `comment` | a comment |
| `fragment` | a template's contents |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    auto doc = txt::html_document::parse("<!DOCTYPE html><!--c--><template>t</template>");
    println("{} {} {}", int(doc.root()[0].kind()), int(doc.root()[1].kind()),
            int(doc.head()[0].content().kind()));
}
```

Output:

```text
1 4 5
```

## See also

- [html_node](html_node/README.md)
- [sgcl::txt](README.md)
