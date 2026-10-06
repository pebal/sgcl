[sgcl](../../README.md) › [txt](../README.md) › [html_document](README.md)

# sgcl::txt::html_document::to_string

```cpp
string to_string() const;
```

Returns the document written by the HTML serialization algorithm (13.3): the DOCTYPE, comments, elements with their
attributes (`&`, a no-break space, `"`, `<` and `>` of a value escaped), void elements without end tags, text
escaped except in the elements whose text is raw (`script`, `style` and their kind).

## Parameters

None.

## Return value

The HTML; for a fragment, of its nodes.

## Complexity

Linear in the size of the document.

## Exceptions

None.

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
    println("{}", doc.to_string());
}
```

Output:

```text
<!DOCTYPE html><html><head><title>News</title></head><body><p id="lead">Hello <a href="/a">first</a> and <a href="https://x.org/b">second</a>. <template><b>later</b></template><svg viewBox="0 0 9 9"><circle r="4"></circle></svg></p></body></html>
```

## See also

- [html_node::outer_html](../html_node/outer_html.md)
- [sgcl::txt::html_document](README.md)
