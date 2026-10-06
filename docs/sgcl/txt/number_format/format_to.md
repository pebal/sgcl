[sgcl](../../README.md) › [txt](../README.md) › [number_format](README.md)

# sgcl::txt::number_format::format_to

```cpp
template<class T>
size_t format_to(const slice<char>& buffer, T value) const noexcept;
```

Writes a number into memory the caller lends, nothing allocated, as [txt::format_to](../format_to.md) does: what
fits is written and the size of the whole comes back, so a caller may ask with an empty buffer and then size one.

## Parameters

| Parameter | Description |
|---|---|
| `buffer` | where the text goes |
| `value` | the number, of an arithmetic type other than `bool` |

## Return value

The size of the whole text, in bytes.

## Complexity

Linear in the digits written.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::number_format pl(txt::locale("pl"));
    char room[32];
    size_t n = pl.format_to(room, 1234567.5);
    println("{} ({} bytes)", std::string_view(room, n), n);
}
```

Output:

```text
1 234 567,5 (13 bytes)
```

## See also

- [format](format.md)
- [sgcl::txt::number_format](README.md)
