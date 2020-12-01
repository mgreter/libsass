# How LibSass handles variables, functions and mixins

This document is intended for developers of LibSass only and are of now use
for implementers. It documents how variable stacks are implemented.

## Foreword

LibSass uses an optimized stack approach similar to how C compilers have always
done it, by using a growable stack where we can push and pop items of. Unfortunately
Sass has proven to be a bit more dynamic than static optimizers like, therefore we
had to adopt the principle a little to accommodate the edge-cases due to this.

There are three different kind of entities on the stack during runtime, namely
variables, functions and mixins. Each has it's own dedicated stack to optimize
the lookups. In this doc we will often only cover one case, but it should be
applicable to any other stack object (with some small differences).

Also for regular sass code and style-rules we wouldn't need this setup, but
it becomes essential to correctly support mixins and functions, since those
can be called recursively. It is also vital for loops, like @for or @each.

## Overview

The whole process is split into two main phases. In order to correctly support
@import we had to introduce the preloader phase, where all @use, @forward and
@import rules are loaded first, before any evaluation happens. This ensures that
we know all entities before the evaluation phase in order to correctly setup
all stack frames.

## Basic example

Let's assume we have the following scss code:

```scss
$a: 1;
b {
  $a: 2;
}
```

This will allocate two independent variables on the stack. For easier reference
we can think of them as variable 0 and variable 1. So let's see what happens if
we introduce some VariableExpressions:

```scss
$a: 1;
b {
  a0: $a;
  $a: 2;
  a1: $a;
}
c {
  a: $a;
}
```

As you may have guesses, the `a0` expression will reference variable 0 and the
`a1` expression will reference variable 1, while the last one will reference
variable 0 again. Given this easy example this might seem overengineered, but
let's see what happens if we introduce a loop:

```
$a: 1;
b {
  @for $x from 1 through 2 {
    a0: $a;
    $a: 2;
    a1: $a;
  }
}
c {
  a: $a;
}
```

Here I want to concentrate on `a0`. In most programing languages, `a0: $a` would
always point to variable 0, but in Sass this is more dynamic. It will actually
reference variable 0 on the first run, and variable 1 on consecutive runs.

## What is an EnvFrame and EnvRef

Whenever we encounter a new scope while parsing, we will create a new EnvFrame.
Every EnvFrame (often also just called idxs) knows the variables, functions and
mixins that are declared within that scope. Each entity is simply referenced by
it's integer offset (first variable, second variable and so on). Each frame is
stored as long as the context/compiler lives. In order to find functions, each
frame keeps a hash-map to get the local offset for an entity name (e.g. varIdxs).
An EnvRef is just a struct with the env-frame address and the local entity offset.

## Where are entities actually stored during runtime

The `EnvRoot` has a growable stack for each entity type. Whenever we evaluate
a lexical scope, we will push the entities to the stack to bring them live.
By doing this, we also update the current pointer for the given env-frame to
point to the correct position within that stack. Let's see how this works:

```scss
$a: 1;
@function recursive($abort) {
  $a: $a + 1;
  @if ($abort) {
    @return $a;
  }
  @else {
    @return recursive(true);
  }
}
a {
  b: recursive(false);
}
```

Here we call the recursive function twice, so the `$a` inside must be independent.
The stack allocation would look the following in this case:

- Entering root scope
  - pushing one variable on the runtime var stack.
- Entering for scope for the first time
  - updating varFramePtr to 1 since there is already one variable.
  - pushing another variable on the runtime var stack
- Entering for scope for the second time
  - updating varFramePtr to 2 since there are now two variable.
  - pushing another variable on the runtime var stack
- Exiting second for scope and restoring old state
- Exiting first for scope and restoring old state
- Exiting root scope

So in the second for loop run, when we have to resolve the variable expression for `$a`,
we first get the base frame pointer (often called stack frame pointer in C). Then we only
need to add the local offset to get to the current frame instance of the variable. Once we
exit a scope, we simply need to pop those entities off the stack and reset the frame pointer.

## How ambiguous/dynamic lookup is done in loops

Unfortunately we are not able to fully statically optimize the variable lookups (as explained
earlier, due to the full dynamic nature of Sass). IMO we can do it for functions and mixins,
as they always have to be declared and defined at the same time. But variables can also just
be declared and defined later. So in order to optimize this situation we will first fetch and
cache all possible variable declarations (vector of VarRef). Then on evaluation we simply need
to check and return the first entity that was actually defined (assigned to).

## Afterword

I hope this example somehow clarifies how variable stacks are implemented in LibSass. This
optimization can easily bring 50% or more performance in contrast to always do the dynamic
lookup via the also available hash-maps. They are needed anyway for meta functions, like
`function-exists`. It is also not possible to (easily) create new variables once the parsing
is done, so the C-API doesn't allow to create new variables during runtime.
