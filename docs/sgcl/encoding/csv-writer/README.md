[sgcl](../../README.md) › [encoding](../README.md) › [csv](../csv/README.md)

# sgcl::encoding::csv::writer

```cpp
#include "sgcl/encoding/csv.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    class csv {
    public:
        class writer;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::encoding::csv::writer` writes CSV into an [io::writer](../../io/writer/README.md) a record at a time, as Go's
`csv.Writer` writes it: [write](write.md) takes a record's fields — a list of texts, any range of texts,
a [row](../csv-row/README.md) as it was read, or a value of a type of the program with its fields as the columns — and quotes
the fields that must be quoted; [flush](flush.md) hands the text gathered to the stream.

## Rules

- **Written as Go writes it.** A field is quoted when it holds the separator, a quote, `'\r'` or `'\n'`, when it
  starts with a space (Go's `unicode.IsSpace`), or when it is `\.` (Postgres' end of data); a quote is doubled; a
  record ends with `'\n'`, or `"\r\n"` after [use_crlf()](use_crlf.md), which also writes a `'\n'`
  inside quotes as `"\r\n"`.
- **A record of one empty field is written `""`**, where Go writes an empty line, which every reader passes over;
  a record whose first field starts with the [options](../csv-options.md)' comment character is quoted, where Go has
  no comment character to write by, so that a reader of the same options does not pass it over as a comment. These
  are the places the writer parts from Go's. A record of no fields is an empty line, which a reader passes over.
- **The writer gathers the text and [flush()](flush.md) hands it to the stream**: nothing reaches the
  stream before, and a long stream of records is flushed in the loop. The first mistake — a field of a type with no
  text in CSV — is kept, the writes after it write nothing, and `flush()` reports it, as it reports a failed write
  of the stream.
- **`flush()` writes on the thread that calls it**; a task uses `co_await w.async_flush()`. `write` never reaches
  the stream and never waits.
- A writer is one thread's at a time, and it is neither copied nor moved. It holds the stream and never closes it.

### From code written for Go

| With Go | With sgcl::encoding |
|---|---|
| `csv.NewWriter(w)` | [csv::writer(out)](csv-writer.md) |
| `Write`, `WriteAll` | [write](write.md), chained or in a loop: the same text |
| `Flush`, `Error` | [flush()](flush.md), which returns the error |
| `UseCRLF` | [use_crlf()](use_crlf.md) |
| `Comma` | [options](../csv-options.md)`::separator` |
| — | [write(record)](write.md) of a type of the program, the header of its field names first, which Go has outside its standard library (gocsv) |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](csv-writer.md) | a writer into a stream, with the options |
| `(destructor)` | drops the writer and the text not flushed |

#### Modifiers

| Function | Description |
|---|---|
| [use_crlf](use_crlf.md) | ends the records with `"\r\n"` |
| [write](write.md) | a record |
| [flush, async_flush](flush.md) | the text gathered into the stream |

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffer out;
    encoding::csv::writer w(out);
    w.write({"name", "quote"});
    w.write({"Ann", "say \"hi\", then go"});
    w.write({"Bob", " leading space"});
    w.write({""});
    println("{}", out.text().size());
    w.flush().value();
    print("{}", out.text());
}
```

Output:

```text
0
name,quote
Ann,"say ""hi"", then go"
Bob," leading space"
""
```

## See also

- [reader](../csv-reader/README.md): records from a text or a stream
- [csv::save](../csv/save.md): values into a file in one line
- [options](../csv-options.md): the separator
- [sgcl::encoding::csv](../csv/README.md)
