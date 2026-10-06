[sgcl](../../README.md) › [txt](../README.md) › [markdown_node](README.md)

# sgcl::txt::markdown_node::operator[]

```cpp
markdown_node operator[](size_t i) const noexcept;
```

Returns a child by its index.

## Parameters

| Parameter | Description |
|---|---|
| `i` | the index, from 0 |

## Return value

The child; no node past the last.

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
    txt::markdown_node para = doc.root()[1];
    for (size_t k = 0; k < para.size(); ++k) {
        println("{} [{}]", int(para[k].kind()), para[k].text());
    }
}
```

Output:

```text
12 [Some ]
17 [text]
12 [ and ]
15 [code]
12 [, a ]
20 [link]
12 [.]
```

## See also

- [size](size.md)
- [sgcl::txt::markdown_node](README.md)
