[sgcl](../../README.md) › [io](../README.md) › [shared_memory](README.md)

# sgcl::io::shared_memory::data

```cpp
slice<byte> data() const noexcept;
```

Returns the region's bytes, to read and to write: the region is always both. What one process writes there another
that mapped the object reads at once, with no ordering beyond what the processes make themselves (an `std::atomic`
in the region, stored with release and loaded with acquire). The slice holds the region as its owner, so a slice
kept after the last handle is gone still reads the region, which is unmapped when nothing holds it any more.

## Parameters

None.

## Return value

The region's bytes; an empty slice once the region is [closed](close.md).

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
    (void)io::shared_memory::remove("sgcl-example-data");
    slice<byte> bytes;
    {
        io::shared_memory region = io::shared_memory::create("sgcl-example-data", 64);
        bytes = region.data();
    }
    bytes[0] = byte('s');  // the slice holds the region
    io::shared_memory again = io::shared_memory::open("sgcl-example-data");
    println("{}", char(again.data()[0]));
    (void)io::shared_memory::remove("sgcl-example-data");
}
```

Output:

```text
s
```

## See also

- [size](size.md): the length of the region
- [slice](../../core/slice/README.md): what it returns
- [sgcl::io::shared_memory](README.md)
