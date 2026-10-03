[sgcl](../../README.md) › [core](../README.md) › [frame_ptr](../frame_ptr.md)

# sgcl::frame_ptr\<Promise\>::operator=

```cpp
frame_ptr& operator=(frame_ptr&& o) noexcept;       // (1)
frame_ptr& operator=(const frame_ptr&) = delete;    // (2)
```

1. Destroys the coroutine held, if any ([destroy](destroy.md)), then takes the frame and the handle of `o` over; `o`
   is empty after. An assignment to itself does nothing.
2. Not copyable.

## Parameters

| Parameter | Description |
|---|---|
| `o` | the `frame_ptr` to take the coroutine from |

## Return value

`*this`.

## Complexity

Constant, plus the destructors of the old coroutine's locals and promise.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <coroutine>
#include <utility>

using namespace sgcl;

struct Noisy {
    string name;
    ~Noisy() {
        println("{} destroyed", name);
    }
};

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

task hold(string name) {
    Noisy local{name};
    co_await std::suspend_always();
}

int main() {
    task a = hold("a");
    task b = hold("b");
    a.frame.resume();  // a suspended with its local alive
    b.frame.resume();
    a.frame = std::move(b.frame);  // a's coroutine destroyed first
    println("{}", bool(b.frame));
}
```

Output:

```text
a destroyed
false
b destroyed
```

## See also

- [destroy](destroy.md): destroys the coroutine
- [sgcl::frame_ptr\<Promise\>](../frame_ptr.md)
