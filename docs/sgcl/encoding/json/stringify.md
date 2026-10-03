[sgcl](../../README.md) › [encoding](../README.md) › [json](../json.md)

# sgcl::encoding::json::stringify

```cpp
/*(1)*/ template<class T> static expected<string, error> stringify(const T& value);
/*(2)*/ template<class T> static expected<string, error> stringify(const T& value, const style& s);
```

The text of a program's value, Go's `json.Marshal` of a struct: `T` is a type described by its fields
([field_list](../field_list.md)) or any kind a field may have — a number, a string, a container, a
[json](../json.md) — and the text is the one Go writes, the fields in their order. The maps and the sets of hash
keep their keys sorted by their text, as Go sorts the keys of a map, unless `style::sort_keys` is false; a sorted
or ordered container is written in its order. A float is written with its own shortest digits, `0.1` for `0.1f`.

1. The text, compact.
2. The text in the [style](../json-style.md) `s`.

## Parameters

| Parameter | Description |
|---|---|
| `value` | the value to write |
| `s` | how the value is written |

## Return value

The text, or an [error](../error.md) where a value has no text: `unsupported_value` with the path of the value,
for NaN or an infinity, an enum's value past its names, nesting past 512 (a cycle of pointers), or a variant
field without `tagged()`. The error has no place, there being no text it was read from: no line, no column and
an offset of 0, and its `message()` is the path and the words, `/x: NaN is not a JSON number`.

## Complexity

Linear in the size of the text.

## Exceptions

- `length_error` when the text would pass the 4 GiB a [string](../../core/string.md) holds; the text is
  measured where its block grows, and the writing stops as soon as it would pass.
- What the program's code that the writing calls throws: `describe`, a field's `to_text` or `to_json`.

## Example

```cpp
#include "sgcl/core.h"
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
    println(encoding::json::stringify(point{1, 2.5}).value());
    println(encoding::json::stringify(point{1, 2.5}, encoding::json::pretty).value());

    map<string, int> scores = {{"go", 3}, {"cpp", 5}, {"rust", 4}};
    println(encoding::json::stringify(scores).value());
    println(encoding::json::stringify(vector<float>{0.1f, 1.5f}).value());

    auto wrong = encoding::json::stringify(point{NAN, 0});
    println(wrong.error().message());
}
```

Output:

```text
{"x":1,"y":2.5}
{
  "x": 1,
  "y": 2.5
}
{"cpp":5,"go":3,"rust":4}
[0.1,1.5]
/x: NaN is not a JSON number
```

## See also

- [parse](parse.md): the way back, `parse<T>`
- [save](save.md): the text into a file
- [from](from.md): the value of a program's type as a json
- [to_string](to_string.md): the text of a json
- [sgcl::encoding::json](../json.md)
