[sgcl](../../README.md) › [encoding](../README.md) › [json](README.md)

# sgcl::encoding::json::set_path

```cpp
json set_path(const string& pointer, const json& value) const noexcept;
```

A new value with the one at a JSON Pointer ([RFC 6901](https://www.rfc-editor.org/rfc/rfc6901)) replaced or
added; this value stays as it was. The last token of the pointer:

- over an object, sets the member, replaced or added;
- over an array, replaces an element, or appends one with `-` or with the index of the end;
- over null, makes an object of the one member.

The objects missing on the way are made, and a null on the way becomes an object. The pointer `""` gives `value`
itself. A pointer that goes through a number, a string or a boolean, past the end of an array, or through a token
that is not an index of an array, and a text that is not a pointer at all, give this value unchanged.

## Parameters

| Parameter | Description |
|---|---|
| `pointer` | the JSON Pointer: empty, or `/` and the tokens separated by `/`, with `~1` for `/` and `~0` for `~` in a key |
| `value` | the value to put there |

## Return value

The new value, or this value when the pointer leads nowhere.

## Complexity

Linear in the length of the pointer, and for each container on the path in the number of its members or
elements, which are copied as handles.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::json doc = encoding::json::parse(
        R"({"user": {"name": "Ala", "tags": ["a"]}, "extra": null})");
    println(doc.set_path("/user/name", "Ola").to_string());
    println(doc.set_path("/user/tags/-", "b").set_path("/user/tags/0", "z").to_string());
    println(doc.set_path("/extra/deep/er", 1).to_string());
    println(doc.set_path("/user/name/first", "Ola") == doc);
    println(doc.set_path("/user/tags/5", "b") == doc);
    println(doc.to_string());
}
```

Output:

```text
{"user":{"name":"Ola","tags":["a"]},"extra":null}
{"user":{"name":"Ala","tags":["z","b"]},"extra":null}
{"user":{"name":"Ala","tags":["a"]},"extra":{"deep":{"er":1}}}
true
true
{"user":{"name":"Ala","tags":["a"]},"extra":null}
```

## See also

- [at_path](at_path.md): the value at a pointer
- [set](set.md), [push_back](push_back.md): one step
- [sgcl::encoding::json](README.md)
