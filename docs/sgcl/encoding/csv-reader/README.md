[sgcl](../../README.md) › [encoding](../README.md) › [csv](../csv/README.md)

# sgcl::encoding::csv::reader

```cpp
#include "sgcl/encoding/csv.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    class csv {
    public:
        class reader;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::encoding::csv::reader` reads CSV a record at a time, from a [string](../../core/string/README.md) or from an
[io::reader](../../io/reader/README.md), as Go's `csv.Reader` reads it: [next](next.md) gives the next
[row](../csv-row/README.md), [rows](rows.md) all of them in a range-for, and [read\<T\>](read.md) the
next record as a value of a type of the program, its fields found by the names of the header
([read_header](read_header.md)). A text is read where it is; a stream is read a block at a time, and a
record is handed out as soon as its line has ended.

## Rules

- **The first mistake stops the reader**: [next()](next.md) returns `nullopt`, as it does at the end,
  and [last_error()](last_error.md) says which — a quote in a field without quotes (`syntax`), a
  character after a closing quote (`syntax`), a quoted field not closed (`unexpected_end`), a record of another
  length than the first (`field_count`; Go gives that record with the error, the reader here stops), a record of a
  stream past [options](../csv-options.md)`::max_record_size` (`out_of_range`), a failed read of the stream (`io`) —
  with the line and the column. Where Go names a place, it is Go's place. Every call after it returns `nullopt`.
- **A record cut by the end of a block goes on where it stopped**: its fields gathered so far are kept and no byte
  is read twice, so a field of a megabyte trickling in a byte at a time costs what the megabyte costs.
- **Every method that may reach into the stream does it on the thread that calls it**; a task uses the `async_`
  forms, `co_await r.async_next()`, which wait for the stream without holding the worker.
- A reader is one thread's at a time, and it is neither copied nor moved: it holds the stream, the record being read and
  the header.
- A reader of a text keeps the text alive while it reads; the rows it returns are their own and outlive it.

### From code written for Go

| With Go | With sgcl::encoding |
|---|---|
| `csv.NewReader(r)` | [csv::reader(in)](csv-reader.md), or `csv::reader(text)` for a text in memory |
| `Read`, `ReadAll` | [next()](next.md), [rows()](rows.md): a record is a [row](../csv-row/README.md), one string of its fields and their places |
| `ParseError`, `ErrBareQuote`, `ErrQuote`, `ErrFieldCount` | [last_error()](last_error.md): `errc::syntax`, `unexpected_end`, `field_count`, with the line and the column; a record of another length stops the reader |
| — | [read_header()](read_header.md), [read\<T\>()](read.md): the header and records as types, which Go has outside its standard library (gocsv) |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](csv-reader.md) | a reader of a text or a stream, with the options |
| `(destructor)` | drops the reader; the rows it returned stay valid |

#### Reading

| Function | Description |
|---|---|
| [next, async_next](next.md) | the next record |
| [read_header, async_read_header](read_header.md) | the next record, taken as the names of the columns |
| [read, async_read](read.md) | the next record as a value of a type |
| [rows](rows.md) | the records to the end, in a range-for |

#### Observers

| Function | Description |
|---|---|
| [header](header.md) | the names of the columns |
| [last_error](last_error.md) | the mistake the reader stopped at |
| [line](line.md) | the line the reader has reached |

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<int> count_records(io::reader in) {
    encoding::csv::reader r(in);
    int count = 0;
    while (co_await r.async_next()) {
        ++count;
    }
    co_return r.last_error() ? -1 : count;
}

int main() {
    io::buffer good("a,b\nc,d\ne,f\n");
    io::buffer bad("a,b\nc,d,e\n");
    println("{}", async::run(count_records(good)));
    println("{}", async::run(count_records(bad)));
}
```

Output:

```text
3
-1
```

## See also

- [row](../csv-row/README.md): a record read
- [writer](../csv-writer/README.md): records into a stream
- [options](../csv-options.md): the separator and Go's other settings
- [sgcl::encoding::csv](../csv/README.md)
