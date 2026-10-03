[sgcl](../../README.md) › [encoding](../README.md) › [json](../json.md) › [builder](../json-builder.md)

# sgcl::encoding::json::builder::push_back

```cpp
builder& push_back(const json& value);
```

Appends an element of an array. On an empty builder it makes it a builder of an array, until
[build](build.md) empties it.

## Parameters

| Parameter | Description |
|---|---|
| `value` | the element; anything a json is made of, `5`, `"text"`, another json |

## Return value

`*this`, for a chain of calls.

## Complexity

Amortized constant.

## Exceptions

`logic_error` when the builder is one of an object: a [set](set.md) came first. The builder is as it was.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::json::builder words;
    for (auto w : string("to be or not").split(' ')) {
        words.push_back(w);
    }
    words.push_back(nullptr).push_back(4);
    println(words.build().to_string());

    encoding::json::builder object;
    object.set("a", 1);
    try {
        object.push_back(2);
    } catch (const logic_error& e) {
        println(e.what());
    }
}
```

Output:

```text
["to","be","or","not",null,4]
sgcl: json::builder: push_back on a builder of an object
```

## See also

- [set](set.md): a member of an object
- [build](build.md): the value
- [sgcl::encoding::json::builder](../json-builder.md)
