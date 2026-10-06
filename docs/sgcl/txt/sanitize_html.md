[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::sanitize_html

```cpp
#include "sgcl/txt/html.h"   // or "sgcl/txt.h"

string sanitize_html(const string& html, const html_sanitizer_options& o = {});
```

Returns HTML that can be put into a page: `html` parsed as a fragment of a `body` (as a browser would read it
there), then written by the serialization algorithm with only what the allowlist keeps. An element not allowed is
unwrapped — its children kept — except those dropped with their content: `script`, `style`, `iframe`, `object`,
`embed`, `template`, `noscript`, `textarea`, `select`, `title`, frames, forms and their controls, media, `svg` and
`math`. An attribute is kept when the list allows it, never an event handler (`on…`) or `style`, and a URL attribute
only with an allowed scheme (after the white space and line breaks a browser strips) or, when allowed, no scheme.

## Parameters

| Parameter | Description |
|---|---|
| `html` | the HTML, untrusted |
| `o` | the [html_sanitizer_options](html_sanitizer_options.md) |

## Return value

The sanitized HTML, well formed: what it says is what a browser reads from it.

## Complexity

Linear in the length of `html`.

## Exceptions

None that the program can cause; running out of memory ends it.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    println("{}",
            txt::sanitize_html(R"html(<p onclick="steal()">Hi <script>alert(1)</script><b>there</b>
<a href="javascript:alert(2)">bad</a> <a href="https://x.org">good</a><img src=x onerror=alert(3)>)html"));
}
```

Output:

```text
<p>Hi <b>there</b>
<a>bad</a> <a href="https://x.org">good</a><img src="x"></p>
```

## See also

- [html_sanitizer_options](html_sanitizer_options.md)
- [html_document::parse_fragment](html_document/parse_fragment.md)
- [html_stencil](html_stencil/README.md)
- [sgcl::txt](README.md)
