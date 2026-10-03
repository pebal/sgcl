[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::encoding_from_name

```cpp
#include "sgcl/txt/encoding.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    optional<encoding> encoding_from_name(const string& name) noexcept;
}
```

Returns the encoding a name stands for, reading the name as a header writes it: in any case, with the dashes, the
underscores and the spaces or without them, under the aliases IANA lists and the ones that turn up in the wild
(`utf8`, `ISO_8859-2`, `cp1250`, `iso8859_2`, `latin2`). A name nobody knows is `nullopt` — an ordinary answer, not a
failure, so no `expected`; the multi byte encodings this module does not have (Shift_JIS, EUC-JP, GB18030, Big5,
EUC-KR) are among them.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the name, as a `charset` parameter or an XML declaration gives it |

## Return value

The encoding, or `nullopt` when the name is not one of the set.

## Complexity

Linear in the number of aliases.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    for (auto name : {"UTF8", "ISO_8859-2", "cp1250", "latin2", "utf16", "Shift_JIS"}) {
        auto e = txt::encoding_from_name(name);
        println("{} -> {}", name, e ? txt::name_of(*e) : "unknown");
    }
}
```

Output:

```text
UTF8 -> utf-8
ISO_8859-2 -> iso-8859-2
cp1250 -> windows-1250
latin2 -> iso-8859-2
utf16 -> utf-16le
Shift_JIS -> unknown
```

## See also

- [name_of](name_of.md): the way back
- [decode](decode.md), [encode](encode.md): bytes and text by an encoding's name, in one call
- [encoding](encoding.md)
- [sgcl::txt](README.md)
