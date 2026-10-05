[sgcl](../../README.md) › [encoding](../README.md) › [json](../json/README.md)

# sgcl::encoding::json::builder

```cpp
#include "sgcl/encoding/json.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    class json {
    public:
        class builder;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::encoding::json::builder` makes an array or an object in a loop without copying it at every step:
[push_back](push_back.md) gathers the elements of an array, [set](set.md) the members of
an object, and [build](build.md) hands out the [json](../json/README.md) and leaves the builder empty for the
next one. Where a json's own [push_back](../json/push_back.md) and [set](../json/set.md) copy the whole value for each
new element, a builder appends to a buffer of its own and makes the value once.

Nothing needs a builder: a value written out at once is made by [array](../json/array.md) or
[object](../json/object.md), and an array of a range by `array` too. The builder is for a value made one element or
one member at a time, with conditions between.

## Rules

- A builder is a value of its own, for one thread at a time; a copy is a builder of its own with the same elements, and
  a builder moved from is empty, as `build` leaves it.
- One builder makes one kind at a time: the first `push_back` or `set` decides, and the other one on the same
  builder is `logic_error` until `build` empties it. An empty builder builds `[]`.
- A key set twice keeps its last value, in the place of its last `set`, as [object](../json/object.md) does.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](json-builder.md) | constructs an empty builder |
| `(destructor)` | drops what the builder holds; it is left to the collector |

#### Capacity

| Function | Description |
|---|---|
| [size](size.md) | the number of elements or members so far |

#### Modifiers

| Function | Description |
|---|---|
| [push_back](push_back.md) | appends an element of an array |
| [set](set.md) | adds a member of an object |
| [build](build.md) | the value, and the builder empty again |

## Complexity

- `push_back`, `set`: amortized constant.
- `build`: linear in the number of elements or members.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::json::builder squares;
    for (int i : range(6)) {
        if (i % 2 == 0) {
            squares.push_back(i * i);
        }
    }
    size_t count = squares.size();
    encoding::json::builder report;
    report.set("count", count).set("squares", squares.build());
    println(report.build().to_string());
    println(squares.build().to_string());
}
```

Output:

```text
{"count":3,"squares":[0,4,16]}
[]
```

## See also

- [array](../json/array.md), [object](../json/object.md): a value written out at once
- [writer](../json-writer/README.md): JSON written into a stream without a value at all
- [sgcl::encoding::json](../json/README.md)
