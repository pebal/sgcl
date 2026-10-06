[sgcl](../../README.md) › [compress](../README.md) › [zstd](../zstd/README.md) › [dictionary](README.md)

# sgcl::compress::zstd::dictionary::empty

```cpp
bool empty() const noexcept;
```

Checks whether the dictionary is none: made by the default constructor, or read from empty bytes. Options holding
such a dictionary compress and read without one.

## Parameters

None.

## Return value

`true` when it is none, `false` otherwise.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    compress::zstd::options o;
    println("{}", o.dictionary.empty());
    o.dictionary = compress::zstd::dictionary(slice<const byte>(string("common words")));
    println("{}", o.dictionary.empty());
}
```

Output:

```text
true
false
```

## See also

- [size](size.md)
- [sgcl::compress::zstd::dictionary](README.md)
