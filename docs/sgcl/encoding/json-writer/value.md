[sgcl](../../README.md) › [encoding](../README.md) › [json](../json.md) › [writer](../json-writer.md)

# sgcl::encoding::json::writer::value

```cpp
writer& value(std::nullptr_t) noexcept;                                   // (1)
writer& value(const char* text) noexcept;                                 // (2)
template<class T> writer& value(const T& v) noexcept(/* see below */);    // (3)
```

Writes a value: an element of the array open, the value of the member whose [key](key.md) came last, or a value
at the top level, which a line ending follows. A value where a key belongs (directly inside an object) is a
mistake, kept and reported by [flush](flush.md). The text is written as [to_string](../json/to_string.md) writes
a json.

1. `null`.
2. A C string, to its first NUL, as a JSON string with the escapes it needs.
3. A value of one of the types a json is made of — a [json](../json.md) as it is, a `bool`, an integer of any type
   but `bool` and the characters (exactly, `uint64_t` and `int64_t` whole), a `float`, a `double` or a
   `long double` (a `float` with its own shortest digits, a `long double` as a `double`), a
   [string](../../core/string.md), a `slice<const char>`, a `std::string` or a `std::string_view` — or a value of
   any other type a field may have, written by its fields as [json::stringify](../json/stringify.md) writes it
   ([field_list](../field_list.md)): a type with `describe`, a container, a map (a hash map with its keys sorted
   unless [style](../json-style.md)`::sort_keys` is `false`), an optional. NaN or an infinity is a mistake, and
   so is a value of a type of the program that has no text — an enum's value past its names, a cycle of pointers
   past 512 levels — reported with its path inside the value. noexcept when `T` is one of the types a json is
   made of.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the characters of a string |
| `v` | the value |

## Return value

`*this`, for the next step in the chain.

## Complexity

Linear in the length of the value's text.

## Exceptions

- (1–2) None.
- (3) None for the types a json is made of; for a type of the program, what its `describe`, a field's `to_text`
  or `to_json` throw.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

#include <cmath>

using namespace sgcl;

struct point {
    double x = 0;
    double y = 0;

    void describe(encoding::field_list& f) {
        f.add("x", x);
        f.add("y", y);
    }
};

int main() {
    encoding::json::writer out(io::stdout);
    out.begin_array().value(nullptr).value("tab\t").value(true).value(UINT64_MAX).value(0.1f).value(1e21);
    out.value(encoding::json::parse(R"({"raw": [1]})").value()).end_array();
    out.value(vector<point>{{1, 2}, {3, 4.5}});
    out.value(map<string, int>{{"b", 2}, {"a", 1}});
    out.flush();

    encoding::json::writer wrong(io::stdout);
    wrong.value(vector<point>{{1, 2}, {std::nan(""), 0}});
    println(wrong.flush().error().message());
}
```

Output:

```text
[null,"tab\t",true,18446744073709551615,0.1,1e+21,{"raw":[1]}]
[{"x":1,"y":2},{"x":3,"y":4.5}]
{"a":1,"b":2}
json: /1/x: NaN is not a JSON number: unsupported value
```

## See also

- [key](key.md): the key of a member
- [json::stringify](../json/stringify.md): a value whole, into a string
- [field_list](../field_list.md): how a type of the program is written
- [sgcl::encoding::json::writer](../json-writer.md)
