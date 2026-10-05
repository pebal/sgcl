[sgcl](../../README.md) › [encoding](../README.md) › [csv](../csv/README.md)

# sgcl::encoding::csv::row

```cpp
#include "sgcl/encoding/csv.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    class csv {
    public:
        class row;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::encoding::csv::row` is a record a [reader](../csv-reader/README.md) read: its fields as text, the line and the column
where each starts, and the header's names when the reader read one, so that a field is asked by its column's name as
well as by its index. Where Go's `Read` returns a `[]string` and keeps the places in the reader (`FieldPos`), a row
carries both, and is a value of its own: kept, copied, passed on after the reader has gone on.

A field is a [slice](../../core/slice/README.md)`<const char>` of the row's own text, valid on its own as long as it is
held; [get](get.md) gives one as a [string](../../core/string/README.md) of its own.

## Rules

- **A row is a string of its fields and their places**: one managed string holds the fields one after another and,
  after them, the end, the line and the column of each; the header is shared by the rows of a reader. One
  allocation a record, whatever the count of fields.
- A row never changes, and its copies share its text.
- A default-constructed row is empty, with no header.

### From code written for Go

| With Go | With sgcl::encoding |
|---|---|
| `r.FieldPos(i)`, `r.InputOffset()` | [row.position(i)](position.md), [row.line()](line.md): the place travels with the row; the column in code points |
| `ReuseRecord` | none: a row is its own and may be kept |

## Member types

| Type | Definition |
|---|---|
| `iterator` | a forward iterator over the fields, its `value_type` `slice<const char>` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](csv-row.md) | constructs an empty row |

#### Element access

| Function | Description |
|---|---|
| [at](at.md) | a field by its index, checked |
| [operator\[\]](operator_at.md) | a field by its index or by the header's name |
| [get](get.md) | the field of a column as a string, or a fallback |

#### Iterators

| Function | Description |
|---|---|
| [begin](begin.md) | an iterator to the first field |
| [end](end.md) | an iterator past the last field |

#### Capacity

| Function | Description |
|---|---|
| [empty](empty.md) | checks whether the row has no fields |
| [size](size.md) | the number of fields |

#### Observers

| Function | Description |
|---|---|
| [line](line.md) | the line the record starts on |
| [position](position.md) | the line and the column where a field starts |

## Complexity

Every member is constant, but `operator[]` and `get` by a name, a lookup in the header's hash table.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::csv::reader r("city,country\n"
                            "\"Zürich\",Switzerland\n"
                            "Kraków,Poland\n");
    r.read_header();
    vector<encoding::csv::row> kept;
    while (auto row = r.next()) {
        kept.push_back(*row);
    }
    for (const auto& row : kept) {
        auto [line, column] = row.position(1);
        println("{} in {}, {} at {}:{}", row[0], row["country"].value(), row.size(), line, column);
    }
}
```

Output:

```text
Zürich in Switzerland, 2 at 2:10
Kraków in Poland, 2 at 3:8
```

## See also

- [reader::next](../csv-reader/next.md): the next row
- [writer::write](../csv-writer/write.md): a row written as it was read
- [sgcl::encoding::csv](../csv/README.md)
