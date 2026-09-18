# Sgcl::Variant

```cpp
#include "sgcl/Sgcl/Core/Variant.h"   // or "sgcl/Sgcl/Sgcl.h"

namespace Sgcl {
    template<class... Ts>
    class Variant;
}
```

The same class in the `sgcl` interface: [variant](../../core/variant.md).

`Variant<Ts...>` is `std::variant` for alternatives that hold tracked pointers. `std::variant` keeps every alternative at the same offset, so a `Ptr` alternative shares its word with the data of the others; the collector's pointer map, built by elimination, finds data at that offset in some object and drops the offset for good, and the pointer is no longer followed ([README: Pointer maps](../../../garbage_collector/overview.md#pointer-maps)). Here the alternatives are laid out by what they hold: the pointer words (`Ptr`, [`WeakPtr`](WeakPtr.md)) share one word that holds null or an address and nothing else (a pointer word's destructor leaves null behind, so several of them may take the word in turn); an alternative that may hold pointers among its data (a struct with a `Ptr` member, or anything the collector cannot rule out: a type with a constructor, `std::string` included) gets a place of its own; the alternatives that cannot hold a pointer (`int`, `double`, a plain struct) share the data storage. The pointer storage starts zeroed, wherever the variant lives.

The interface is that of `std::variant` under the interface's names: the constructors and their overload resolution (`Variant<int, Ptr<T>> v = Ptr(Make<T>())` picks the pointer), `in_place_type` and `in_place_index`, `Emplace`, `Index`, `IsValueless`, `Swap`, `Get`, `GetIf`, `Is`, `Visit` (over one variant as a member, over several as a free function), `bad_variant_access` (the one of `std`), the six comparisons and `<=>` when the alternatives have them, `std::hash` when they do. What differs: nothing is `constexpr` (the alternatives live in raw storage), the variant is never trivially copyable, and its size is the pointer word plus the places of the alternatives that may hold pointers plus the largest of the others (`Variant<int, Ptr<T>, WeakPtr<T>>` is 16 bytes). A variant of pointer-free alternatives has no reason to be one of these: `std::variant` is smaller and `constexpr`.

The variant has no word of its own. Where it may live is decided by its alternatives: with `Ptr` or `WeakPtr` alternatives, where a `Ptr` may (on a stack or inside a managed object).

## Rules

- The alternatives follow their own rules where the variant lives: a `Ptr` alternative in a variant on the unmanaged heap is the same mistake as a `Ptr` in a `std::vector` ([The rules](../../core/README.md#the-rules), 1).
- An alternative that may hold pointers is traced through the pointer map of the object holding the variant, like a member at the same offset would be; its own data words leave the map by elimination, its pointer words never do.
- A pointer alternative is destroyed when another is emplaced or assigned, or the variant is destroyed: its object is unreferenced from then on, and dies with the next cycle that finds it so.
- Thread safety is that of `std::variant`: threads share one with the program's own synchronization ([The rules](../../core/README.md#the-rules), 6).
- In a destructor, a pointer alternative is a `Ptr` member: not to be read there ([The rules](../../core/README.md#the-rules), 5).

## Members

```cpp
using InnerType = sgcl::variant<Ts...>;
static constexpr size_t Count;                              // sizeof...(Ts)

Variant();                                                  // the first alternative, value-initialized
Variant(const Variant&);
Variant(Variant&&);
template<class U> Variant(U&& u);                           // the alternative std::variant's overload resolution selects
template<class T, class... A> explicit Variant(std::in_place_type_t<T>, A&&...);
template<size_t I, class... A> explicit Variant(std::in_place_index_t<I>, A&&...);
explicit Variant(InnerType v);
~Variant();

Variant& operator=(const Variant&);
Variant& operator=(Variant&&);
template<class U> Variant& operator=(U&& u);

template<class T, class... A> T& Emplace(A&&...);
template<size_t I, class... A> decltype(auto) Emplace(A&&...);

size_t Index() const noexcept;                              // variant_npos when valueless
template<class T> bool Is() const noexcept;                 // holds_alternative
bool IsValueless() const noexcept;
template<class T> T& Get();                                 // and const: bad_variant_access on another type
template<size_t I> decltype(auto) Get();                    // and const
template<class T> T* GetIf() noexcept;                      // and const: null on another type
template<size_t I> auto GetIf() noexcept;
template<class F> decltype(auto) Visit(F&& f);              // and const
void Swap(Variant&);
InnerType& Inner() noexcept;
const InnerType& Inner() const noexcept;
```

The free functions, in `Sgcl`:

```cpp
template<class F, class... Vs> decltype(auto) Visit(F&& f, Vs&&... variants);   // over several at once
template<class... Ts> void swap(Variant<Ts...>&, Variant<Ts...>&);
bool operator==, !=, <, <=, >, >= (const Variant<Ts...>&, const Variant<Ts...>&);   // when every alternative has ==, or <
auto operator<=>(const Variant<Ts...>&, const Variant<Ts...>&);                     // when every alternative has <=>
template<class... Ts> struct std::hash<Variant<Ts...>>;                    // when every alternative has a hash
```

`Get` throws `bad_variant_access` (the one of `std`) on the wrong index, `Visit` on a valueless variant. `Visit` over several variants calls the visitor with one alternative of each; the visitor (a callable or a pointer to member, through `std::invoke`) returns one type for every combination of alternatives, as with `std::visit` (ill-formed otherwise: a constraint); a visitor whose results differ converts them itself (`-> double`).

```cpp
struct Node { int value; };
using Value = Variant<int, Ptr<Node>, String>;

Value v = Ptr(Make<Node>(1));  // index 1: the pointer, in the word of its own
assert(v.Is<Ptr<Node>>());
assert(v.Get<1>()->value == 1);
v = 5;                                           // index 0: the int; the pointer destroyed, the word null
v = "text";                                      // index 2: the string
auto described = v.Visit([](const auto& x) -> String {
    if constexpr(std::is_same_v<std::remove_cvref_t<decltype(x)>, String>) {
        return x;
    } else {
        return "not a string";
    }
});
assert(described == "text");
```

## Example

```cpp
#include "sgcl/Sgcl/Sgcl.h"
#include <iostream>

// A tree of values: a leaf holds a number or a name, a branch holds its
// children. The children are tracked pointers, next to the data, in one
// variant per node: the collector follows them.
struct Node;
using Children = List<Ptr<Node>>;
struct Node {
    Variant<double, String, Children> value;
};

double Sum(const Ptr<Node>& node) {
    return node->value.Visit([](const auto& v) -> double {
        using T = std::remove_cvref_t<decltype(v)>;
        if constexpr(std::is_same_v<T, double>) {
            return v;
        } else if constexpr(std::is_same_v<T, Children>) {
            double total = 0;
            for (auto& child : v) {
                total += Sum(child);
            }
            return total;
        } else {
            return 0;
        }
    });
}

int main() {
    Ptr root = Make<Node>();
    Children children;
    children.Add(Make<Node>(Node{1.5}));
    children.Add(Make<Node>(Node{String("name")}));
    children.Add(Make<Node>(Node{2.5}));
    root->value = std::move(children);
    std::cout << Sum(root) << "\n";              // 4
    root->value = 0.0;                           // the children unreferenced: collected
    Collector::Collect(true);           // optional, for the demonstration only
    return 0;
}
```

The output:

```
4
```

## See also

- [Any](Any.md): the same for a value of any type; [Ptr](Ptr.md), [WeakPtr](WeakPtr.md): the pointer words
- README: [variant, any, function and expected](../../core/README.md#variant-any-function-and-expected), [Pointer maps](../../../garbage_collector/overview.md#pointer-maps), [The rules](../../core/README.md#the-rules)
