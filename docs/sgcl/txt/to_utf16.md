[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::to_utf16

```cpp
#include "sgcl/txt/encoding.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    vector<char16_t> to_utf16(const string& text) noexcept;
}
```

Returns the text `text` as UTF-16 units in the byte order of the machine, for the edge of a system call that takes them: a code point outside the Basic Multilingual Plane is a surrogate pair. An invalid sequence of UTF-8 is a `U+FFFD`.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text, UTF-8 |

## Return value

The units, with no terminating zero.

## Complexity

Linear in the length of the text: sized once, as no code point takes more units than its bytes.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    println("{::#06x}", txt::to_utf16("a😀"));
}
```

Output:

```text
[0x0061, 0xd83d, 0xde00]
```

## See also

- [from_utf16](from_utf16.md): the way back
- [to_utf32](to_utf32.md)
- [sgcl::txt](README.md)
