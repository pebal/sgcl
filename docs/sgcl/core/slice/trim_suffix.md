[sgcl](../../README.md) › [core](../README.md) › [slice](../slice.md)

# sgcl::slice\<T\>::trim_suffix

```cpp
slice trim_suffix(std::basic_string_view<CharT> suffix) const noexcept;
```

The text without `suffix` when it ends with it, the text as it is otherwise, as Go's `strings.TrimSuffix`: a slice
of the same owner, nothing copied. Takes part only for a slice of a character type.

## Parameters

| Parameter | Description |
|---|---|
| `suffix` | the suffix to drop |

## Return value

A slice of the characters before the suffix, or the slice itself, with the same owner.

## Complexity

Linear in the size of `suffix`.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string file = "archive.tar.gz";
    string_slice s = file;
    println("{} {}", s.trim_suffix(".gz"), s.trim_suffix(".zip"));
}
```

Output:

```text
archive.tar archive.tar.gz
```

## See also

- [trim_prefix](trim_prefix.md): the same at the start
- [remove_suffix](remove_suffix.md): drops a number of characters, in place
- [sgcl::slice\<T\>](../slice.md)
