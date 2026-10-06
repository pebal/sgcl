[sgcl](../../README.md) › [encoding](../README.md) › [ini](README.md)

# sgcl::encoding::ini::sections

```cpp
slice<const section> sections() const noexcept;
```

The [sections](../ini-section.md) in the file's order, each with its entries in order, a slice of the value,
nothing copied; the section `""` first when there are keys before the first header.

## Parameters

None.

## Return value

The sections.

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
    encoding::ini config = encoding::ini::parse(R"(name = app

; the server
[server]
host = example.com
port: 8080
tls = on
motd = Welcome!
    Second line.

[paths]
data = /var/lib/app
)").value();
    for (const auto& [name, members] : config.sections()) {
        println("[{}] {} entries", name, members.size());
        for (const auto& [key, value] : members) {
            println("  {} = {}", key, value);
        }
    }
}
```

Output:

```text
[] 1 entries
  name = app
[server] 4 entries
  host = example.com
  port = 8080
  tls = on
  motd = Welcome!
Second line.
[paths] 1 entries
  data = /var/lib/app
```

## See also

- [get](get.md)
- [sgcl::encoding::ini](README.md)
