[sgcl](../../README.md) › [txt](../README.md) › [markdown_node](README.md)

# sgcl::txt::markdown_node::url

```cpp
string url() const noexcept;
```

Returns a link's or an image's destination, its escapes and entities read, as written (the HTML percent-encodes it).

## Parameters

None.

## Return value

The destination; empty for other nodes.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    auto doc = txt::markdown_document::parse(
        "# Notes\n\nSome *text* and `code`, a [link](/a \"A\").\n\n"
        "3. three\n4. [x] four\n\n~~~cpp\nint x;\n~~~\n\n| a | b |\n|:-|-:|\n| 1 | 2 |\n");
    println("{}", doc.root()[1][5].url());
}
```

Output:

```text
/a
```

## See also

- [title](title.md)
- [sgcl::txt::markdown_node](README.md)
