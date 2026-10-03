[sgcl](../../README.md) › [net](../README.md) › [url](README.md)

# sgcl::net::url::fragment

```cpp
string fragment() const noexcept;
```

The fragment without its `#`, escaped as the URL writes it (Go's `URL.Fragment` holds it unescaped).

## Parameters

None.

## Return value

The fragment; the empty string when there is none or it is empty ([has_fragment](has_fragment.md) tells the two
apart).

## Complexity

Linear in the length of the part.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    println(net::url("https://x/doc#part 2").fragment());
}
```

Output:

```text
part%202
```

## See also

- [with_fragment](with_fragment.md), [without_fragment](without_fragment.md): another fragment, none
- [sgcl::net::url](README.md)
