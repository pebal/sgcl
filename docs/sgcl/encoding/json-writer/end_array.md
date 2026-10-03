[sgcl](../../README.md) › [encoding](../README.md) › [json](../json/README.md) › [writer](README.md)

# sgcl::encoding::json::writer::end_array

```cpp
writer& end_array() noexcept;
```

Closes the array open: `]`. At the top level the array is followed by a line ending. With no array open — at the
top level, or where an object is open — it is a mistake, kept and reported by [flush](flush.md).

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
    encoding::json::writer out(io::stdout, encoding::json::pretty);
    out.begin_array().value("a").begin_array().end_array().end_array();
    out.flush();

    encoding::json::writer wrong(io::stdout);
    wrong.begin_object().end_array();
    println(wrong.flush().error().message());
}
```

Output:

```text
[
  "a",
  []
]
json: end_array without an open array: syntax error
```

## See also

- [begin_array](begin_array.md): opens it
- [end_object](end_object.md): closes an object
- [sgcl::encoding::json::writer](README.md)
