[sgcl](../../README.md) › [slog](../README.md) › [record](README.md)

# sgcl::slog::record::begin

```cpp
attrs::iterator begin() const noexcept;
```

Returns an iterator to the first attribute at the top of the record's tree: the logger's attributes from `with`
before its first group, then that group, or the call's own attributes when the logger has no group. The
[iterator](../attrs-iterator.md) is an input iterator whose `*` gives an [attr](../attr/README.md) by value; it compares
equal to [end](end.md) past the last one. A `for` over the record walks them.

## Parameters

None.

## Return value

An iterator to the first attribute; equal to `end()` for a record with none.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/slog.h"

using namespace sgcl;

int main() {
    slog::memory kept;
    slog::logger(kept).with("a", 1).info("m", "b", 2);
    auto r = kept.records()[0];
    for (auto i = r.begin(); i != r.end(); ++i) {
        println("{}", (*i).key());
    }
}
```

Output:

```text
a
b
```

## See also

- [end](end.md), [size](size.md)
- [attr](../attr/README.md)
- [sgcl::slog::record](README.md)
