[sgcl](../../README.md) › [encoding](../README.md) › [email](../email/README.md) › [part](README.md)

# sgcl::encoding::email::part::headers

```cpp
vector<pair<string, string>> headers() const;
```

Every field of the part's head in its order, the values as [header](header.md) gives them.

## Parameters

None.

## Return value

The fields.

## Complexity

Linear in the size of the head.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::email::part p("text/plain", "x");
    for (auto& [name, value] : p.headers()) {
        println("{}: {}", name, value);
    }
}
```

Output:

```text
Content-Type: text/plain; charset=utf-8
```

## See also

- [set_header](set_header.md)
- [part](README.md)
