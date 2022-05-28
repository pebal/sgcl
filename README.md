# SGCL
## About the SGCL
SGCL is a pauseless tracing garbage collector for C ++. The garbage collector works in a separate thread and never pauses other threads. All operations are lock-free.
## Compiling
C++17 compiler required.
## Classes
Base class of all traced objects:
```
class object;

// example
class foo : public virtual gc::object {};
```
A class that simplifies traced class definition: 
```
class collected;

// example
class foo : public gc::collected {};
```
Traced pointers:
```
class ptr<T>;
class weak_ptr<T>;
class atomic_ptr<T>;
class atomic_weak_ptr<T>;

// example
gc::ptr<foo> a;
```
## Functions
Create a tracked object:
```
ptr<T> make(...);

// example
auto a = gc::make<foo>();
```
Pointer cast:
```
ptr<T> static_pointer_cast(ptr<U>);
ptr<T> dynamic_pointer_cast(ptr<U>);
ptr<T> const_pointer_cast(ptr<U>);

// example
gc::ptr<object> a = gc::make<foo>();
auto b = gc::dynamic_pointer_cast<foo>(a);
```
## Example usage
```
#include "sgcl.h"

struct node : public gc::collected {
  gc::ptr<node> l, r;
};

gc::ptr<node> tree(int c) {
  auto n = gc::make<node>();
  if (c) {
    n->l = tree(c - 1);
    n->r = tree(c - 1);
  }
  return n;
}

int main() {
  tree(10);    
  return 0;
}
```
```
#include "sgcl.h"

class foo : public gc::collected {
};
class bar : public gc::collected {
};
struct baz : public foo, public bar {
};

int main() {
  gc::ptr<foo> a = gc::make<baz>();
  auto b = a.clone();
  auto c = gc::dynamic_pointer_cast<bar>(b);
  return 0;
}
```
```
#include "sgcl.h"

class foo : public gc::collected {
};

int main() {
  gc::atomic_ptr<foo> a = gc::make<foo>();
  gc::atomic_ptr<foo> b = a.load(std::memory_order_relaxed);
  auto c = b.exchange(gc::make<foo>()); 
  return 0;
}
```
## Not working
Cycles are not detected when a traced object contains dynamic container with traced pointers.
```
class foo : public gc::collected {
  std::vector<gc::ptr<foo>> vec = {gc::ptr<foo>(this)};
};
```
## Todo
- Implement dedicated containers (to detect cycles)
- Test it in a variety of scenarios
