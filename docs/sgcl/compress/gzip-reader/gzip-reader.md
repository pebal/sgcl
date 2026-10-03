[sgcl](../../README.md) › [compress](../README.md) › [gzip](../gzip.md) › [reader](../gzip-reader.md)

# sgcl::compress::gzip::reader::reader

```cpp
/*(1)*/ explicit reader(const io::reader& in) noexcept;
/*(2)*/ reader(const io::reader& in, gzip::single_member_t) noexcept;
```

Constructs a reader of what the data read from `in` decompresses to. Nothing is read yet: the first read reads the
header.

1. A reader of every member of the stream, one after another.
2. A reader of the first member alone: it stops after the member's trailer.

The decoder's memory (its window and state) is taken here, and kept across a [reset](reset.md).

## Parameters

| Parameter | Description |
|---|---|
| `in` | the reader the compressed bytes come from |
| `single_member_t` | the tag, `gzip::single_member` |

## Complexity

Constant; the decoder's window is allocated.

## Exceptions

None.

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto two = compress::gzip::compress("first part\n");
    auto second = compress::gzip::compress("second part\n");
    two.insert(two.end(), second.begin(), second.end());  // as `cat a.gz b.gz`

    compress::gzip::reader all{io::buffer(two)};
    print("{}", all.read_all_text().value_or(string()));
    compress::gzip::reader first{io::buffer(two), compress::gzip::single_member};
    print("{}", first.read_all_text().value_or(string()));
}
```

Output:

```text
first part
second part
first part
```

## See also

- [read](read.md)
- [sgcl::compress::gzip::reader](../gzip-reader.md)
