[sgcl](../../README.md) › [encoding](../README.md) › [csv](../csv.md) › [reader](../csv-reader.md)

# sgcl::encoding::csv::reader::next, async_next

```cpp
optional<row> next();                                // (1)
async::task<optional<row>> async_next() noexcept;    // (2)
```

The next record, Go's `Read`. Empty lines are skipped, and with [options](../csv-options.md)`::comment` the lines
starting with it; a quoted field may hold the separator, `""` for a quote, and line endings. When the reader read a
header, the row knows its names. At the end of the input, or at a mistake, there is no record: the two are told
apart by [last_error()](last_error.md), which is empty at the end.

1. Reads on the thread that calls it, the stream's reads included.
2. The same in a task: the reads of the stream are `co_await`ed, and the worker does other work meanwhile.

## Parameters

None.

## Return value

The record, or `nullopt` at the end of the input, at a mistake, and at every call after either.

## Complexity

Linear in the length of the record and of the empty lines and comments before it.

## Exceptions

- (1) What the read of the stream under the reader throws; none for a reader of a text.
- (2) None when the task is made: what the read of the stream throws comes out of its `co_await`.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::csv::reader r("id,note\n"
                            "1,\"a quote \"\" and a comma, inside\"\n"
                            "\n"
                            "2,\"two\r\nlines\"\r\n"
                            "3,bare \" quote\n");
    while (auto row = r.next()) {
        println("{} [{}]", row->at(0), row->at(1));
    }
    println("{}", r.last_error()->message());
}
```

Output:

```text
id [note]
1 [a quote " and a comma, inside]
2 [two
lines]
6:8: bare " in a field without quotes
```

## See also

- [rows](rows.md): every record in a range-for
- [read](read.md): the next record as a value of a type
- [last_error](last_error.md): why the reading stopped
- [sgcl::encoding::csv::reader](../csv-reader.md)
