[sgcl](../../README.md) › [async](../README.md) › [blocking_pool](../blocking_pool.md)

# sgcl::async::blocking_pool::idle_time

```cpp
static duration idle_time();
```

Returns how long a thread of the pool that finds the queue empty waits for a job before it exits: the time
[set_idle_time](set_idle_time.md) set, else `config::blocking_idle_milliseconds` (10 s, `-DSGCL_BLOCKING_IDLE_MS`).

## Parameters

None.

## Return value

The idle time.

## Complexity

Constant.

## Exceptions

`std::system_error` when the pool's lock cannot be taken, as `std::mutex::lock` reports it.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;
using namespace std::chrono_literals;

int main() {
    println("{}", async::blocking_pool::idle_time());
    async::blocking_pool::set_idle_time(2s);
    println("{}", async::blocking_pool::idle_time());
}
```

Output:

```text
10s
2s
```

## See also

- [set_idle_time](set_idle_time.md): sets it
- [sgcl::async::blocking_pool](../blocking_pool.md)
