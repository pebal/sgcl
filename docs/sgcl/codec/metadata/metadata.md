[sgcl](../../README.md) › [codec](../README.md) › [metadata](README.md)

# sgcl::codec::metadata::metadata

```cpp
metadata();
```

Makes metadata of nothing: every field `nullopt`, the orientation 1, both blocks empty. What
[read](read.md) of a file without metadata gives too.

## Parameters

None.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    codec::metadata none;
    println("{} {} {}", none.make().has_value(), none.orientation(), none.exif().size());
}
```

Output:

```text
false 1 0
```

## See also

- [read](read.md), [load](load.md), [from_exif](from_exif.md): metadata of a file
- [sgcl::codec::metadata](README.md)
