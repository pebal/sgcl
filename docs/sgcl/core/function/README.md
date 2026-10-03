[sgcl](../../README.md) › [core](../README.md)

# sgcl::function\<R(Args...)\>

```cpp
#include "sgcl/core/function.h"   // or "sgcl/core.h"

namespace sgcl {
    template<class Signature>
    class function;   // undefined

    template<class R, class... Args>
    class function<R(Args...)>;

    using std::bad_function_call;
}
```

`sgcl::function<R(Args...)>` is `std::function` for a closure that captures tracked pointers. `std::function` keeps
a small closure in a buffer inside itself, where a `tracked_ptr` would share its word with the data of other
closures (the offset leaves the collector's pointer map by elimination:
[Pointer maps](../../../garbage_collector/overview.md#pointer-maps)), and a large one on the unmanaged heap, where a
`tracked_ptr` may not live; so a `std::function` may not capture one ([The rules](../README.md#the-rules), 1). Here a
closure goes to one of two places by what it is:

- a small one that cannot hold a pointer into a buffer of 16 bytes inside the `function`: at most 16 bytes, aligned
  to at most 8, moved without throwing, and either trivially copyable (which a closure with a pointer word never
  is), trivially default constructible, smaller than a word or aligned under one, or a `std::reference_wrapper` — a
  function pointer, a captureless lambda, a lambda capturing ints, a `double`, a raw pointer or a reference;
- any other, a closure capturing a `tracked_ptr` or a `weak_ptr` first of all, but also one capturing a `string`, a
  `std::string` or anything else with a copy constructor of its own (a closure has no default constructor, so the
  collector cannot rule a pointer out of such a word), into a managed node of its own, held by a pointer in a word
  of the `function` and traced through its own pointer map, so that a closure capturing the object that holds the
  `function` is a cycle collected like any other.

The closure is destroyed the moment the `function` drops it, on that thread, as a container destroys an erased
element, and the node is reclaimed by the collector later. The word holds null or an address and nothing else; a
`function` is 32 bytes, as `std::function`.

The interface is that of `std::function`: the constructors (a null function pointer, a null member pointer or an
empty function of either library make an empty one), the assignments, `std::reference_wrapper`, `swap`,
`operator bool`, the call (`bad_function_call`, the one of `std`, on an empty function; the callable is called as
an lvalue, as `std::function` calls it), `target_type`, `target<T>`, the deduction guides from a function pointer
and from a functor's `operator()`, `==` with `nullptr`. A copy of a closure in a node is a node of its own. A
callable larger than a page is not supported. A callable that need not be copyable goes into a
[move_only_function](../move_only_function/README.md). A Go `func` value is the same thing in a language where every closure
lives on the collected heap.

## Rules

- The word is a `tracked_ptr`, so a `function` lives where one may, as the containers do: on a stack or inside a
  managed object ([The rules](../README.md#the-rules), 1). The closure follows the rules of its captures where the
  `function` lives, as a member would.
- A closure in a node is destroyed by an assignment, the assignment of `nullptr` or the destructor, at once, on the
  calling thread; the objects it captured are unreferenced from then on and die with the next cycle that finds them
  so.
- A closure in a node is traced: one that captures a strong pointer to the object holding the `function` is a
  cycle, collected when nothing else reaches it; one that captures a strong pointer to an object an
  [expiry_queue](../expiry_queue/README.md) watches keeps that object alive. `expiry_queue` takes its function as a
  `function`: an entry's function may capture the objects it works on.
- Thread safety is that of `std::function`: threads share one with the program's own synchronization
  ([The rules](../README.md#the-rules), 6).

## Template parameters

| Parameter | Description |
|---|---|
| `R` | The result type of the call; `void` discards the callable's result. |
| `Args` | The parameter types of the call. |

## Member types

| Type | Definition |
|---|---|
| `result_type` | `R` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](function.md) | constructs a `function`, empty or holding a callable |
| `(destructor)` | destroys the callable, if any; a node is left to the collector |
| [operator=](operator_assign.md) | assigns another `function`, a callable or `nullptr` |
| [swap](swap.md) | swaps the callables of two `function` objects |
| [operator bool](operator_bool.md) | checks whether the `function` holds a callable |
| [operator()](operator_call.md) | calls the callable |
| [target_type](target_type.md) | the `typeid` of the callable held |
| [target](target.md) | a pointer to the callable held, by its type |

## Non-member functions

| Function | Description |
|---|---|
| [operator==](operator_cmp.md) | compares with `nullptr` |
| [swap](swap2.md) | swaps the callables of two `function` objects |

## Deduction guides

```cpp
template<class R, class... Args>
function(R (*)(Args...)) -> function<R(Args...)>;

template<class F>
function(F) -> function</* the signature of F::operator() */>;
```

The second takes the parameters and the result of `F::operator()`, whatever its `const`, `noexcept` and `&`.

## Complexity

Every operation is constant. A closure in the buffer costs no allocation; a closure in a node costs one managed
allocation per construction and per copy. A call is one indirect call, plus the step through the node's pointer
for a closure in a node.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

// An event with listeners: each listener a function capturing the object it works on, kept in a
// vector where a tracked pointer may live. A listener capturing the button that holds it would be
// a cycle, collected with the button.
struct Label {
    string text;
};

struct Button {
    vector<function<void(const string&)>> on_click;  // the closures with pointers in managed nodes
    void click(const string& what) {
        for (auto& f : on_click) {
            f(what);
        }
    }
};

int main() {
    Button button;
    tracked_ptr label = make_tracked<Label>();
    // the closure in a managed node: label followed
    button.on_click.push_back([label](const string& what) { label->text = "clicked " + what; });
    // no pointers: inside the function
    button.on_click.push_back([](const string& what) { println("log: {}", what); });
    tracked_ptr<Label> seen = label;
    label = nullptr;  // the listener keeps the label
    collector::force_collect(true);  // optional, for the demonstration only
    button.click("ok");
    println("{}", seen->text);
    button.on_click.clear();  // the closures destroyed now; the label lives on through seen
}
```

Output:

```text
log: ok
clicked ok
```

## See also

- [move_only_function](../move_only_function/README.md): the same storage for a callable that is not copyable
- [any](../any/README.md): the same storage for a value
- [expiry_queue](../expiry_queue/README.md): where a `function` runs with the object alive one last time
- [thread](../thread/README.md): the callable of a thread, in a managed node as well
- [tracked_ptr](../tracked_ptr/README.md), [weak_ptr](../weak_ptr/README.md)
- [Pointer maps](../../../garbage_collector/overview.md#pointer-maps), [README: The rules](../README.md#the-rules)
