# Sgcl::Function

```cpp
#include "sgcl/Sgcl/Core/Function.h"   // or "sgcl/Sgcl/Sgcl.h"

namespace Sgcl {
    template<class Signature>
    class Function;             // R(Args...)
    template<class Signature>
    class MoveOnlyFunction;     // R(Args...), R(Args...) const, R(Args...) noexcept, R(Args...) const noexcept
}
```

The same classes in the `sgcl` interface: [function, move_only_function](../../core/function.md).

`Function<R(Args...)>` is `std::function` for a closure that captures tracked pointers. `std::function` keeps a small closure in a buffer inside itself, where a `Ptr` would share its word with the data of other closures (the offset leaves the collector's pointer map by elimination: [README: Pointer maps](../../../garbage_collector/overview.md#pointer-maps)), and a large one on the unmanaged heap, where a `Ptr` may not live; so a `std::function` may not capture one ([The rules](../../core/README.md#the-rules), 1). Here a closure goes to one of two places by what it is: a small one that cannot hold a pointer (16 bytes at most, and trivially default constructible, or smaller than a word, or aligned under a word: a function pointer, a captureless lambda, a lambda capturing ints, a `std::reference_wrapper`) into a buffer inside the `Function`; any other, a closure capturing a `Ptr` or a `WeakPtr` first of all, but also one capturing a reference, a raw pointer or a `double` (a closure has no default constructor, so the collector cannot rule a pointer out of a word aligned as one), into a managed node of its own, held by a pointer in a word of the `Function` and traced through its own pointer map, so that a closure capturing the object that holds the `Function` is a cycle collected like any other; the closure is destroyed the moment the `Function` drops it, on that thread, as a container destroys a removed element, and the node is reclaimed by the collector later. The word holds null or an address and nothing else; 32 bytes, as `std::function`.

The interface is that of `std::function` under the interface's names: the constructors (a null function pointer, a null member pointer or an empty function make an empty one), the assignments, `std::reference_wrapper`, `Swap`, `operator bool`, the call (`bad_function_call`, the one of `std`, on an empty function; the callable is called as an lvalue, as `std::function` calls it), `TargetType`, `Target<T>`, the deduction guides from a function pointer and from a functor's `operator()`, `==` with `nullptr`. A copy of a closure in a node is a node of its own. A callable larger than a page is not supported.

`MoveOnlyFunction<Signature>` is `std::move_only_function` over the same storage: the callable need not be copyable (a lambda capturing a `UniquePtr`), an empty `MoveOnlyFunction` of another signature or an empty function makes an empty one, the signature's `const` and `noexcept` are honoured (`R(Args...) const` is callable through a `const MoveOnlyFunction&`, `R(Args...) noexcept` makes the call `noexcept`; the reference qualifiers `&` and `&&` are not supported), `in_place_type`, and calling an empty one is undefined (debug builds assert).

The word is a `Ptr`, so a `Function` lives where one may, as the containers do: on a stack or inside a managed object. What the closure captures follows the rules of its type where the `Function` lives, as a member would. [`ExpiryQueue`](../Containers/ExpiryQueue.md) takes its function as a `Function`: an entry's function may capture the objects it works on.

## Rules

- A `Function` lives where a `Ptr` may ([The rules](../../core/README.md#the-rules), 1); the closure follows the rules of its captures where the `Function` lives.
- A closure in a node is destroyed by an assignment or the destructor, at once, on the calling thread; the objects it captured are unreferenced from then on and die with the next cycle that finds them so.
- A closure in a node is traced: one that captures a strong pointer to the object holding the `Function` is a cycle, collected when nothing else reaches it; one that captures a strong pointer to an object an [`ExpiryQueue`](../Containers/ExpiryQueue.md) watches keeps that object alive.
- Thread safety is that of `std::function` ([The rules](../../core/README.md#the-rules), 6).

## Members

```cpp
using ResultType = R;
using InnerType = sgcl::function<R(Args...)>;

Function() noexcept;
Function(std::nullptr_t) noexcept;
Function(const Function&);
Function(Function&&) noexcept;
template<class F> Function(F&& f);                    // decay_t<F> invocable as R(Args...), copy constructible
explicit Function(InnerType f) noexcept;
~Function();

Function& operator=(const Function&);
Function& operator=(Function&&) noexcept;
Function& operator=(std::nullptr_t) noexcept;
template<class F> Function& operator=(F&& f);
template<class F> Function& operator=(std::reference_wrapper<F> f) noexcept;

void Swap(Function&) noexcept;
explicit operator bool() const noexcept;
R operator()(Args... args) const;                     // bad_function_call when empty
const std::type_info& TargetType() const noexcept;    // typeid(void) when empty
template<class T> T* Target() noexcept;
template<class T> const T* Target() const noexcept;
InnerType& Inner() noexcept;
const InnerType& Inner() const noexcept;
```

The free functions, in `Sgcl`:

```cpp
template<class R, class... Args> void swap(Function<R(Args...)>&, Function<R(Args...)>&) noexcept;
template<class R, class... Args> bool operator==(const Function<R(Args...)>&, std::nullptr_t) noexcept;
template<class R, class... Args> Function(R (*)(Args...)) -> Function<R(Args...)>;
template<class F> Function(F) -> Function</* the signature of F::operator() */>;
```

`MoveOnlyFunction<Signature>` has the same members without the copies, `TargetType` and `Target`, plus `explicit MoveOnlyFunction(std::in_place_type_t<T>, Args&&...)`; its `operator()` carries the signature's `const` and `noexcept`.

```cpp
struct Node { int value; };
Ptr node = Make<Node>(1);
Function<int()> f = [node] { return node->value; };   // a closure with a pointer: in a managed node of its own
Function<int(int)> g = [](int x) { return x + 1; };   // no pointers: inside the function
Function<int(int)> h = g;
assert(f() == 1 && g(1) == 2 && h(1) == 2 && !Function<void()>());
assert(f.TargetType() != typeid(void) && g.Target<int (*)(int)>() == nullptr);
node = nullptr;                                                // f still holds the Node
MoveOnlyFunction<int() const> m = [p = std::make_unique<int>(2)] { return *p; };   // a move-only closure
assert(m() == 2);
```

## Example

```cpp
#include "sgcl/Sgcl/Sgcl.h"
#include <iostream>

// An event with listeners: each listener a function capturing the object
// it works on, kept in a List where a tracked pointer may live. The
// listener's objects live while the listener does; a listener capturing
// the button that holds it would be a cycle, collected with the button.
struct Label {
    String text;
};

struct Button {
    List<Function<void(const String&)>> onClick;   // the closures in managed nodes
    void Click(const String& what) {
        for (auto& f : onClick) {
            f(what);
        }
    }
};

int main() {
    Button button;
    Ptr label = Make<Label>();
    button.onClick.Add([label](const String& what) { label->text = "clicked " + what; });   // the closure in a managed node: label followed
    button.onClick.Add([](const String& what) { std::cout << "log: " << what << "\n"; });   // no pointers: inside the function
    Ptr<Label> seen = label;
    label = nullptr;                                     // the listener keeps the label
    Collector::Collect(true);                   // optional, for the demonstration only
    button.Click("ok");
    std::cout << seen->text << "\n";                     // clicked ok
    button.onClick.Clear();                              // the closures destroyed now; the label lives on through seen
    return 0;
}
```

The output:

```
log: ok
clicked ok
```

## See also

- [Any](Any.md): the same storage for a value; [ExpiryQueue](../Containers/ExpiryQueue.md): where a `Function` runs with the object alive one last time
- [Ptr](Ptr.md), [WeakPtr](WeakPtr.md), [UniquePtr](UniquePtr.md)
- README: [variant, any, function and expected](../../core/README.md#variant-any-function-and-expected), [Pointer maps](../../../garbage_collector/overview.md#pointer-maps), [The rules](../../core/README.md#the-rules)
