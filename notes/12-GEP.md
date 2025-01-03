### GEP - Get Element Pointer

The GEP instruction or the get element pointer instruction is used to compute the address of a field.

`<address> = getelementptr <ty>, <ty>* <base>, <idx1>, <idx2>, ...`

Example:

`%ptr = getelementptr i32 (type), i32* %base (type - starting memory address), i32 1 (offset)`

Example GEP instruction for an array:

`@ptr = getelementptr [4 * i32], [4 * i32]* @array, i32 0`

In case of arrays, we get a pointer of not the array element type, rather a pointer which comprises of the whole array. In case of offsets we would not move to the next or n-offset element in the array rather (offset * elements in array * type of array element)th position in memory. The first offset while iterating in complex structures is always zero.

Instruction to iterate in an array:

`%px = getelementptr [4 * i32], [4 * i32]* @array, i32 0, i32 2` - this instruction will move us to the index 2 in a zero index array.

For string "Hello, World!", the representation looks as follows:

`@str = global [15 * i8] c"Hello, World!\0A\00"
%px = getelementptr [15 * i8], [15 * i8]* @str, i32 0, i32 2` -> this will point to char "l".

For the class Point with two variables. The class will be represented with struct types.

`%Point = {i32, i32}`

`@p = global %Point {i32 10, i32 20}` - instance of a class

`%px = getelementptr %Point, %Point* @p, i32 0, i32 1` - pointer will point to i32 20