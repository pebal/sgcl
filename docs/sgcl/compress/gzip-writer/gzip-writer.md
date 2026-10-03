[sgcl](../../README.md) › [compress](../README.md) › [gzip](../gzip.md) › [writer](../gzip-writer.md)

# sgcl::compress::gzip::writer::writer

```cpp
explicit writer(const io::writer& out) noexcept;             // (1)
writer(const io::writer& out, const options& o) noexcept;    // (2)
```

Constructs a writer that compresses what is written to it into `out`. Nothing is written to `out` yet: the header goes
before the first bytes.

1. The default options: level 6, an empty header.
2. The options given: the level and the header.

The encoder's memory (its window and tables) is taken here, and kept across a [reset](reset.md).

## Parameters

| Parameter | Description |
|---|---|
| `out` | the writer the compressed bytes go to: any [io writer](../../io/writer.md), a file, a buffer, a socket |
| `o` | the level and the [header](../gzip_header.md) ([options](../gzip-options.md)) |

## Complexity

Constant; the encoder's tables are allocated.

## Exceptions

None.

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    compress::gzip::header h;
    h.name = "report.csv";
    h.comment = "made by an example";

    io::buffer sink;
    compress::gzip::writer w(sink, {.level = compress::level::smallest, .header = h});
    w.write("day,visits\nmonday,12\n");
    (void)w.close();

    compress::gzip::reader r(sink);
    println("{}: {}", r.header()->name, r.header()->comment);
    print("{}", r.read_all_text().value_or(string()));
}
```

Output:

```text
report.csv: made by an example
day,visits
monday,12
```

## See also

- [close](close.md): the end of the stream
- [sgcl::compress::gzip::writer](../gzip-writer.md)
