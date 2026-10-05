[sgcl](../../README.md) › [async](../README.md)

# sgcl::async::channel\<T\>

```cpp
#include "sgcl/async/channel.h"   // or "sgcl/async.h"

namespace sgcl::async {
    template<class T>
    class channel;

    template<>
    class channel<void>;
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::async::channel<T>` is the channel of Go: a queue with the synchronization of both ends. A channel of
capacity *n* buffers *n* elements; one of capacity 0 buffers none, and a send waits until a receive takes the
element, so that the pair is a meeting of the two sides (a rendezvous) and not a delivery to a buffer. A receive on
an empty channel waits, a send on a full one waits: a producer ahead of its consumer stops (back-pressure).
[close](close.md) ends the stream: what was sent is still received, then every receive gives nothing at
once and every send gives `false`; a range-for over the channel runs until then.

The waiting is done by a thread, on an atomic of its own, or by a coroutine: `co_await ch.receive()` and
`co_await ch.send(v)` suspend the coroutine with its handle on the channel's list of waiters, and the send or the
receive that serves it hands it to the [scheduler](../scheduler/README.md), which runs it on a worker; the serving thread
returns at once. A [select](../select.md) waits on several channels at once, each through a case
([on_receive](on_receive.md), [on_send](on_send.md)).

The buffer is a lock-free ring, the bounded queue of Vyukov: an array of slots made once, a sequence number per
slot, one compare-exchange per send or receive and no allocation per element. The ring has a power of two of slots,
at least the capacity, and the capacity is enforced apart (a channel of 3 has a ring of 4). The lists of waiters are
[concurrent::queue](../../concurrent/queue/README.md)s, and every waiter is a managed object: nothing in the channel takes a
lock, nothing frees anything, and a waiter that was cancelled or served is reclaimed by the collector
([README: Lock-free containers](../../concurrent/README.md#lock-free-containers)). A rendezvous has a small ring too,
through which a waiting sender's element passes to the receiver that serves it, so that waiting senders are served
in their order. A buffered channel keeps a count of the entries of each list beside it, so that a send that has
pushed and a receive that has popped, with nobody waiting, which is the common case, look at one word rather than
at the head of a queue; a rendezvous, which serves every element through the lists, keeps none.

What differs from Go: a send to a closed channel gives `false` where Go panics, a receive from a closed and drained
one gives an empty `optional` where Go gives the zero value and `false`, and there is no nil channel: every channel
is made by its constructor. `size()` and `capacity()` are Go's `len` and `cap`.

## Rules

- A channel is a handle: one word, a tracked word to the channel's state (the ring, the lists), which copies share; `==`
  says whether two are the same channel. It is made by the constructor, `channel<T> ch;` a rendezvous and
  `channel<T> ch(n)` a buffer of *n*; there is no empty channel. A task takes it as a parameter by value,
  `async::task<> worker(async::channel<Job> jobs)`, the copy keeping the channel for as long as the task runs.
- [send](send.md) and [receive](receive.md) return an [operation](../operation/README.md), carried out by
  `co_await` in a task or by `.wait()` on a thread ([README: Waiting operations](../README.md#waiting-operations)).
  They wait only for the other side; [try_send](try_send.md) and [try_receive](try_receive.md) never
  wait. Every operation is lock-free on the channel's side.
- Elements are delivered in the order each sender sent them; between senders sending at the same moment the order
  is theirs.
- A coroutine that awaits a channel must have a managed frame (a [task](../task/README.md), or a promise derived from
  `managed_frame`; a `static_assert` says so otherwise), so that the tracked pointers of its frame are roots while it
  waits ([README: Coroutines](../README.md#coroutines)). The channel's waiter holds the frame for the length of the
  wait, so a task nobody holds may wait.
- A coroutine's waiter is on the list before the coroutine looks at the channel one last time (for a send or a
  receive that came meanwhile), and is published to the serving side only after that look: nothing serves it before,
  so the coroutine cannot be resumed, finished and its channel destroyed while the look still reads it. A thread's
  wait has no such window, since the thread blocks and its channel stays. The thread that serves a coroutine's wait
  makes it ready on the scheduler and goes on; a worker runs the coroutine.
- A send to a closed channel gives `false`; a send waiting when the channel closes gives `false` with its element
  undelivered. A second `close()` does nothing.

## Template parameters

| Parameter | Description |
|---|---|
| `T` | The type of the elements: an object type that is move-constructible; the overloads that take a `const T&` copy it. `channel<void>` is a [specialization](#specializations). |

## Member types

| Type | Definition |
|---|---|
| `value_type` | `T` |
| `size_type` | `size_t` |
| `iterator` | an input iterator that receives on each step, the end iterator once the channel is closed and drained ([begin](begin.md)) |
| `receive_op` | the awaiter of a receive in a task ([receive](receive.md)) |
| `send_op` | the awaiter of a send in a task ([send](send.md)) |
| `receive_case<F>` | a receive case of a select ([on_receive](on_receive.md)) |
| `send_case<F>` | a send case of a select ([on_send](on_send.md)) |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](channel.md) | makes a channel, or another handle of the same channel |
| `(destructor)` | lets go of the handle; the state is the collector's once no handle holds it |
| [operator=](operator_assign.md) | makes the handle one of another channel |

#### Operations

| Function | Description |
|---|---|
| [send](send.md) | sends an element, waiting for room or for a receiver |
| [try_send](try_send.md) | sends an element if it can without waiting |
| [receive](receive.md) | receives an element, waiting for one |
| [try_receive](try_receive.md) | receives an element if one is there |
| [close](close.md) | ends the stream |

#### Select cases

| Function | Description |
|---|---|
| [on_receive](on_receive.md) | a case of a select served by an element or by the close |
| [on_send](on_send.md) | a case of a select served when the element is delivered |

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

`channel<void>` is a channel of signals: it carries nothing but the fact of a send, for a signal of readiness or a
cancellation. Its [send](send.md), [try_send](try_send.md), [on_send](on_send.md) take no
element, its [receive](receive.md) gives `true` for a signal and `false` once the channel is closed and
drained, [try_receive](try_receive.md) whether a signal was there, and the body of its
[on_receive](on_receive.md) takes no argument and is called for a signal and for the close alike. It has no
`iterator`, `begin`, `end`, `send_op`, `receive_case` or `send_case`; the rest (`close`, `closed`, `capacity`,
`size`, `empty`, `==`, the constructors and the assignments) is the primary's.

## Complexity

- A send or a receive that does not wait: constant, one compare-exchange on the ring, and no allocation; plus the
  wake of a waiter it serves.
- A send or a receive that waits: one managed object, the waiter, pushed on a list.
- `close`: linear in the number of waiters, each woken.

A channel of a capacity up to 8 of small elements (a slot of at most 64 bytes, a rendezvous included) has its ring
in its state: one managed object for the channel. A larger ring is an array of its own.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<> produce(async::channel<int> numbers) {  // by value: the same channel
    for (int i : range(1, 6)) {
        co_await numbers.send(i);  // suspends while the buffer is full
    }
    numbers.close();
}

async::task<> square(async::channel<int> numbers, async::channel<int> squares) {
    while (auto n = co_await numbers.receive()) {
        co_await squares.send(*n * *n);
    }
    squares.close();  // numbers closed and drained: the stream ends downstream
}

int main() {
    async::channel<int> numbers(2);
    async::channel<int> squares;  // a rendezvous
    async::go(produce(numbers));
    async::go(square(numbers, squares));

    vector<int> received;
    for (int s : squares) {  // the thread waits until squares is closed
        received.push_back(s);
    }
    println("{}", received);
}
```

Output:

```text
[1, 4, 9, 16, 25]
```

## See also

- [select](../select.md): a wait on several channels at once
- [receive_channel](../receive_channel/README.md): the receiving end alone, for a function that only receives
- [broadcast](../broadcast/README.md): every subscriber receives every value
- [promise](../promise/README.md), [event](../event/README.md): a completion set once
- [concurrent::queue](../../concurrent/queue/README.md): the lists of waiters;
  [concurrent::bounded_queue](../../concurrent/bounded_queue/README.md): a ring without the waiting
- [task](../task/README.md), [scheduler](../scheduler/README.md): the tasks that await a channel and what runs them
- [README: Handles](../README.md#handles), [README: Waiting operations](../README.md#waiting-operations)
