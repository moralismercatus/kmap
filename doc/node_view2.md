# Node View Design Decisions

-------

# General Design

The following are the components of a NV2:

* Anchor
* Link
* Chain
* Tether
* Actor
* Predicate

```
Result = Tether >> Actor
Tether = Anchor >> Chain
Chain = ( Link >> Link )*
Anchor = <concrete node>*
```

-------

## Anchor

An anchor is what roots the chain to reality, to a concrete element of the network; the stake that binds to chain to the ground.

Without the anchor, a chain would just an unresolvable idea of an unresolvable network structure.

For example, say we have following links without an anchor:

```cpp
result = child( "victor" ) | desc( "charlie" ) | to_node_set( km );
```

The question is: what is this asking of the network? Yes, _somewhere_ in the network there is the structure of a child, "victor", followed by some descendant, "charlie", but child of what? Even if one says, "Any child named 'victor'," such a claim should be explicit, as in, to make the intent clear, and avoid errors in which the anchor is forgotten:

```cpp
result = abs_root | child( "victor" ) | desc( "charlie" ) | to_node_set( km );
```

------

## Link

A link refers to an intermediate step forming the overall structure/chain, such as child, descendant, ancestor, and so on.

Example:

```cpp
auto const chain = child( "victor" ) | desc( "charlie" );
```

------

## Chain

A chain is composed of one or more links. A chain can be combined with other chains.

Example:

```cpp
auto const chain1 = child( "victor" ) | desc( "charlie" );
auto const chain2 = chain1 | child( "delta" );
auto const chain3 = child( "echo" ) | chain1 | chain2 | child( "foxtrot" );
```

------

## Tether

A tether is a chain that has been attached to an anchor. An actor can only act on a tether. A tether suffix can be added to a tether to form another tether, but a prefix cannot be added.

Example:

```cpp
auto const chain = child( "victor" ) | desc( "charlie" );
auto const tether1 = abs_root | chain;
auto const tether2 = tether1 | desc( "echo" );
```

------

## Actor

The actor is what actually consumes and acts on the tether to retrieve the result. It is the only non-lazy, "greedy", component.

```cpp
auto const result = abs_root | child( "victor" ) | desc( "charlie" ) | to_node_set( km );
```

------

## Predicate

A predicate a conditional for a link. A refinement for the link. So, if you have a `view::child`, all child nodes will be selected; however, one can refine this with e.g., `view::child( "victor" )`, then only child nodes with the heading "victor" will be selected.

Q: If both the link type (e.g., "child") and predicate act to refine "node", why are they distinct?
A: For the most part, it is out of convenience. A "child" node is a general type. Any link could be a child, hypothetically, but headings are particular, and domain specific. "child" is a fundamental building block, whereas `child( "victor" )` is either an intermediary or ending "block."

Because predicates are domain specific, they are more challenging to implement. For example, `child( "victor.charlie" )` doesn't actually make sense, as only a heading makes sense for a child, not a heading_path, whereas e.g., `view::desc( "victor.child" )` does make sense.

------

# Particular Design

------

## Alias Link

Alias link design has been challenging because it has three parts:

1. The alias node itself
1. The source node
1. The destination node

It is not obvious what `view::alias` should refer to, in terms of predicates.

It's simple enough to declare `view::alias` to mean the alias node, but what about `view::alias( id )`? Does it mean the Uuid of the alias node itself, or of the source?

To be consistent with other predicates like `view::child`, I imagine the ID should refer to the alias node itself, and not the source (and definitely not the dest).

I think it may be ideal to have `view::alias` and `view::alias_src`, rather than `view::alias( pred::src{ id } )` because `pred::src` isn't generalizable. What does e.g., `view::child( pred::src{ id } )` mean?

FWIW, I could complete the alias picture with `view::alias_dst`.

Well, shoot. How does this maintain the property of unwindability (or whatever it is to be called). That is:

```cpp
// "Unwindable:
    view::abs_root
|   view::child;
|   view::parent // Current nodeset == { abs_root } => OK
// "NOT Unwindable:
    view::abs_root
|   view::alias_src( "victor" );
|   view::parent // Current nodeset == { parent_of_victor } => Not OK!
// "Whereas, Unwindable:
    view::abs_root
|   view::alias( pred::Src{ "victor" } );
|   view::parent // Current nodeset == { abs_root } => OK
```

What this tells me is that we need to use predicates, but, somehow, can I not make pred::Src, pred::Dst have to apply to every Link?

Can I make each Link able to opt-in to any predicate? That would solve the problem of "predicate-explosion".

struct Alias
{
    using PredicateVariant = std::variant< Src, Dst, Uuid, PredFn, AnyOf, AllOf, NoneOf, Exactly >;
};

So... one problem I see is in trying to forward predicates to other links:

```cpp
return view::root{ nw->resolve( node ) } | 
```

------

## Qualifier Predicates

That is, expansions on independent predicates:

* any_of
* all_of
* none_of
* exactly

These should probably be under

pred::qual

And used as links:

```cpp
# Current:
return view::root( node )
     | view::child( pred::all_of( "victor", "charlie", "delta" ) )
     | act::to_node_set( km );
# Desired:
return view::root( node )
     | pred::all_of( view::child, "victor", "charlie", "delta" ) )
     | act::to_node_set( km );
```

This becomes much simpler to implement, (so every Link doesn't need to define all_of, none_of, etc.), but legibility suffers.
The former definitely is more legible than the latter.

What about...:
```cpp
return view::root( node )
     | view::child | pred::all_of( "victor", "charlie", "delta" )
     | act::to_node_set( km );
```

I think my only real option is:

```cpp
return anchor::root( node )
     | view::all_of( view::child( "victor" )
                   , view::child( "charlie" )
                   , view::child( "delta" ) ) )
     | act::to_node_set( km );
```

That is, for those predicates that essentially use the Link, but don't require the Link to be aware of it, is to take the predicate/qualifier outside.

I think it may be possible for view::child( pred::all_of( "victor", "charlie", "delta" ) ), by providing operator()( all_of ), operator()( any_of), etc. and returning pred::all_of( ... ), basically propagate it. But, for now, the above.

As to the propagation of predicates, by virtue of using constexpr, propagation should be possible via Link-specific combinations of predicates.

That is to say, if a Link has operator()( variant< string, Uuid, Intermediate, Src, Dst > ), it should still be able to propagate to another Link with operator()( variant< string, Uuid, Intermediate > ), for example.

------

## Tips

Not all qualifier operations are of equal complexity.

For example, `none_of` fetches all and then fetches the selected, and performs a set diff. This means that, even if it is efficient to fetch the selected (say, e.g., via direct path input), all nodes will need to be selected.
