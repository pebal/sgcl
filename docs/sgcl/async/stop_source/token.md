[sgcl](../../README.md) › [async](../README.md) › [stop_source](../stop_source.md)

# sgcl::async::stop_source::token

```cpp
stop_token token() const noexcept;
```

Returns a token of this source: what a task is given to see the stop with, and what a child source is made from.
Every token of a source shares its state, and equals the others; a token keeps the state alive as the source does.

## Parameters

None.

## Return value

A [stop_token](../stop_token.md) of this source.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<string> wait_for_stop(async::stop_token token) {
    co_await token.stopped();
    co_return "stopped";
}

int main() {
    async::stop_source source;
    async::stop_token token = source.token();
    println("{} {}", token.stop_possible(), token == source.token());
    auto t = async::spawn(wait_for_stop(token));
    source.request_stop();
    println("{}", t.wait());
}
```

Output:

```text
true true
stopped
```

## See also

- [stop_token](../stop_token.md): what is returned
- [(constructor)](stop_source.md): a child made from a token
- [sgcl::async::stop_source](../stop_source.md)
