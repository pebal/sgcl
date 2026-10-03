[sgcl](../../README.md) › [time](../README.md) › [zone](../zone.md)

# sgcl::time::zone::available

```cpp
static vector<string> available() noexcept;
```

The names of the zones of the system's tz database, sorted: every file of the database that starts with `TZif`,
leaving out the copies of the whole database for other uses (`posix/`, `right/`), `posixrules` and `localtime` —
598 names on this machine. Each of them [loads](load.md). Go cannot list the zones.

The directory is read again at every call, on the thread that asks: the files of the database are listed and the
start of each is read.

## Parameters

None.

## Return value

The names, sorted; empty where the system has no database (Windows).

## Complexity

Linear in the number of files of the database.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/time.h"

using namespace sgcl;

int main() {
    vector<string> names = time::zone::available();
    size_t loaded = 0;
    for (const string& name : names) {
        if (time::zone::load(name)) {
            ++loaded;
        }
    }
    println("{} {}", names.size() > 400, loaded == names.size());
}
```

Output:

```text
true true
```

## See also

- [load](load.md): a zone by its name
- [sgcl::time::zone](../zone.md)
