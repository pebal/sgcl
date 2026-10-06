[sgcl](../../README.md) › [encoding](../README.md) › [ini](README.md)

# sgcl::encoding::ini::contains

```cpp
bool contains(const string& section) const noexcept;                       // (1)
bool contains(const string& section, const string& key) const noexcept;    // (2)
```

1. Whether there is the section, entries in it or not.
2. Whether there is the key in the section.

## Parameters

| Parameter | Description |
|---|---|
| `section` | the section's name, exactly |
| `key` | the key, exactly |

## Return value

`true` when there is.

## Complexity

Linear in the sections, and (2) in the section's entries.

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
    println("{} {} {} {}", config.contains("server"), config.contains("Server"), config.contains("server", "host"),
            config.contains("paths", "host"));
}
```

Output:

```text
true false true false
```

## See also

- [get](get.md)
- [sgcl::encoding::ini](README.md)
