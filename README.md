C interpreter
--

This is just a little project that interprets your C code.

Todo:
 - [x] Add global variable inline assignment / declaration
 - [ ] Add local variable inline assignment / declaration
 - [ ] Add support for expressions like +=, -=, etc.
 - [ ] Add struct support?
 - [ ] Add support for switch cases, to get it boot strapping
 - [ ] Add support for function prototypes?

Usage
--
Simply pass the .c file into the interpreter as an argument when executing, 
then any arguments for your own code!

Current features:
 - has support for int, char, and void types as well as pointers
 - support for some standard library functions:
   `open`, `read`, `close`, `printf`, `malloc`, `memset`, `memcmp`
   `exit`
 - support for most operators, besides self assignment ones like +=
 - can do `if` and `while` loops
 - And of course, support for function delcarations

Comiling
--
To compile, run the following code:
```
git clone https://github.com/Macjja/c-interpreter.git
cd c-interpreter
gcc -o c-interpret main.c
```
Credits
--
The initial code was created following the tutorial found at: 
https://github.com/lotabout/write-a-C-interpreter/tree/master

Other modifications by me to add more features, and to get it running.
