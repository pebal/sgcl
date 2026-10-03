[sgcl](../../README.md) › [encoding](../README.md) › [csv](../csv.md) › [row](../csv-row.md)

# sgcl::encoding::csv::row::get

```cpp
string get(const string& column, const string& fallback) const noexcept;
```

The field of the header's column named `column` as a [string](../../core/string.md) of its own, or `fallback` when
there is none: the reader read no header, the header has no such column, or the row is shorter than the header.
`row.get("city", "?")` is the one-line form of `row["city"]` with a default.

## Parameters

| Parameter | Description |
|---|---|
| `column` | the name of the header's column |
| `fallback` | what is returned when the row has no such field |

## Return value

The field's text, or `fallback`.

## Complexity

A lookup of the name in the header's hash table, and linear in the length of the field.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::csv::reader r("name,city\nAnn,Oslo\nBob\n", {.same_field_count = false});
    r.read_header();
    while (auto row = r.next()) {
        println("{} lives in {}", row->get("name", "?"), row->get("city", "an unknown place"));
    }
}
```

Output:

```text
Ann lives in Oslo
Bob lives in an unknown place
```

## See also

- [operator\[\]](operator_at.md): the field as a slice, or `nullopt`
- [reader::read_header](../csv-reader/read_header.md): the names of the columns
- [sgcl::encoding::csv::row](../csv-row.md)
