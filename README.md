C interpreter
--

This is just a little project that interprets your C code.

Todo:
 - [x] Add global variable inline assignment / declaration
 - [ ] Add local variable inline assignment / declaration
 - [ ] Add struct support?
 - [ ] Add support for switch cases, to get it boot strapping

Usage
--
Simply pass the .c file into the interpreter as an argument when executing, 
then any arguments for your own code!

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
