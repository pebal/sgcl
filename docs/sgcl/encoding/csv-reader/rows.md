[sgcl](../../README.md) › [encoding](../README.md) › [csv](../csv/README.md) › [reader](README.md)

# sgcl::encoding::csv::reader::rows

```cpp
generator<row> rows() noexcept;
```

The records to the end of the input, for a range-for: `for (auto row : r.rows())`. Each step is a call of
[next()](next.md), and the range ends where `next()` returns `nullopt`, at the end or at a mistake, which
[last_error()](last_error.md) tells apart after the loop. Go's `ReadAll`, a record at a time, the rows kept only
as long as the loop keeps them. The reading is on the thread that walks the range; a task reads with
[async_next](next.md).

## Parameters

None.

## Return value

A [generator](../../core/generator/README.md) of the records; it reads through the reader, which must outlive it.

## Complexity

Constant; walking it, linear in the length of the input.

## Exceptions

None. The reads made as the range is walked throw what [next()](next.md) throws, out of the iterator's look at a
record, which reads it.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::csv::reader r("fruit,kg\napple,3\npear,1\n\"plum\n");
    r.read_header();
    for (auto row : r.rows()) {
        println("{}: {}", row["fruit"].value(), row["kg"].value());
    }
    println("{}", r.last_error()->message());
}
```

Output:

```text
apple: 3
pear: 1
4:7: extraneous or missing " in a quoted field
```

## See also

- [next](next.md): one record
- [sgcl::encoding::csv::reader](README.md)
