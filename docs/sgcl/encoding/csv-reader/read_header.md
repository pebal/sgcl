[sgcl](../../README.md) › [encoding](../README.md) › [csv](../csv.md) › [reader](../csv-reader.md)

# sgcl::encoding::csv::reader::read_header, async_read_header

```cpp
optional<row> read_header();                                // (1)
async::task<optional<row>> async_read_header() noexcept;    // (2)
```

Reads the next record, as [next](next.md) does, and takes it as the header: its fields are the names of the
columns, which the rows read after it are asked by (`row["age"]`, [row::get](../csv-row/get.md)) and
[read\<T\>](read.md) finds the fields of a type by. A name given twice is found at its first column. Called again,
it takes the next record as a new header. Go has no header; gocsv, outside its standard library, reads one.

1. Reads on the thread that calls it.
2. The same in a task.

## Parameters

None.

## Return value

The header's record, or `nullopt` at the end of the input or at a mistake, when the header stays as it was.

## Complexity

Linear in the length of the record; each name is put into the header's hash table.

## Exceptions

- (1) What the read of the stream under the reader throws; none for a reader of a text.
- (2) None when the task is made: what the read of the stream throws comes out of its `co_await`.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::csv::reader r("name,age\nAnn,31\nBob,27\n");
    auto header = r.read_header();
    println("{} columns", header->size());
    while (auto row = r.next()) {
        println("{} is {}", row->get("name", "?"), row->get("age", "?"));
    }
}
```

Output:

```text
2 columns
Ann is 31
Bob is 27
```

## See also

- [header](header.md): the names read
- [row::operator\[\]](../csv-row/operator_at.md): a field by its column's name
- [sgcl::encoding::csv::reader](../csv-reader.md)
