[sgcl](../../README.md) › [core](../README.md) › [slice](README.md)

# sgcl::slice\<T\>::trim_prefix

```cpp
slice trim_prefix(std::basic_string_view<CharT> prefix) const noexcept;
```

The text without `prefix` when it starts with it, the text as it is otherwise, as Go's `strings.TrimPrefix`: a
slice of the same owner, nothing copied. Takes part only for a slice of a character type.

## Parameters

| Parameter | Description |
|---|---|
| `prefix` | the prefix to drop |

## Return value

A slice of the characters after the prefix, or the slice itself, with the same owner.

## Complexity

Linear in the size of `prefix`.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string header = "Bearer abc123";
    string_slice s = header;
    println("{} {}", s.trim_prefix("Bearer "), s.trim_prefix("Basic "));
}
```

Output:

```text
abc123 Bearer abc123
```

## See also

- [trim_suffix](trim_suffix.md): the same at the end
- [remove_prefix](remove_prefix.md): drops a number of characters, in place
- [sgcl::slice\<T\>](README.md)
