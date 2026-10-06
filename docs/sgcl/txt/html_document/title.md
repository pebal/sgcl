[sgcl](../../README.md) › [txt](../README.md) › [html_document](README.md)

# sgcl::txt::html_document::title

```cpp
string title() const;
```

Returns the text of the document's first `title` element of HTML, its runs of white space made one space and those
at its ends dropped, as `document.title` reads it.

## Parameters

None.

## Return value

The title; empty without one.

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
    println("[{}]", doc.title());
}
```

Output:

```text
[News]
```

## See also

- [html_node::text](../html_node/text.md)
- [sgcl::txt::html_document](README.md)
