[sgcl](../../README.md) › [encoding](../README.md) › [json](../json/README.md) › [writer](README.md)

# sgcl::encoding::json::writer::begin_object

```cpp
writer& begin_object() noexcept;
```

Opens an object: `{`. An object is a value, so it stands where a value may — at the top level, as an element of
an array, after a [key](key.md) — and its members follow as a key and a value each, up to
[end_object](end_object.md). An object where a key belongs (directly inside another object) is a mistake, kept
and reported by [flush](flush.md).

## Parameters

None.

## Return value

`*this`, for the next step in the chain.

## Complexity

Amortized constant: the bracket, and the line ending and the indent of a laid-out text.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::json::writer out(io::stdout);
    out.begin_object().key("user").begin_object().key("id").value(7).end_object().end_object();
    out.begin_array().begin_object().end_object().end_array();
    out.flush();

    encoding::json::writer wrong(io::stdout);
    wrong.begin_object().begin_object();
    println(wrong.flush().error().message());
}
```

Output:

```text
{"user":{"id":7}}
[{}]
json: a value in an object where a key belongs: syntax error
```

## See also

- [end_object](end_object.md): closes it
- [key](key.md): the key of a member
- [begin_array](begin_array.md): opens an array
- [sgcl::encoding::json::writer](README.md)
