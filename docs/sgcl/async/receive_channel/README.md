[sgcl](../../README.md) › [async](../README.md)

# sgcl::async::receive_channel\<T\>

```cpp
#include "sgcl/async/channel.h"   // or "sgcl/async.h"

namespace sgcl::async {
    template<class T>
    class receive_channel;

    template<>
    class receive_channel<void>;
}
```

`sgcl::async::receive_channel<T>` is a [channel](../channel/README.md) seen from its receiving end, Go's `<-chan T`: a handle
of the same channel as the one it was made from, with the receive, its case of a select and the looks, and with no
send and no close. A `channel<T>` converts to it, as a Go channel converts to its receive-only type, so a function
that only receives takes a `receive_channel<T>` and is given either.

The library hands one out where the program only listens: [stop_token::channel](../stop_token/channel.md) is the
receiving end of the stop's channel, which only the stop may close. Through a full handle a `close()` would read
as a stop that stopped nothing, neither the children of the source nor its deadline.

## Rules

- A `receive_channel` is a handle: one word, the tracked word of the channel's state, as the
  [channel](../channel/README.md)'s handle is; `==` says whether two are the same channel, a `channel<T>` compared with it
  included. It is made from a channel, or copied from another; there is no empty one. It lives where a channel
  handle lives.
- [receive](receive.md) returns an [operation](../operation/README.md), carried out by `co_await` in a task or
  by `.wait()` on a thread ([README: Waiting operations](../README.md#waiting-operations)); everything else is the
  channel's, done the same way.

## Template parameters

| Parameter | Description |
|---|---|
| `T` | The type of the elements, as for [channel](../channel/README.md). `receive_channel<void>` is a [specialization](#specializations). |

## Member types

| Type | Definition |
|---|---|
| `value_type` | `T` |
| `size_type` | `size_t` |
| `iterator` | the channel's input iterator, which receives on each step ([begin](begin.md)) |
| `receive_op` | the awaiter of a receive in a task ([receive](receive.md)) |
| `receive_case<F>` | a receive case of a select ([on_receive](on_receive.md)) |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](receive_channel.md) | makes the receiving end of a channel, or another handle of it |
| `(destructor)` | lets go of the handle; the state is the collector's once no handle holds it |
| [operator=](operator_assign.md) | makes the handle one of another channel's receiving end |

#### Operations

| Function | Description |
|---|---|
| [receive](receive.md) | receives an element, waiting for one |
| [try_receive](try_receive.md) | receives an element if one is there |

#### Select cases

| Function | Description |
|---|---|
| [on_receive](on_receive.md) | a case of a select served by an element or by the close |

#### Iterators

| Function | Description |
|---|---|
| [begin](begin.md) | receives the first element, for a range-for |
| [end](end.md) | the end iterator |

#### Capacity

| Function | Description |
|---|---|
| [empty](empty.md) | checks whether no element is in the buffer and no sender waits |
| [size](size.md) | the number of elements in the buffer |
| [capacity](capacity.md) | the number of elements the buffer holds; 0 for a rendezvous |

#### Observers

| Function | Description |
|---|---|
| [closed](closed.md) | checks whether the channel is closed |

## Non-member functions

| Function | Description |
|---|---|
| [operator==, operator!=](operator_cmp.md) | checks whether two handles are of the same channel |

## Specializations

`receive_channel<void>` is the receiving end of a [channel](../channel/README.md)`<void>`, a channel of signals: its
[receive](receive.md) gives `true` for a signal and `false` once the channel is closed and drained,
[try_receive](try_receive.md) whether a signal was there, and the body of its
[on_receive](on_receive.md) takes no argument. It has no `iterator`, `begin`, `end` or
`receive_case`; the rest is the primary's.

## Complexity

The channel's: a receive that does not wait is one compare-exchange on its ring, one that waits allocates its
waiter, one managed object. Making a `receive_channel` copies one tracked word.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<int> sum(async::receive_channel<int> numbers) {  // only receives
    int total = 0;
    while (auto n = co_await numbers.receive()) {
        total += *n;
    }
    co_return total;
}

int main() {
    async::channel<int> numbers(4);
    async::task<int> summing = async::spawn(sum(numbers));  // the channel converts
    for (int i : range(1, 5)) {
        numbers.send(i).wait();
    }
    numbers.close();
    println("{}", summing.wait());
}
```

Output:

```text
10
```

## See also

- [channel](../channel/README.md): the channel with both ends
- [stop_token::channel](../stop_token/channel.md): the receiving end of a stop
- [select](../select.md): a wait on several channels at once
- [README: Handles](../README.md#handles), [README: Waiting operations](../README.md#waiting-operations)
