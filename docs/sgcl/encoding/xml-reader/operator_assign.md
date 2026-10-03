[sgcl](../../README.md) › [encoding](../README.md) › [xml](../xml.md) › [reader](../xml-reader.md)

# sgcl::encoding::xml::reader::operator=

```cpp
reader& operator=(reader&& other) = default;    // (1)
reader& operator=(const reader&) = delete;      // (2)
```

1. Takes the reading of `other` over, where it is; what this reader read before is dropped, and `other` is left to
   be destroyed or assigned to.
2. A reader is not copied: two readers would take turns at one stream.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the reader taken over |

## Return value

`*this`.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::xml::reader r("<a/>");
    for (string doc : {"<b>1</b>", "<c>2</c>"}) {
        r = encoding::xml::reader(doc);
        println(r.read()->to_string());
    }
}
```

Output:

```text
<b>1</b>
<c>2</c>
```

## See also

- [(constructor)](xml-reader.md)
- [sgcl::encoding::xml::reader](../xml-reader.md)
