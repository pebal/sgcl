[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::html_sanitizer_options

```cpp
#include "sgcl/txt/html.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    struct html_sanitizer_options {
        vector<string> elements;
        vector<string> attributes;
        vector<string> url_schemes;
        bool relative_urls = true;
        bool nofollow = false;
        bool keep_comments = false;
    };
}
```

**Requires [rooted](../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::txt::html_sanitizer_options` is the allowlist of [sanitize_html](sanitize_html.md): what survives.

## Member objects

| Field | Description |
|---|---|
| `elements` | the elements kept; empty: those of text people write — `a abbr acronym address article aside b bdi bdo blockquote br caption cite code col colgroup dd del details dfn div dl dt em figcaption figure footer h1`–`h6 header hr i img ins kbd li mark ol p pre q rp rt ruby s samp section small span strike strong sub summary sup table tbody td tfoot th thead time tr tt u ul var wbr` |
| `attributes` | the attributes kept, `element:name` or `*:name` for any element; empty: `*:title *:lang *:dir a:href img:src img:alt img:width img:height`, the spans of cells and columns, `ol:start ol:type ol:reversed li:value`, the `cite` of quotes and edits, the `datetime` of times and edits, `details:open` |
| `url_schemes` | the schemes a URL attribute may have; empty: `http`, `https`, `mailto` |
| `relative_urls` | URLs without a scheme; `true` by default |
| `nofollow` | `rel="nofollow noopener noreferrer"` on every link with an href; `false` by default |
| `keep_comments` | comments kept (their dashes and angle brackets made spaces); `false` by default |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::html_sanitizer_options o{.elements = {"a", "b"},
                                  .attributes = {"a:href", "*:class"},
                                  .nofollow = true};
    println("{}",
            txt::sanitize_html("<p class=x><b class=y>bold</b> <a href=//x.org>link</a></p>", o));
}
```

Output:

```text
<b class="y">bold</b> <a href="//x.org" rel="nofollow noopener noreferrer">link</a>
```

## See also

- [sanitize_html](sanitize_html.md)
- [sgcl::txt](README.md)
