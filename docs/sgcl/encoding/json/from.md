[sgcl](../../README.md) › [encoding](../README.md) › [json](README.md)

# sgcl::encoding::json::from

```cpp
template<class T>
static expected<json, error> from(const T& value);
```

The value of a program's `T` as a json, as [xml::from](../xml/from.md) makes an element of one: what
[stringify](stringify.md) writes, read back as a value. It goes through the text, so it is for a value made once,
for a tree to go on building or to hand on; [as](as.md)`<T>` is the way back. `T` is a type described by its
fields ([field_list](../field_list/README.md)) or any kind a field may have.

## Parameters

| Parameter | Description |
|---|---|
| `value` | the value to turn into a json |

## Return value

The json, or the [error](../error/README.md) of [stringify](stringify.md): `unsupported_value` with the path of a value
that has no text (NaN, an enum's value past its names, nesting past 512), and no place: its `message()` is the
path and the words.

## Complexity

Linear in the size of the text of `value`.

## Exceptions

- `length_error` when the text would pass the 4 GiB a [string](../../core/string/README.md) holds.
- What the program's code that the writing calls throws: `describe`, a field's `to_text` or `to_json`.

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
    auto p = encoding::json::from(point{3, 4}).value();
    auto labelled = p.set("label", "corner");
    println(labelled.to_string());
    println(labelled["y"].as_int(0));
}
```

Output:

```text
{"x":3,"y":4,"label":"corner"}
4
```

## See also

- [as](as.md): the value as a program's type
- [stringify](stringify.md): the text of a program's value
- [sgcl::encoding::json](README.md)
