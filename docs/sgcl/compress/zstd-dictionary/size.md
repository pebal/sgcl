[sgcl](../../README.md) › [compress](../README.md) › [zstd](../zstd/README.md) › [dictionary](README.md)

# sgcl::compress::zstd::dictionary::size

```cpp
size_t size() const noexcept;
```

Returns the number of bytes of the dictionary's content: all of raw content, and what follows the tables in a
dictionary of zstd's format. 0 for none.

## Parameters

None.

## Return value

The content's size.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    compress::zstd::dictionary raw(slice<const byte>(string("raw content")));
    println("{} {}", raw.size(), compress::zstd::dictionary().size());
}
```

Output:

```text
11 0
```

## See also

- [empty](empty.md)
- [sgcl::compress::zstd::dictionary](README.md)
