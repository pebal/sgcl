[sgcl](../../README.md) › [concurrent](../README.md) › [copy_on_write](../copy_on_write.md)

# sgcl::concurrent::copy_on_write\<T\>::load, operator snapshot

```cpp
snapshot load() const noexcept;        // (1)
operator snapshot() const noexcept;    // (2)
```

1. The current value: one atomic load of the pointer.
2. The same load, as an implicit conversion, so that a `copy_on_write` goes where a snapshot is wanted:
   `tracked_ptr<const T> c = config;`.

The snapshot holds the value alive and unchanged for as long as it exists, whatever the writers do meanwhile.

## Parameters

None.

## Return value

A snapshot of the current value, `tracked_ptr<const T>`.

## Complexity

Constant: one atomic load.

## Exceptions

None.

## Notes

Wait-free, and never blocked by a writer: a writer builds its value apart and swings the pointer, so a reader sees
the old value whole or the new value whole, never a part of one. Two loads may see two values; what has to be read
as one is read through one snapshot.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Config {
    string host;
    int port;
};

int main() {
    concurrent::copy_on_write config(Config{"localhost", 8080});

    auto before = config.load();
    config.store(Config{"example.com", 443});
    tracked_ptr<const Config> after = config;

    println("{}:{}", before->host, before->port);
    println("{}:{}", after->host, after->port);
}
```

Output:

```text
localhost:8080
example.com:443
```

## See also

- [store](store.md), [update](update.md): replace the value
- [sgcl::concurrent::copy_on_write\<T\>](../copy_on_write.md)
