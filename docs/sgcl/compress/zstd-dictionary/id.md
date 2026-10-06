[sgcl](../../README.md) › [compress](../README.md) › [zstd](../zstd/README.md) › [dictionary](README.md)

# sgcl::compress::zstd::dictionary::id

```cpp
uint32_t id() const noexcept;
```

Returns the id the dictionary names itself by: the one in a dictionary of zstd's format, which frames made with it
carry in their header ([dictionary_id](../zstd/dictionary_id.md)); 0 for raw content and for none.

## Parameters

None.

## Return value

The id, or 0.

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
    println("{} {}", raw.id(), compress::zstd::dictionary().id());
}
```

Output:

```text
0 0
```

## See also

- [dictionary_id](../zstd/dictionary_id.md): the id a frame names
- [sgcl::compress::zstd::dictionary](README.md)
