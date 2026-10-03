[sgcl](../../README.md) › [async](../README.md) › [condition_variable](README.md)

# sgcl::async::condition_variable::wait

```cpp
auto wait(mutex::guard& g) noexcept;                          // (1)
template<class Pred>
auto wait(mutex::guard& g, Pred pred)                         // (2)
    noexcept(std::is_nothrow_copy_constructible_v<Pred> &&
             std::is_nothrow_move_constructible_v<Pred>);
template<class Lock>
    requires (!std::is_same_v<Lock, mutex::guard>)
void wait(Lock& lock);                                        // (3)
template<class Lock, class Pred>
    requires (!std::is_same_v<Lock, mutex::guard>)
void wait(Lock& lock, Pred pred);                             // (4)
```

Lets go of the mutex, waits for a notify and takes the mutex back. The waiter is put on the queue before the mutex
is let go of, so no notify made under the mutex after the wait began is lost.

1. With the [guard](../mutex-guard/README.md) of a mutex, which holds it locked. The call does nothing yet; it returns an
   [operation](../operation/README.md), carried out in one of two ways
   ([README: Waiting operations](../README.md#waiting-operations)): `co_await cv.wait(g)` in a task, which waits
   for the notify and for the mutex holding no thread, and `cv.wait(g).wait()` on a thread, which blocks.
2. The same until `pred()` is `true`: the predicate is checked under the mutex before every wait, and a wait is
   made only while it is `false`. The operation keeps a copy of `pred`.
3. A thread's wait with a lock over the mutex that has `unlock()` and `lock()`, such as
   `std::unique_lock<sgcl::async::mutex>`: blocks the thread.
4. The same until `pred()` is `true`, checked under the mutex before every wait.

- (1–4) When the wait returns, the mutex is held again, by the guard or by the lock. There is no spurious wakeup,
  but the condition may have changed between the notify and the mutex taken back: (2) and (4) check it again, and a
  use of (1) or (3) checks it in a loop of its own.

## Parameters

| Parameter | Description |
|---|---|
| `g` | the guard that holds the mutex locked; it holds it again when the wait ends |
| `lock` | a lock over the mutex, locked; locked again when the wait ends |
| `pred` | the condition waited for, called under the mutex with no arguments, returning a value tested as `bool` |

## Return value

- (1–2) An [operation](../operation/README.md). Carried out, by `co_await` or by `.wait()`, it gives nothing, the mutex held
  again.
- (3–4) None.

## Complexity

A wait makes a channel of one signal for its waiter and pushes it on the queue, unlocks the mutex and locks it again
once notified; (2) and (4) as many times as the predicate is `false`. In a task, (1) and (2) carried out make a task
of their own, one frame on the managed heap.

## Exceptions

- (1) The call: none. (2) The call: what the copy or the move constructor of `Pred` throws; none when they are
  noexcept.
- Carried out, and (3–4): what `pred` throws, with the mutex held; `std::system_error` when the unlock wakes a
  waiting task, the wake must start the scheduler's workers and a thread cannot be started
  ([README: The rules](../README.md#the-rules), 5).

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <mutex>

using namespace sgcl;

struct Turn {
    async::mutex lock;
    async::condition_variable changed;
    int next = 0;  // guarded by lock
};

async::task<> player(tracked_ptr<Turn> t, int id) {
    auto guard = co_await t->lock.scoped_lock();
    co_await t->changed.wait(guard, [&] { return t->next == id; });  // a task
    println("player {}", id);
    ++t->next;
    t->changed.notify_all();
}

int main() {
    tracked_ptr t = make_tracked<Turn>();
    vector<async::task<>> players;
    for (int id : {2, 0, 1}) {
        players.push_back(async::spawn(player(t, id)));
    }
    std::unique_lock lock(t->lock);
    t->changed.wait(lock, [&] { return t->next == 3; });  // a thread
    println("all played");
    lock.unlock();
    for (auto& p : players) {
        p.wait();
    }
}
```

Output:

```text
player 0
player 1
player 2
all played
```

## See also

- [notify_one](notify_one.md), [notify_all](notify_all.md): what ends the wait
- [scoped_lock](../mutex/scoped_lock.md): the guard of a task
- [sgcl::async::condition_variable](README.md)
