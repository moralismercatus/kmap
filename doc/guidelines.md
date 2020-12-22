Argument ordering guidelines:

1. Order by Contingency

Imagine the following function:

auto create_child( Kmap&, parent, child, title, heading );

Kmap is the environment in which all the rest reside, therefore all are contingent upon Kmap. Thus, Kmap leads.

A child cannot exist without a parent, and the heading and title cannot exist without the child, so parent is next followed by child.

The order of heading and title is trickier. It would seem arbitrary, as they are actually allowed to be disjointed (i.e., unrelated, though it's highly discouraged). In practice, however, headings tend to be contingent upon the title. In general, headings can be derived from titles, but not vice-versa because character information gets lost when deriving a heading from a title. So, in practice, as a general rule, heading is contigent upon title.
