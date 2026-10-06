[sgcl](../../README.md) › [compress](../README.md) › [zstd](README.md)

# sgcl::compress::zstd::dictionary_id

```cpp
static uint32_t dictionary_id(const slice<const byte>& data) noexcept;
```

Returns the id of the dictionary the first frame of `data` names in its header, without decoding anything: which of an
application's dictionaries to read it with. A frame made with a dictionary of zstd's format names its id unless the
options left it out (`write_dictionary_id`); a frame made with raw content or with none names none.

## Parameters

| Parameter | Description |
|---|---|
| `data` | the compressed bytes, from the start of a frame (its header at least) |

## Return value

The dictionary's id, or 0 when the frame names none, when `data` does not start with a zstd frame or when it ends
inside the header.

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
    compress::zstd::dictionary common(slice<const byte>(string("the words most messages share")));
    auto packed = compress::zstd::compress("a message", {.dictionary = common});
    println("{} {}", common.id(), compress::zstd::dictionary_id(packed));
}
```

Output:

```text
0 0
```

## See also

- [dictionary](../zstd-dictionary/README.md), [content_size](content_size.md)
- [sgcl::compress::zstd](README.md)
