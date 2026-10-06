[sgcl](../../README.md) › [compress](../README.md) › [snappy](README.md)

# sgcl::compress::snappy::compress

```cpp
static vector<byte> compress(const slice<const byte>& data) noexcept;    // (1)
static vector<byte> compress(const string& text) noexcept;               // (2)
template<class T>
static vector<byte> compress(const T& text) noexcept;                    // (3)
```

Compresses the whole of the data at once in Snappy's framing format: the stream identifier, then a chunk for every
64 KB of data, compressed, or stored as it is where compressing would not save an eighth, each with the masked CRC-32C
of its data. Empty data is the identifier alone. The compressor's table is the thread's, kept from one call to the
next, so a small compress allocates only its result.

1. The bytes of `data`.
2. The bytes of the text.
3. Takes part only for a literal, a character array or a `std::string_view`: the text's bytes, as (2) takes them.

## Parameters

| Parameter | Description |
|---|---|
| `data` | the bytes to compress |
| `text` | the text to compress, its bytes as they are (UTF-8) |

## Return value

The compressed bytes, a new vector.

## Complexity

Linear in the size of the data.

## Exceptions

None.

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    println("{}", compress::snappy::compress(string()).size());
    println("{}", compress::snappy::compress("hello").size());
    println("{}", compress::snappy::compress(string("x").repeat(1000)).size());
}
```

Output:

```text
10
23
70
```

## See also

- [decompress](decompress.md): the other way
- [compress_block](compress_block.md): no framing
- [snappy::writer](../snappy-writer/README.md): a stream
- [sgcl::compress::snappy](README.md)
