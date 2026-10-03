[sgcl](../../README.md) › [encoding](../README.md) › [json](../json/README.md) › [writer](README.md)

# sgcl::encoding::json::writer::end_object

```cpp
writer& end_object() noexcept;
```

Closes the object open: `}`. At the top level the object is followed by a line ending. With no object open — at
the top level, or where an array is open — and after a [key](key.md) with no value, it is a mistake, kept and
reported by [flush](flush.md).

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
    out.begin_object().key("a").value(1).end_object();
    out.begin_object().end_object();
    out.flush();

    encoding::json::writer no_value(io::stdout);
    no_value.begin_object().key("a").end_object();
    println(no_value.flush().error().message());

    encoding::json::writer in_array(io::stdout);
    in_array.begin_array().end_object();
    println(in_array.flush().error().message());
}
```

Output:

```text
{"a":1}
{}
json: end_object after a key with no value: syntax error
json: end_object without an open object: syntax error
```

## See also

- [begin_object](begin_object.md): opens it
- [end_array](end_array.md): closes an array
- [sgcl::encoding::json::writer](README.md)
