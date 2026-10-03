[sgcl](../../README.md) › [io](../README.md) › [mapping](../mapping.md)

# sgcl::io::mapping::data

```cpp
slice<const byte> data() const noexcept;
```

Returns the mapped bytes, read only: the range of the file the mapping was made of, from the byte `offset` named.
The slice holds the region as its owner, so a slice kept after the last handle is gone still reads the mapping,
which is unmapped when nothing holds it any more.

## Parameters

None.

## Return value

The mapped bytes; an empty slice for an empty mapping and once the mapping is [closed](close.md).

## Complexity

Constant.

## Exceptions

None.

## Notes

A slice taken before [close](close.md) stays valid after it and reads zeros: the range is replaced by zero pages,
never left a hole.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    (void)io::write_file("record.txt", string("header:payload:trailer"));
    io::mapping m = io::map("record.txt");
    slice<const byte> bytes = m.data();
    println("{} {}", bytes.size(), string(bytes.subslice(7, 7)));
}
```

Output:

```text
22 payload
```

## See also

- [writable_data](writable_data.md): the same bytes to write into
- [size](size.md): the length of the range
- [slice](../../core/slice.md): what it returns
- [sgcl::io::mapping](../mapping.md)
