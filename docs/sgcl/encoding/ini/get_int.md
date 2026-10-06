[sgcl](../../README.md) › [encoding](../README.md) › [ini](README.md)

# sgcl::encoding::ini::get_int, get_double, get_bool

```cpp
optional<int64_t> get_int(const string& section, const string& key) const noexcept;             // (1)
int64_t get_int(const string& section, const string& key, int64_t fallback) const noexcept;     // (2)
optional<double> get_double(const string& section, const string& key) const noexcept;           // (3)
double get_double(const string& section, const string& key, double fallback) const noexcept;    // (4)
optional<bool> get_bool(const string& section, const string& key) const noexcept;               // (5)
bool get_bool(const string& section, const string& key, bool fallback) const noexcept;          // (6)
```

The value of the key in the section read as a number or a boolean, blanks at its ends passed over; nullopt, or the
fallback, when there is no such section or key or its value does not read.

1. A decimal integer with an optional sign, within `int64_t`.
2. (1), or `fallback`.
3. A decimal float, `inf` and `nan` among them.
4. (3), or `fallback`.
5. `true`, `yes`, `on`, `1` and `false`, `no`, `off`, `0`, in any case: configparser's `getboolean`.
6. (5), or `fallback`.

## Parameters

| Parameter | Description |
|---|---|
| `section` | the section's name |
| `key` | the key |
| `fallback` | what the even forms give when there is no value |

## Return value

The value, or `nullopt`, or `fallback`.

## Complexity

Linear in the sections and in the section's entries.

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
    println("{} {} {}", config.get_int("server", "port"), config.get_int("server", "host"), config.get_int("server", "host", -1));
    println("{} {}", config.get_bool("server", "tls"), config.get_bool("server", "port", false));
    println(config.get_double("server", "port", 0.0));
}
```

Output:

```text
8080 nullopt -1
true false
8080
```

## See also

- [get](get.md)
- [sgcl::encoding::ini](README.md)
