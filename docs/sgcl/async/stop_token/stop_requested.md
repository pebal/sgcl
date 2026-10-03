[sgcl](../../README.md) › [async](../README.md) › [stop_token](README.md)

# sgcl::async::stop_token::stop_requested

```cpp
bool stop_requested() const noexcept;
```

Checks whether the stop has been requested: the closed flag of the token's channel, one load. A token with no
source is never stopped. It is the look of a loop that computes, between its steps; a task that waits uses
[on_stop](on_stop.md) or [stopped](stopped.md) instead, which end the wait itself.

## Parameters

None.

## Return value

`true` when the source of the token, or a parent of that source, has requested the stop; `false` otherwise and
for a token with no source.

## Complexity

Constant: one load.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    async::stop_source source;
    async::stop_token token = source.token();
    thread computing([token] {
        while (!token.stop_requested()) {  // a look between the steps of the work
            this_thread::yield();
        }
    });
    println("{}", token.stop_requested());
    source.request_stop();
    computing.join();
    println("{}", token.stop_requested());
    println("{}", async::stop_token().stop_requested());
}
```

Output:

```text
false
true
false
```

## See also

- [stop_source::request_stop](../stop_source/request_stop.md): the stop requested
- [on_stop](on_stop.md), [stopped](stopped.md): waits that the stop ends
- [sgcl::async::stop_token](README.md)
