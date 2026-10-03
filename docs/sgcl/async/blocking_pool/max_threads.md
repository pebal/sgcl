[sgcl](../../README.md) › [async](../README.md) › [blocking_pool](README.md)

# sgcl::async::blocking_pool::max_threads

```cpp
static unsigned max_threads();
```

Returns the most threads the pool grows to: the number [set_threads](set_threads.md) set, else
`SGCL_BLOCKING_THREADS` from the environment (read once, at the first need), else `config::blocking_threads`; 0 in
any of them is resolved to the larger of 64 and four times the hardware concurrency.

## Parameters

None.

## Return value

The cap, resolved: never 0.

## Complexity

Constant.

## Exceptions

`std::system_error` when the pool's lock cannot be taken, as `std::mutex::lock` reports it.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    println("the default is at least 64: {}", async::blocking_pool::max_threads() >= 64);
    async::blocking_pool::set_threads(8);
    println("{}", async::blocking_pool::max_threads());
}
```

Output:

```text
the default is at least 64: true
8
```

## See also

- [set_threads](set_threads.md): sets the cap
- [get_statistics](get_statistics.md): the threads there are
- [sgcl::async::blocking_pool](README.md)
