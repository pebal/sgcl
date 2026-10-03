[sgcl](../../README.md) › [encoding](../README.md) › [json](../json/README.md) › [reader](README.md)

# sgcl::encoding::json::reader::depth

```cpp
uint32_t depth() const noexcept;
```

The number of arrays and objects open: 0 at the top level, one more after each opening bracket, one less after
each closing one. A depth past [options](../json-options.md)`::max_depth` (512) is never reached: the bracket
that would open it is `depth_limit`. Go's v2 `Decoder.StackDepth`.

## Parameters

None.

## Return value

The arrays and objects open.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

#include <algorithm>

using namespace sgcl;

int main() {
    encoding::json::reader r(string(R"({"a": [[1], {"b": [2, 3]}]})"));
    uint32_t deepest = 0;
    while (r.next()) {
        deepest = std::max(deepest, r.depth());
    }
    println("{} {}", deepest, r.depth());

    encoding::json::options o;
    o.max_depth = 2;
    encoding::json::reader shallow(string("[[[1]]]"), o);
    while (shallow.next()) {
    }
    println("{} {}", shallow.depth(), shallow.last_error()->message());
}
```

Output:

```text
4 0
2 1:3: nesting deeper than 2
```

## See also

- [more](more.md): whether the array or the object open has another element
- [json::options](../json-options.md): `max_depth`
- [sgcl::encoding::json::reader](README.md)
