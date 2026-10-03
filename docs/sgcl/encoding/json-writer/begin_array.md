[sgcl](../../README.md) › [encoding](../README.md) › [json](../json/README.md) › [writer](README.md)

# sgcl::encoding::json::writer::begin_array

```cpp
writer& begin_array() noexcept;
```

Opens an array: `[`. An array is a value, so it stands where a value may — at the top level, as an element of
another array, after a [key](key.md) — and its elements follow up to [end_array](end_array.md). An array where a
key belongs (directly inside an object) is a mistake, kept and reported by [flush](flush.md). The writer sets no
bound on the nesting of what it is given step by step; a value of a type of the program is bounded at 512 levels
([value](value.md)).

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
#include "sgcl/core.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::json::writer out(io::stdout);
    out.begin_array();
    for (int row : range(3)) {
        out.begin_array();
        for (int col : range(3)) {
            out.value(row == col ? 1 : 0);
        }
        out.end_array();
    }
    out.end_array();
    out.flush();
}
```

Output:

```text
[[1,0,0],[0,1,0],[0,0,1]]
```

## See also

- [end_array](end_array.md): closes it
- [begin_object](begin_object.md): opens an object
- [sgcl::encoding::json::writer](README.md)
