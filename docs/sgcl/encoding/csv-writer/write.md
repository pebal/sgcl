[sgcl](../../README.md) › [encoding](../README.md) › [csv](../csv.md) › [writer](../csv-writer.md)

# sgcl::encoding::csv::writer::write

```cpp
/*(1)*/ writer& write(std::initializer_list<string> fields) noexcept;
/*(2)*/ writer& write(const row& r) noexcept;
/*(3)*/ template<class R>
        requires std::ranges::input_range<const R&>
            && (std::is_convertible_v<std::ranges::range_reference_t<const R&>, std::string_view>
                || std::is_convertible_v<std::ranges::range_reference_t<const R&>, string>
                || std::is_same_v<std::remove_cvref_t<std::ranges::range_reference_t<const R&>>,
                                  slice<const char>>)
        writer& write(const R& fields);
/*(4)*/ template<class T>
        writer& write(const T& record);
```

Writes a record, Go's `Write`: the fields one after another with the separator between them, each quoted when it
holds the separator, a quote, `'\r'` or `'\n'`, starts with a space or is `\.`, a quote inside doubled, and the
record's line ending after them. A record of one empty field is written `""`, where Go writes an empty line that a
reader would pass over, and a first field that starts with the options' comment character is quoted, so that a
reader of the same options reads the record; a record of no fields is an empty line, which a reader passes over. The
text is gathered in the writer; [flush](flush.md) hands it to the stream.

1. The fields of a list: `w.write({"a", "b"})`.
2. The fields of a [row](../csv-row.md) as it was read, with this writer's separator.
3. The fields of any range of texts: a `vector<string>`, a vector of `std::string`, of `const char*`, of slices.
4. The fields of a value of a type with `describe(field_list&)` ([field_list](../field_list.md)), in the order
   `describe` names them; takes part only when `T` has `describe`, a method or a free function. The first such
   record writes the header of the field names before it. A field is written as one text: a number in decimal,
   NaN and the infinities as `NaN`, `+Inf`, `-Inf`, a boolean `true` or `false`, an enum with `names` by its name,
   a type with `to_text` by its text, an empty optional as an empty field. A field of any other kind (a container,
   a structure) writes nothing more: the writer keeps `unsupported_value` and [flush](flush.md) returns it.

Once the writer holds that mistake, or the failure of its stream, nothing more reaches the stream and nothing more
is gathered: a record written then is dropped, and every `flush` returns the first.

## Parameters

| Parameter | Description |
|---|---|
| `fields` | the texts of the record |
| `r` | a record read |
| `record` | a value whose fields are the record |

## Return value

`*this`, so that writes chain: `w.write({"a"}).write({"b"})`.

## Complexity

Linear in the length of the fields.

## Exceptions

- (1–2) None.
- (3) What the conversion of an element to text throws.
- (4) What `describe` of `T` throws.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

#include <cmath>

using namespace sgcl;

struct sample {
    string label;
    double value = 0;
    optional<int> rank;

    void describe(encoding::field_list& f) {
        f.add("label", label);
        f.add("value", value);
        f.add("rank", rank);
    }
};

int main() {
    encoding::csv::writer w(io::stdout);
    w.write(sample{"first, best", 0.1, 1}).write(sample{"none", NAN, nullopt});
    w.write(vector<string>{"\\.", "a\"b"});

    encoding::csv::reader r("x;y\n");
    w.write(r.next().value());
    w.flush().value();
}
```

Output:

```text
label,value,rank
"first, best",0.1,1
none,NaN,
"\.","a""b"
x;y
```

## See also

- [flush](flush.md): the text into the stream
- [csv::save](../csv/save.md): values into a file
- [reader::read\<T\>](../csv-reader/read.md): a record read back as a value
- [sgcl::encoding::csv::writer](../csv-writer.md)
