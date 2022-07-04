# Node View Design Decisions

## General Design

### std::ranges-based or original?

Serious consideration should be made as to whether kmap's node structure fits in with std::ranges (or equiv. range-v3). The benefits are to leverage the existing
infastructure. Much of the difficulties have already been sorted out, and redunancy can be reduced. The concerns are to do with how it fits. Would it be a shoe-in,
or a horse-shoe-in-kind-of; less control over desired behavior. Both work on collections of objects. Both are lazy, but all kmap "ranges" require kmap passed to execute.

### Algorithms should use concepts instead of variants

Currently, we have this kind of design:

```cpp
using OperatorVariant = std::variant< Alias
                                    , Ancestor
                                    , Attr
                                    , Child
                                    , Desc
                                    , DirectDesc
                                    , Leaf
                                    , RLineage
                                    , Parent
                                    , Resolve
                                    , Sibling
                                    , Single
                                    , Tag >;

struct Intermediary
{
    std::vector< OperatorVariant > op_chain;
    UuidSet root = {};
};

using SelectionVariant = std::variant< char const*
                                     , all_of
                                     , any_of
                                     , exactly
                                     , none_of
                                     , Intermediary
                                     , Uuid >;
```

For a new operator or selector to work, they must be added to this centeralized list. This is undesirable. What if TaskStore wants to plug in its interfaces here? Well,
this centeral variant needs to be updated, everything recompiled. Tight coupling, and quite unnecessary, I think. The variant can instead be vector< any< Concept > >, or even
a base class that each inherits from.

## Details

### act::create_node

#### Return failure if the leaf view already exists

```cpp
  view::make( root )
| view::child( "subroot" ) // fetch_or_create
| view::child( "alias_root" ) // fetch_or_create
| view::alias( alias_src_node ) // create only
| act::create_node( kmap ) );
```

This is the only practical way to differentiate between `act::create_node` and `act::fetch_or_create_node`.

### view::alias

