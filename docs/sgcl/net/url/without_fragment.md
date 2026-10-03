[sgcl](../../README.md) › [net](../README.md) › [url](README.md)

# sgcl::net::url::without_fragment

```cpp
url without_fragment() const noexcept;
```

The URL without its fragment, nor its `#`: what a client sends, which never carries the fragment.

## Parameters

None.

## Return value

The new URL; an equal one when there is no fragment.

## Complexity

Linear in the length of the URL.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    println(net::url("https://x/doc?q=1#top").without_fragment().to_string());
}
```

Output:

```text
https://x/doc?q=1
```

## See also

- [with_fragment](with_fragment.md): another fragment
- [sgcl::net::url](README.md)
