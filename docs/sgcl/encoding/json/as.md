[sgcl](../../README.md) › [encoding](../README.md) › [json](../json.md)

# sgcl::encoding::json::as

```cpp
/*(1)*/ template<class T> expected<T, error> as() const;
/*(2)*/ template<class T> expected<T, error> as(const options& o) const;
```

This value as a program's `T`: a type described by its fields ([field_list](../field_list.md)) or any kind a field
may have. It reads the tree as [parse](parse.md)`<T>` reads a text, by the same rules — the members into the
fields by their names, a key no field has skipped, a field that is not there left as it is unless it is
`required()` — for a value that came as a json: a member of a larger document, the answer of a server. It is
Go's `json.Unmarshal` into a struct of a value already decoded.

1. With the default [options](../json-options.md).
2. With `o`: of its members, `max_depth` and `reject_unknown_fields` matter here, the tree being parsed already.

## Parameters

| Parameter | Description |
|---|---|
| `o` | what the reading accepts |

## Return value

The `T`, or an [error](../error.md) with the path of the value that failed as a JSON Pointer: `type_mismatch`,
`missing_field`, `unknown_field` (the path of the unknown member), `out_of_range`. The error has no place, there
being no text: no line, no column and an offset of 0, and its `message()` is the path and the words,
`/x: expected an integer, found a string`.

## Complexity

Linear in the size of the value.

## Exceptions

What the program's code that the reading calls throws: the constructors of `T` and of its fields, `describe`, a
field's `from_text` or `from_json`.

## Notes

`T` needs a default constructor: the value is made first and its fields read into it.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

struct point {
    int x = 0;
    int y = 0;

    void describe(encoding::field_list& f) {
        f.add("x", x);
        f.add("y", y);
    }
};

int main() {
    encoding::json doc = encoding::json::parse(R"({"from": {"x": 1, "y": 2}, "to": {"x": "far"}})");
    auto from = doc["from"].as<point>();
    println("{} {}", from->x, from->y);
    println(doc["to"].as<point>().error().message());
    println(doc.as<vector<encoding::json>>().error().message());
}
```

Output:

```text
1 2
/x: expected an integer, found a string
expected an array, found an object
```

## See also

- [from](from.md): the way back, a program's value as a json
- [parse](parse.md): `parse<T>`, a program's type of a text
- [sgcl::encoding::json](../json.md)
