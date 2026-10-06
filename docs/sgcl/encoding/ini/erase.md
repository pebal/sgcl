[sgcl](../../README.md) › [encoding](../README.md) › [ini](README.md)

# sgcl::encoding::ini::erase

```cpp
ini erase(const string& section, const string& key) const noexcept;    // (1)
ini erase(const string& section) const noexcept;                       // (2)
```

New sections; the value as it is when there is nothing to take out.

1. Without the key in the section (the section stays, empty or not).
2. Without the section and its entries.

## Parameters

| Parameter | Description |
|---|---|
| `section` | the section's name |
| `key` | the key |

## Return value

The new sections.

## Complexity

Linear in the size of the sections.

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
    println("{} {} {}", config.erase("paths").size(), config.erase("paths", "data").size(), config.erase("none").size());
}
```

Output:

```text
2 3 3
```

## See also

- [set](set.md)
- [sgcl::encoding::ini](README.md)
