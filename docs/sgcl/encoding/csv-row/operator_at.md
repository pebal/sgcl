[sgcl](../../README.md) › [encoding](../README.md) › [csv](../csv.md) › [row](../csv-row.md)

# sgcl::encoding::csv::row::operator[]

```cpp
slice<const char> operator[](size_t index) const noexcept;                         // (1)
optional<slice<const char>> operator[](const string& column) const noexcept;       // (2)
template<size_t N>
optional<slice<const char>> operator[](const char (&column)[N]) const noexcept;    // (3)
```

A field of the record, as a slice of the row's own text.

1. The field at `index`, which must be below [size()](size.md), as a vector's `operator[]` asks: a debug build
   asserts it, a release build does not check. [at](at.md) is the form that checks.
2. The field of the header's column named `column`: the header is the record the reader read with
   [read_header](../csv-reader/read_header.md). `nullopt` when the reader read no header, the header has no such
   column, or the row is shorter than the header and has no field there. When the header names a column twice, the
   first is found.
3. The same for a literal, `row["age"]`, which would otherwise be ambiguous between (1) and (2).

## Parameters

| Parameter | Description |
|---|---|
| `index` | the field's index, from 0 |
| `column` | the name of the header's column |

## Return value

- (1) The field.
- (2–3) The field, or `nullopt`.

## Complexity

- (1) Constant.
- (2–3) A lookup of the name in the header's hash table.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::csv::reader r("name,age,city\nAnn,31,Oslo\nBob,27\n", {.same_field_count = false});
    r.read_header();
    while (auto row = r.next()) {
        string city = "city";
        println("{} {} {} {}", (*row)[0], (*row)["age"].value(), (*row)[city].has_value(),
                (*row)["zip"].has_value());
    }
}
```

Output:

```text
Ann 31 true false
Bob 27 false false
```

## See also

- [at](at.md): a field by its index, checked
- [get](get.md): the field of a column as a string, or a fallback
- [sgcl::encoding::csv::row](../csv-row.md)
