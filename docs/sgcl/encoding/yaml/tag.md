[sgcl](../../README.md) › [encoding](../README.md) › [yaml](README.md)

# sgcl::encoding::yaml::tag

```cpp
string tag() const noexcept;
```

The application's tag of the node, its handle resolved: `!Ref` as written, `!e!point` under
`%TAG !e! tag:example.com,2026:` as `tag:example.com,2026:point`, `!!binary` as `tag:yaml.org,2002:binary`, a
verbatim `!<...>` as its URI. Empty for a node without one and for one under a core tag of its kind (`!!str`,
`!!int`), which made the kind.

## Parameters

None.

## Return value

The tag, or the empty string.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto v = encoding::yaml::parse("[!Ref name, !!binary aGk=, !!str 12, plain]").value();
    for (const auto& e : v.elements()) {
        println("[{}] {}", e.tag(), e.text());
    }
}
```

Output:

```text
[!Ref] name
[tag:yaml.org,2002:binary] aGk=
[] 12
[] plain
```

## See also

- [tagged](tagged.md)
- [sgcl::encoding::yaml](README.md)
