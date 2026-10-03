[sgcl](../../README.md) › [concurrent](../README.md) › [copy_on_write](README.md)

# sgcl::concurrent::copy_on_write\<T\>::update

```cpp
template<class F>
snapshot update(F&& f);
```

Changes the value: `f(T&)` is called on a copy of the current value, a managed object of its own, which then
replaces the current one with a compare-exchange. When another writer got in between, the exchange fails, and the
copy is made again from the value that writer installed and `f` called again, after a backoff.

## Parameters

| Parameter | Description |
|---|---|
| `f` | the change, called as `f(value)` with a `T&` to the copy; called once or more |

## Return value

A snapshot of the value installed.

## Complexity

The copy of the value and the call of `f`, once per attempt; an attempt is lost only to another writer's
exchange.

## Exceptions

What the copy of `T` or `f` throws.

If an exception is thrown, the value is as it was: the copy being changed is nobody else's.

## Notes

Lock-free: some writer's exchange always succeeds. `f` runs on copies nobody else sees, possibly more than once,
so it should be a change of its argument and nothing else, without effects outside it. A lost exchange backs off
exponentially before the next copy, as the [stack](../stack/README.md) does at its head: sixteen writers at one value
took 500 to 600 ns per update without the backoff and 20 with it, measured.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::copy_on_write<vector<int>> listeners(vector<int>{});
    vector<thread> writers;
    for (int w : range(4)) {
        writers.emplace_back([&listeners, w] {
            for (int i : range(250)) {
                listeners.update([&](vector<int>& v) { v.push_back(w * 250 + i); });
            }
        });
    }
    for (auto& t : writers) {
        t.join();
    }
    println("{} listeners", listeners.load()->size());

    auto installed = listeners.update([](vector<int>& v) { v.clear(); });
    println("{} listeners", installed->size());
}
```

Output:

```text
1000 listeners
0 listeners
```

## See also

- [compare_exchange](compare_exchange.md): the same exchange, the change decided by the caller
- [store](store.md): replaces the value without looking at it
- [sgcl::concurrent::copy_on_write\<T\>](README.md)
