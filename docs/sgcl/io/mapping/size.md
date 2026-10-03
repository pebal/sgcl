[sgcl](../../README.md) › [io](../README.md) › [mapping](README.md)

# sgcl::io::mapping::size

```cpp
size_t size() const noexcept;
```

Returns the length of the mapped range, fixed when the file was mapped: a file that grows afterwards is not seen
past the old end, and a new [map](../map.md) sees the rest.

## Parameters

None.

## Return value

The length of the range in bytes; 0 for an empty mapping and once the mapping is [closed](close.md).

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    (void)io::write_file("grows.txt", string("start"));
    io::mapping m = io::map("grows.txt");
    (void)io::append_file("grows.txt", string(" and more"));
    println("{} {}", m.size(), string(m.data()));
    println("{}", io::map("grows.txt").value().size());
}
```

Output:

```text
5 start
14
```

## See also

- [data](data.md): the bytes
- [map_options](../map_options.md): `offset` and `length`, the range
- [sgcl::io::mapping](README.md)
