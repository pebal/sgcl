[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::name_of

```cpp
#include "sgcl/txt/encoding.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    constexpr const char* name_of(encoding e) noexcept;
}
```

Returns the name a header would use for the encoding `e`, the Encoding Standard's preferred one in lower case: `"utf-8"`, `"iso-8859-2"`, `"windows-1250"`, `"us-ascii"`.

## Parameters

| Parameter | Description |
|---|---|
| `e` | the encoding |

## Return value

The name, a C string of static storage.

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
    println("{} {} {}", txt::name_of(txt::encoding::utf8), txt::name_of(txt::encoding::koi8_r),
            txt::name_of(txt::encoding::x_mac_cyrillic));
}
```

Output:

```text
utf-8 koi8-r x-mac-cyrillic
```

## See also

- [encoding_from_name](encoding_from_name.md): the way back
- [encoding](encoding.md)
- [sgcl::txt](README.md)
