[sgcl](../../README.md) › [compress](../README.md) › [bzip2](../bzip2.md) › [reader](../bzip2-reader.md)

# sgcl::compress::bzip2::reader::reader

```cpp
explicit reader(const io::reader& in) noexcept;
```

Constructs a reader of what the data read from `in` decompresses to, every stream of it. Nothing is read yet.

## Parameters

| Parameter | Description |
|---|---|
| `in` | the reader the compressed bytes come from |

## Complexity

Constant; the decoder and the input buffer are allocated.

## Exceptions

None.

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto packed = encoding::hex::decode("425a68393141592653591931653d00000081000244a000"
                                        "219a68334d07338bb9229c28480c98b29e80");
    (void)io::write_file("words.bz2", *packed);
    io::reader file = *io::open("words.bz2");
    compress::bzip2::reader r(file);
    println("{}", r.read_all_text().value_or(string()));
    (void)r.close();  // closes the file
    (void)io::remove("words.bz2");
}
```

Output:

```text
hello
```

## See also

- [read](read.md)
- [sgcl::compress::bzip2::reader](../bzip2-reader.md)
