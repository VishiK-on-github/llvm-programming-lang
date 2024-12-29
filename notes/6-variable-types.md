### Types of variables:

1. Global Variables: outside of any functions. stored in the binary file. mutable. can have only constant initializers.
2. Register Variables: usually used to store temp results. uses single static assignment (SSA), so no memory manipulations.
3. Stack Variables: load / store local variables. these are allocated on stack.
