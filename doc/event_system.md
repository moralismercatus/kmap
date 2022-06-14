# Events should be fired ex post facto/after-the-fact.

Let's consider the scenario where `create.child` is executed.

There are at least two reasonable interception points:

1. Before node is created: pre.
1. After node is created: post.

The trouble with the pre case is that constraints are expected. E.g., in `create.child`'s case, that the child-to-be has a valid parent. It is possible that an outlet listening for such an event could break the constraints, requiring a re-examination of state, after the event is fired. This is non-ideal.

In the post case, the action is already done, so there isn't the same threat to state change posed. Of course, if somehow the outlet was meant to prevent completion of the action (which may indicate a dubious design), the outlet is still able to undo the action.

Following, verbs should be described in past-tense e.g., prefer "selected" over "select".