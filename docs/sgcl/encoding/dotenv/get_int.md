[sgcl](../../README.md) › [encoding](../README.md) › [dotenv](README.md)

# sgcl::encoding::dotenv::get_int, get_double, get_bool

```cpp
optional<int64_t> get_int(const string& key) const noexcept;             // (1)
int64_t get_int(const string& key, int64_t fallback) const noexcept;     // (2)
optional<double> get_double(const string& key) const noexcept;           // (3)
double get_double(const string& key, double fallback) const noexcept;    // (4)
optional<bool> get_bool(const string& key) const noexcept;               // (5)
bool get_bool(const string& key, bool fallback) const noexcept;          // (6)
```

The value of the key read as a number or a boolean, blanks at its ends passed over; nullopt, or the fallback, when
there is no such key or its value does not read.

1. A decimal integer with an optional sign, within `int64_t`.
2. (1), or `fallback`.
3. A decimal float, `inf` and `nan` among them.
4. (3), or `fallback`.
5. `true`, `yes`, `on`, `1` and `false`, `no`, `off`, `0`, in any case (Python's configparser's words).
6. (5), or `fallback`.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key |
| `fallback` | what the even forms give when there is no value |

## Return value

The value, or `nullopt`, or `fallback`.

## Complexity

Linear in the entries.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::dotenv::options o;
    o.use_environment = false;
    encoding::dotenv env = encoding::dotenv::parse(R"(# the service
export HOST=example.com
PORT=8080
DEBUG=yes
GREETING="hello\tworld"
URL=http://${HOST}:${PORT}/
)", o).value();
    println("{} {} {}", env.get_int("PORT"), env.get_int("HOST"), env.get_int("HOST", -1));
    println("{} {}", env.get_bool("DEBUG"), env.get_bool("PORT", false));
    println(env.get_double("PORT", 0.0));
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
- [sgcl::encoding::dotenv](README.md)
