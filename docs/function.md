# sgcl::function, sgcl::move_only_function

```cpp
#include "sgcl/function.h"   // or "sgcl/sgcl.h"

namespace sgcl {
    template<class Signature, template<class> class Ptr = tracked_ptr>
    class function;              // function<R(Args...)>
    template<class Signature, template<class> class Ptr = tracked_ptr>
    class move_only_function;    // R(Args...), R(Args...) const, R(Args...) noexcept, R(Args...) const noexcept
}
```

`sgcl::function<R(Args...)>` is `std::function` for a closure that captures tracked pointers. `std::function` keeps a small closure in a buffer inside itself, where a `tracked_ptr` would share its word with the data of other closures (the offset leaves the collector's pointer map by elimination: [README: Pointer maps](../README.md#pointer-maps)), and a large one on the unmanaged heap, where a `tracked_ptr` may not live; so a `std::function` may not capture one ([The rules](../README.md#the-rules), 1). Here a closure goes to one of two places by what it is: a small one that cannot hold a pointer (16 bytes at most, trivially default constructible: a function pointer, a captureless lambda, a lambda capturing ints or references) into a buffer inside the `function`; any other, a closure capturing a `tracked_ptr` or a `weak_ptr` first of all, into a managed node of its own, held by a pointer in a word of the `function` and traced through its own pointer map, so that a closure capturing the object that holds the `function` is a cycle collected like any other; the closure is destroyed the moment the `function` drops it, on that thread, as a container destroys an erased element, and the node is reclaimed by the collector later. The word holds null or an address and nothing else; 32 bytes, as `std::function`.

The interface is that of `std::function`: the constructors (a null function pointer, a null member pointer or an empty function of either library make an empty one), the assignments, `std::reference_wrapper`, `swap`, `operator bool`, the call (`bad_function_call`, the one of `std`, on an empty function; the callable is called as an lvalue, as `std::function` calls it), `target_type`, `target<T>`, the deduction guides from a function pointer and from a functor's `operator()`, `==` with `nullptr`. A copy of a closure in a node is a node of its own. A callable larger than a page is not supported.

`move_only_function<Signature>` is `std::move_only_function` over the same storage: the callable need not be copyable (a lambda capturing a `unique_ptr`), the signature's `const` and `noexcept` are honoured (`R(Args...) const` is callable through a `const move_only_function&`, `R(Args...) noexcept` makes the call `noexcept`; the reference qualifiers `&` and `&&` are not supported), `in_place_type`, and calling an empty one is undefined (debug builds assert).

`Ptr` is the kind of the word, and so where the `function` lives, as for the containers: `sgcl::function` on a stack or inside a managed object, `gc::function` and `gc::move_only_function` ([gc/gc.h](README.md#the-gc-namespace)) anywhere, a `std::vector` of callbacks included. What the closure captures follows the rules of its type where the `function` lives, as a member would: an `sgcl::tracked_ptr` captured by a `gc::function` on the unmanaged heap is the mistake it would be in a `std::vector`; capture a `gc::tracked_ptr` there. [`expiry_queue`](expiry_queue.md) takes its function as a `function` of the queue's kind: an entry's function may capture the objects it works on.

## Rules

- An `sgcl::function` lives where a `tracked_ptr` may, a `gc::function` anywhere ([The rules](../README.md#the-rules), 1); the closure follows the rules of its captures where the `function` lives: an `sgcl::tracked_ptr` captured by a `gc::function` on the unmanaged heap is the mistake it would be as a member; capture a `gc::tracked_ptr` there.
- A closure in a node is destroyed by `reset`, an assignment or the destructor, at once, on the calling thread; the objects it captured are unreferenced from then on and die with the next cycle that finds them so.
- A closure in a node is traced: one that captures a strong pointer to the object holding the `function` is a cycle, collected when nothing else reaches it; one that captures a strong pointer to an object an [`expiry_queue`](expiry_queue.md) watches keeps that object alive.
- Thread safety is that of `std::function` ([The rules](../README.md#the-rules), 6).

## Members

```cpp
using result_type = R;

function() noexcept;
function(std::nullptr_t) noexcept;
function(const function&);
function(function&&) noexcept;
template<class F> function(F&& f);                    // decay_t<F> invocable as R(Args...), copy constructible
~function();

function& operator=(const function&);
function& operator=(function&&) noexcept;
function& operator=(std::nullptr_t) noexcept;
template<class F> function& operator=(F&& f);
template<class F> function& operator=(std::reference_wrapper<F> f) noexcept;

void swap(function&) noexcept;
explicit operator bool() const noexcept;
R operator()(Args... args) const;                     // bad_function_call when empty
const std::type_info& target_type() const noexcept;   // typeid(void) when empty
template<class T> T* target() noexcept;
template<class T> const T* target() const noexcept;
```

The free functions, in `sgcl`:

```cpp
template<class R, class... Args, template<class> class Ptr> void swap(function<R(Args...), Ptr>&, function<R(Args...), Ptr>&) noexcept;
template<class R, class... Args, template<class> class Ptr> bool operator==(const function<R(Args...), Ptr>&, std::nullptr_t) noexcept;
template<class R, class... Args> function(R (*)(Args...)) -> function<R(Args...)>;
template<class F> function(F) -> function</* the signature of F::operator() */>;
```

`move_only_function<Signature>` has the same members without the copies, `target_type` and `target`, plus `explicit move_only_function(std::in_place_type_t<T>, Args&&...)` (and with an `initializer_list`); its `operator()` carries the signature's `const` and `noexcept`.

```cpp
struct Node { int value; };
gc::tracked_ptr node = gc::make_tracked<Node>(1);
gc::function<int()> f = [node] { return node->value; };   // a closure with a pointer: in a managed node of its own
gc::function<int(int)> g = [](int x) { return x + 1; };   // no pointers: inside the function
gc::function<int(int)> h = g;
assert(f() == 1 && g(1) == 2 && h(1) == 2 && !gc::function<void()>());
assert(f.target_type() != typeid(void) && g.target<int (*)(int)>() == nullptr);
node = nullptr;                                            // f still holds the Node
gc::move_only_function<int() const> m = [p = std::make_unique<int>(2)] { return *p; };   // a move-only closure
assert(m() == 2);
```

## Example

```cpp
#include "sgcl/sgcl.h"
#include <iostream>
#include <string>

// An event with listeners: each listener a function capturing the object
// it works on, kept in a std::vector on the unmanaged heap, as any
// gc:: type may be. The listener's objects live while the listener does;
// a listener capturing the button that holds it would be a cycle,
// collected with the button.
struct Label {
    std::string text;
};

struct Button {
    std::vector<gc::function<void(const std::string&)>> on_click;   // gc::function: lives anywhere
    void click(const std::string& what) {
        for (auto& f : on_click) {
            f(what);
        }
    }
};

int main() {
    Button button;
    gc::tracked_ptr label = gc::make_tracked<Label>();
    button.on_click.push_back([label](const std::string& what) { label->text = "clicked " + what; });   // the closure in a managed node: label followed
    button.on_click.push_back([](const std::string& what) { std::cout << "log: " << what << "\n"; });   // no pointers: inside the function
    gc::tracked_ptr<Label> seen = label;
    label = nullptr;                                     // the listener keeps the label
    gc::collector::force_collect(true);                  // optional, for the demonstration only
    button.click("ok");
    std::cout << seen->text << "\n";                     // clicked ok
    button.on_click.clear();                             // the closures destroyed now; the label lives on through seen
    return 0;
}
```

## See also

- [any](any.md): the same storage for a value; [expiry_queue](expiry_queue.md): where a `function` runs with the object alive one last time
- [tracked_ptr](tracked_ptr.md), [weak_ptr](weak_ptr.md), [unique_ptr](unique_ptr.md)
- README: [variant, any, function and expected](../README.md#variant-any-function-and-expected), [Pointer maps](../README.md#pointer-maps), [The rules](../README.md#the-rules)
- `tests/function.cpp`: every behaviour above, checked.
