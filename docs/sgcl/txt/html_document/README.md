[sgcl](../../README.md) › [txt](../README.md)

# sgcl::txt::html_document

```cpp
#include "sgcl/txt/html.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    class html_document;
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::txt::html_document` is a parsed HTML document: the WHATWG HTML parsing algorithm over UTF-8 text, as a
browser reads it, and the tree it builds — immutable [html_node](../html_node/README.md)s. Parsing never fails.
gumbo-parser is the oracle of its tests; where gumbo lags the standard (it leaves adjacent text nodes apart, does
not know `<search>`, replaces the C1 controls), the standard is followed.

## Rules

- A handle of one word to an immutable document: copies share it, any number of threads read it.
- UTF-8 only: no encoding is sniffed from a `<meta charset>`, which is read as an element like any other.
- No script runs: the scripting flag only decides whether `<noscript>` holds text or markup.
- A document nests as deep as its text says; reading, walking, serializing and sanitizing it use no recursion.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](html_document.md) | constructs no document |
| [parse](parse.md) | reads a document (static) |
| [parse_fragment](parse_fragment.md) | reads a fragment in the context of an element (static) |

#### Observers

| Function | Description |
|---|---|
| [root](root.md) | the document node |
| [html](html.md) | the `html` element |
| [head](head.md) | the `head` element |
| [body](body.md) | the `body` element |
| [title](title.md) | the title's text |
| [errors](errors.md) | the parse errors, when kept |
| [quirks](quirks.md) | the quirks mode of the DOCTYPE |
| [to_string](to_string.md) | the document serialized |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    auto doc = txt::html_document::parse(
        "<!DOCTYPE html><title>News</title><p id=lead>Hello "
        "<a href=\"/a\">first</a> and <a href=\"https://x.org/b\">second</a>. "
        "<template><b>later</b></template><svg viewbox=\"0 0 9 9\"><circle r=4></svg>");
    println("{}", doc.title());
    for (const auto &a : doc.root().elements("a")) {
        println("{} -> {}", a.text(), *a.attribute("href"));
    }
}
```

Output:

```text
News
first -> /a
second -> https://x.org/b
```

## See also

- [html_node](../html_node/README.md)
- [sanitize_html](../sanitize_html.md)
- [html_options](../html_options.md)
- [Benchmarks](../benchmarks.md)
- [sgcl::txt](../README.md)
