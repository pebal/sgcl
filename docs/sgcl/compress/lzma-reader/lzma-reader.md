[sgcl](../../README.md) › [compress](../README.md) › [lzma](../lzma.md) › [reader](../lzma-reader.md)

# sgcl::compress::lzma::reader::reader

```cpp
/*(1)*/ explicit reader(const io::reader& in) noexcept;
/*(2)*/ reader(const io::reader& in, const limits& l) noexcept;
```

Constructs a reader of what the data read from `in` decompresses to. Nothing is read yet: the first read reads the
header and takes the dictionary it asks for.

1. With the default [limits](../limits.md): a dictionary of up to 1 GiB.
2. With the limits given: their `max_memory` bounds the dictionary; `max_size` is not read (a stream has no bound on its
   output).

## Parameters

| Parameter | Description |
|---|---|
| `in` | the reader the compressed bytes come from |
| `l` | `max_memory`, the bound on the dictionary ([limits](../limits.md)) |

## Complexity

Constant; the input buffer is allocated.

## Exceptions

None.

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffer packed;
    compress::lzma::writer w(packed, {.level = 9});  // a dictionary of 64 MiB, the size not known
    w.write("hello");
    (void)w.close();

    compress::lzma::reader small{io::buffer(packed.data()), {.max_memory = 1 << 20}};
    println("{}", small.read_all_text().has_value());
    println("{}", small.last_error()->message());
    compress::lzma::reader r{io::buffer(packed.data())};
    println("{}", r.read_all_text().value_or(string()));
}
```

Output:

```text
false
offset 0: lzma: the dictionary needs more memory than the limit allows
hello
```

## See also

- [read](read.md)
- [sgcl::compress::lzma::reader](../lzma-reader.md)
