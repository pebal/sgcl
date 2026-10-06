[sgcl](../../README.md) › [compress](../README.md) › [zstd](README.md)

# sgcl::compress::zstd::content_size

```cpp
static optional<uint64_t> content_size(const slice<const byte>& data) noexcept;
```

Returns the size of the content the first frame of `data` holds, as its header gives it, without decoding anything:
the size to make room for before a [decompress](decompress.md) into memory of one's own, or to refuse data too large
before reading it. [compress](compress.md) writes the size; a [writer](../zstd-writer/README.md), which cannot know
it, does not. Only the header is read: the size is what the frame claims, which `decompress` checks.

## Parameters

| Parameter | Description |
|---|---|
| `data` | the compressed bytes, from the start of a frame (its header at least) |

## Return value

The content's size, or no value when the header does not give it, when `data` does not start with a zstd frame or
when it ends inside the header.

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
    auto packed = compress::zstd::compress(string("x").repeat(5000));
    println("{}", compress::zstd::content_size(packed).value_or(0));

    io::buffer sink;
    compress::zstd::writer w(sink);
    w.write("streamed");
    (void)w.close();
    println("{}", compress::zstd::content_size(sink.data()).has_value());
}
```

Output:

```text
5000
false
```

## See also

- [dictionary_id](dictionary_id.md): the other field of the header
- [sgcl::compress::zstd](README.md)
