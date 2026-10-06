[sgcl](../../README.md) › [codec](../README.md) › [qr](README.md)

# sgcl::codec::qr::to_svg

```cpp
string to_svg(uint32_t border = 4) const;
```

The symbol as an SVG document: a light square of `size() + 2 · border` units and one path of the dark modules,
each row's run of them a rectangle, one unit a module, `shape-rendering="crispEdges"` so that a viewer draws no
seams. It scales to any size without blur.

## Parameters

| Parameter | Description |
|---|---|
| `border` | the quiet zone in modules; 4 unless told |

## Return value

The document, ending with a line feed.

## Complexity

Linear in the modules of the symbol.

## Exceptions

None.

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string svg = codec::qr::encode("SGCL")->to_svg();
    println("{}", svg.substr(0, svg.find('>') + 1));  // the root element's tag
    println("{} bytes", svg.size());
}
```

Output:

```text
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 29 29" shape-rendering="crispEdges">
1702 bytes
```

## See also

- [to_image](to_image.md): the symbol as an image
- [sgcl::codec::qr](README.md)
