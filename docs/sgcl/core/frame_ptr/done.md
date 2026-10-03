[sgcl](../../README.md) › [core](../README.md) › [frame_ptr](../frame_ptr.md)

# sgcl::frame_ptr\<Promise\>::done

```cpp
bool done() const noexcept;
```

Checks whether there is nothing to resume: the `frame_ptr` is empty, or the coroutine is suspended at its final
suspend point.

## Parameters

None.

## Return value

`true` when the `frame_ptr` is empty or the coroutine has ended, `false` otherwise.

## Complexity

Constant.

## Exceptions

None.

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
        void unhandled_exception() noexcept {
        }
    };
    frame_ptr<promise_type> frame;
};

task three_steps() {
    for (int i : range(3)) {
        co_await std::suspend_always();
    }
}

int main() {
    task t = three_steps();
    int resumes = 0;
    while (!t.frame.done()) {
        t.frame.resume();
        ++resumes;
    }
    frame_ptr<task::promise_type> empty;
    println("{} {}", resumes, empty.done());
}
```

Output:

```text
4 true
```

## See also

- [resume](resume.md): resumes the coroutine
- [operator bool](operator_bool.md): checks whether there is a coroutine
- [sgcl::frame_ptr\<Promise\>](../frame_ptr.md)
