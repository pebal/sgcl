[sgcl](../../README.md) › [time](../README.md) › [zone](README.md)

# sgcl::time::zone::zone

```cpp
zone() noexcept = default;            // (1)
explicit zone(const string& name);    // (2)
```

1. UTC, the same as [utc](utc.md): the null pointer, nothing allocated.
2. The zone a name written in the program names: [load](load.md)'s zone, or a
   `bad_expected_access<time::error>` with `load`'s error, as `std::chrono::locate_zone` throws for a name it does
   not know. A name that comes from outside (a setting, the user) is loaded, and its error is a value; one the
   program itself wrote is constructed.

## Parameters

| Parameter | Description |
|---|---|
| `name` | a name of the system's tz database, `"Europe/Warsaw"`, or `"UTC"` |

## Complexity

- (1) Constant.
- (2) As [load](load.md): linear in the size of the zone's file the first time the name is loaded, a lookup of
  the name after that.

## Exceptions

- (1) None.
- (2) `bad_expected_access<time::error>` with `load`'s error when the name is not a zone of the system's database;
  its `what()` is the error's message.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/time.h"

using namespace sgcl;

int main() {
    time::zone warsaw("Europe/Warsaw");
    time::zone none;
    println("{} {}", warsaw.name(), none.name());
    try {
        time::zone mars("Mars/Olympus_Mons");
    } catch (const bad_expected_access<time::error>& e) {
        println("{}", e.what());
    }
}
```

Output:

```text
Europe/Warsaw UTC
unknown time zone "Mars/Olympus_Mons"
```

## See also

- [load](load.md): a name from outside, its error a value
- [utc](utc.md): UTC
- [sgcl::time::zone](README.md)
