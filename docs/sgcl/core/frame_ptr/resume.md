[sgcl](../../README.md) › [core](../README.md) › [frame_ptr](../frame_ptr.md)

# sgcl::frame_ptr\<Promise\>::resume

```cpp
void resume();
```

Resumes the coroutine on the calling thread: it runs until it next suspends or ends. Precondition: the `frame_ptr`
is not empty and the coroutine is suspended and not [done](done.md), as for `std::coroutine_handle::resume`.

## Parameters

None.

## Return value

None.

## Complexity

What the coroutine runs until its next suspension.

## Exceptions

An exception the coroutine lets out of its promise's `unhandled_exception` propagates from `resume()`.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <coroutine>

using namespace sgcl;

struct task {
    struct promise_type : managed_frame {
        task get_return_object() {
            auto h = std::coroutine_handle<promise_type>::from_promise(*this);
            return task{frame_ptr<promise_type>(h)};
        }
        std::suspend_always initial_suspend() noexcept {
            return {};
        }
        std::suspend_always final_suspend() noexcept {
            return {};
        }
        void return_void() noexcept {
        }
        void unhandled_exception() {
            throw;  // out of resume()
        }
    };
    frame_ptr<promise_type> frame;
};

task two_steps() {
    println("step 1");
    co_await std::suspend_always();
    println("step 2");
    throw runtime_error("failed");
}

int main() {
    task t = two_steps();
    t.frame.resume();  // one step per resume, whatever the coroutine co_awaits
    try {
        t.frame.resume();
    } catch (const runtime_error& e) {
        println("{}", e.what());
    }
}
```

Output:

```text
step 1
step 2
failed
```

## See also

- [done](done.md): checks whether there is anything left to resume
- [sgcl::frame_ptr\<Promise\>](../frame_ptr.md)
