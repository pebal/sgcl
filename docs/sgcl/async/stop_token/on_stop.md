[sgcl](../../README.md) › [async](../README.md) › [stop_token](../stop_token.md)

# sgcl::async::stop_token::on_stop

```cpp
template<class F>
auto on_stop(F f) const noexcept(std::is_nothrow_move_constructible_v<F>);
```

Returns a case of a [select](../select.md) served by the stop, with `f` as its body: `token.on_stop([&] {
running = false; })` beside the receive it bounds, so that a wait is cancelled the way anything else is waited
for. When the stop is requested, every select waiting with this case is served by it and runs `f()`; a select
that starts after the stop is served at once (unless another case is ready too: the select takes the first
ready). It is the receive case of the token's [channel](channel.md), which the stop closes.

The token must have a source (an assertion in debug builds): [stop_possible](stop_possible.md) says.

## Parameters

| Parameter | Description |
|---|---|
| `f` | the body: called with no arguments when the case is served |

## Return value

The case, to give to `select`: `co_await async::select(..., token.on_stop(f))` in a task,
`async::select(..., token.on_stop(f)).wait()` on a thread. The select gives the index of the case it served.

## Complexity

Constant.

## Exceptions

What the move constructor of `F` throws; none when it is noexcept. What `f` throws when the case is served goes
out of the select.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<string> wait_for_reply(async::channel<int> replies, async::stop_token token) {
    string result;
    co_await async::select(
        replies.on_receive([&](int v) { result = "reply " + to_string(v); }),
        token.on_stop([&] { result = "stopped"; })
    );
    co_return result;
}

int main() {
    async::stop_source source;
    async::channel<int> replies;
    auto t = async::spawn(wait_for_reply(replies, source.token()));
    source.request_stop();
    println("{}", t.wait());
}
```

Output:

```text
stopped
```

## See also

- [stopped](stopped.md): the wait for the stop alone
- [timeout](../timeout.md): a deadline as a case beside it
- [select](../select.md): what the case is given to
- [sgcl::async::stop_token](../stop_token.md)
