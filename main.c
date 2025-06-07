#include <fcntl.h>
#include <memory.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define int long long int

int token;           // current token
char *src, *old_src; // pointer to source code string
int poolsize;        // default size of text/data/stack
int line;            // line number

int *text,     // text segment (instructions)
    *old_text, // for dump of text segment
    *stack;    // stack
char *data;    // data segment

int *pc, *bp, *sp, ax, cycle; // virual machine registers
// bp is a pointer to some element of the stack
// sp is the pointer to the top of the stack
// ax is the general purpose register
//
int index_of_bp; // index of bp pointer on stack


// instructions
enum {LEA ,IMM ,JMP ,CALL,JZ  ,JNZ ,ENT ,ADJ ,LEV ,LI  ,LC  ,SI  ,SC  ,PUSH,
       OR  ,XOR ,AND ,EQ  ,NE  ,LT  ,GT  ,LE  ,GE  ,SHL ,SHR ,ADD ,SUB ,MUL ,DIV ,MOD ,
       OPEN,READ,CLOS,PRTF,MALC,MSET,MCMP,EXIT };

// tokens and classes (operators are last and in precidence order)
enum {
  Num = 128, Fun, Sys, Glo, Loc, Id,
  Char, Else, Enum, If, Int, Return, Sizeof, While,
  Assign, Cond, Lor, Lan, Or, Xor, And, Eq, Ne, Lt, Gt, Le, Ge, Shl, Shr, Add, Sub, Mul, Div, Mod, Inc, Dec, Brak
};

int token_val;    // value of current token (mainly for number)
int *current_id,  // current parsed ID
    *symbols;     // symbol table

// fields of identifier (like a struct, but our complier doesn't understand them so we use int array)
enum {Token, Hash, Name, Type, Class, Value, BType, BClass, BValue, IdSize};

enum { CHAR, INT, PTR };
int *idmain; // the 'main' function

// set of helper functions to make code more readable
int is_low_char(int input) {
  return (input >= 'a' && input <= 'z');
}

int is_upper_char(int input) {
  return (input >= 'A' && input <= 'Z');
}

int is_num_char(int input) {
  return (input >= '0' && input <= '9');
}

int is_hex_char(int input) {
  return is_num_char(input) || (input >= 'a' && token <= 'f') || (input >= 'A' && input <= 'F');
}

int is_oct_char(int input) {
  return (input >= '0' && input <= '7');
}

// handles parsing the next token
void next() {
  char *last_pos;
  int hash;
  while (token = *src) {
    ++src;
    // parse token here
    if (token == '\n') {
      ++line;
    }
    else if (token == '#') {
      //skip macro, we will not support them
      while (*src != 0 && *src != '\n') {
        src++;
      }
    }
    else if (is_low_char(token) || is_upper_char(token) || (token == '_')) {
      // parse identifier
      last_pos = src - 1;
      hash = token;
      while (is_low_char(*src) || is_upper_char(*src) || is_num_char(*src) || (*src == '_')) {
        hash = hash * 147 + *src;
        src++;
      }
      // look for existing identifier, linear search
      current_id = symbols;
      while (current_id[Token]) {
        if (current_id[Hash] == hash && !memcmp((char *)current_id[Name], last_pos, src - last_pos)) {
          // found one, return
          token = current_id[Token];
          return;
        }
        current_id = current_id + IdSize;
      }
      // store new ID
      current_id[Name] = (int)last_pos;
      current_id[Hash] = hash;
      token = current_id[Token] = Id;
      return;
    }
    else if (is_num_char(token)) {
      // parse number, three kinds: dec(123) hex(0x123) oct(017)
      token_val = token - '0';
      if (token_val > 0) {
        // decimal, starts with [1-9]
        while (is_num_char(*src)) {
          token_val = token_val * 10 + *src++ - '0';
        }
      } else {
        // number starts with a zero
        if (*src == 'x' || *src == 'X') {
          // hex
          token = *++src;
          while (is_hex_char(token)) {
            token_val = token_val * 16 + (token & 15) + (token >= 'A' ? 9 : 0);
            token = *++src;
          }
        } else {
          // oct num
          while (is_oct_char(*src)) {
            token_val = token_val * 8 + *src++ - '0';
          }
        }
      }
      token = Num;
      return;
    }
    else if (token == '"' || token == '\'') {
      // parse string literal, currently, the only supported escape
      // characcter is '\n', store the string literal into data.
      last_pos = data;
      while (*src != 0 && *src != token) {
        token_val = *src++;
        if (token_val == '\\') {
          // escape char
          token_val = *src++;
          if (token_val == 'n') {
            token_val = '\n';
          }
        }
        if (token == '"') {
          *data++ = token_val;
        }
      }
      src++;
      // if it is a single characcter, reutrn Num token
      if (token == '"') {
        token_val = (int)last_pos;
      } else {
        token = Num;
      }

      return;
    }
    else if (token == '/') {
      if (*src == '/') {
        // skip comments
        while (*src != 0 && *src != '\n') {
          ++src;
        }
      } else {
        // then it is a divide operator
        token = Div;
        return;
      }
    }
    else if (token == '=') {
      // parse '=' and '=='
      if (*src == '=') {
        src++;
        token = Eq;
      }
      else {
        token = Assign;
      }
      return;
    }
    else if (token == '+') {
      // parse '+' and '++'
      if (*src == '+') {
        src++;
        token = Inc;
      } else {
        token = Add;
      }
      return;
    }
    else if (token == '-') {
      // parse - and --
      if (*src == '-') {
        src++;
        token = Dec;
      } else {
        token = Sub;
      }
      return;
    }
    else if (token == '!') {
      // parse !=
      if (*src == '=') {
        src++;
        token = Ne;
      }
      return;
    }
    else if (token == '<') {
      // parse '<=', '<<', or '<'
      if (*src == '=') {
        src++;
        token = Le;
      } else if (*src == '<') {
        src++;
        token = Shl;
      } else {
        token = Lt;
      }
      return;
    }
    else if (token == '>') {
      // parse '>=' '>>' or '>'
      if (*src == '=') {
        src++;
        token = Ge;
      } else if (*src == '>') {
        src++;
        token = Shr;
      } else {
        token = Gt;
      }
      return;
    }
    else if (token == '|') {
      // parse 
      if (*src == '|') {
        src++;
        token = Lor;
      } else {
        token = Or;
      }
      return;
    }
    else if (token == '&') {
      // parse '&' and '&&'
      if (*src == '&') {
        src++;
        token = Lan;
      } else {
        token = And;
      }
      return;
    }
    else if (token == '^') {
      token = Xor;
      return;
    }
    else if (token == '%') {
      token = Mod;
      return;
    }
    else if (token == '*') {
      token = Mul;
      return;
    }
    else if (token == '[') {
      token = Brak;
      return;
    }
    else if (token == '?') {
      token = Cond;
      return;
    }
    else if (token == '~' || token == ';' || token == '{' || token == '}' ||
             token == '(' || token == ')' || token == ']' || token == ',' ||
             token == ':') {
      // these get passed directly as tokens
      return;
    }
  }
  return;
}

void match(int tk) {
  if (token == tk) {
    next();
  } else {
    printf("%d: expected token: %d\n", line, tk);
    exit(-1);
  }
}

void expression(int level) {
  // do nothing
}

void enum_declaration() {
  // parse enum [id] { a = 1, b = 3, ...}
  int i;
  i = 0;
  while (token != '}') {
    if (token != Id) {
      printf("%d: ba enum identifier %d\n", line, token);
      exit(-1);
    }
    next();
    if (token == Assign) {
      // like {a=10}
      next();
      if (token != Num) {
        printf("%d: bad enum initializer\n", line);
        exit(-1);
      }
      i = token_val;
      next();
    }

    current_id[Class] = Num;
    current_id[Type] = INT;
    current_id[Value] = i++;

    if (token == ',') {
      next();
    }
  }
}


void statement() {
  // there are 6 kinds of statements here:
  // 1. if (...) <statement> [else <statement>]
  // 2. while (...) <statement>
  // 3. { <statement> }
  // 4. return xxx;
  // 5. <empty statement>;
  // 6. expression; (expression end with semicolon)
  
  int *a, *b;

  if (token == If) {
    match(If);
    match('(');
    expression(Assign); // parse condition
    match(')');
    
    *++text = JZ;
    b = ++text;

    statement();         // parse statement
    if (token == Else) { // parse else
      match(Else);
      
      // emit code for JMP B
      *b = (int)(text + 3);
      *++text = JMP;
      b = ++text;

      statement();
    }
    *b = (int)(text + 1);
  }
  else if (token == While) {
    match(While);

    a = text + 1;

    match('(');
    expression(Assign);
    match(')');

    *++text = JZ;
    b = ++text;

    statement();

    *++text = JMP;
    *++text = (int)a;
    *b = (int)(text + 1);
  }
  else if (token == Return) {
    // return [expression];
    match(Return);

    if (token != ';') {
      expression(Assign);
    }

    match(';');

    // emit code for return
    *++text = LEV;
  }
  else if (token == '{') {
    // { statment ... }
    match('{');

    while (token != '}') {
      statement();
    }

    match(';');
  }
  else if (token == ';') {
    // empty statement
    match(';');
  }
  else {
    // a = b; or function_call();
    expression(Assign);
    match(';');
  }
}

void function_body() {
  // type func_name (...) {...}
  //                   -->|   |<--

  // ... {
  // 1. local declarations
  // 2. statements
  // }

  int pos_local; // position of local variables on the stack.
  int type; 
  pos_local = index_of_bp;

  while (token == Int | token == Char) {
    // local variable declaration, just like global ones.
    basetype = (token == Int) ? INT : CHAR;
    match(token);

    while (token != ';') {
      type = basetype;
      while (token == Mul) {
        match(Mul);
        type = type + PTR;
      }

      if (token != Id) {
        // invalid declaration
        printf("%d: bad local declaration\n", line);
        exit(-1);
      }
      if (current_id[Class] == Loc) {
        // identifier exists
        printf("%d: duplicate local declaration\n", line);
        exit(-1);
      }
      match(Id);
      
      // store the local variable
      current_id[BClass] = current_id[Class]; current_id[Class] = Loc;
      current_id[BType] = current_id[Type]; current_id[Type] = type;
      current_id[BValue] = current_id[Value]; current_id[Value] = ++pos_local;

      if (token == ',') {
        match(',');
      }
    }

    match(';');
  }

  // save the stack size for local variables;
  *++text = ENT;
  *++text = pos_local - index_of_bp;

  // statements
  while (token != '}') {
    statement();
  }

  // emit code for leaving the sub function
  *++text = LEV;
}


void function_parameter() {
  int type;
  int params;
  params = 0;
  while (token != ')') {
    // type {'*'} id {',' type {'*'} id}

    type = INT;
    if (token == Int) {
      match(Int);
    } else if (token == Char) {
      type = CHAR;
      match(Char);
    }

    // pointer type
    while (token == Mul) {
      match(Mul);
      type = type + PTR;
    }

    // parameter name
    if (token != Id) {
      printf("%d: bad parameter declaration\n", line);
      exit(-1);
    }
    if (current_id[Class] == Loc) {
      printf("%d: duplicate parameter declaration\n", line);
      exit(-1);
    }

    match(Id);

    // backup global var, then set info for local var of same name
    current_id[BClass] = current_id[Class]; current_id[Class] = Loc;
    current_id[BType] = current_id[Type]; current_id[Type] = type;
    current_id[BValue] = current_id[Value]; current_id[Value] = params++; // index of current param

    if (token == ',') {
      match(',');
    }
  }
  index_of_bp = params + 1;
}

void function_declaration() {
  // type func_name (...) {...}
  //                | this part
  match('(');
  function_parameter();
  match(')');
  match('{');
  function_body();
  // match('}'); leave match to be consumed by global_declaration

  // unwind local variable declarations for all local variables.
  current_id = symbols;
  while (current_id[Token]) {
    if (current_id[Class] == Loc) {
      current_id[Class] = current_id[BClass];
      current_id[Type] = current_id[BType];
      current_id[Value] = current_id[BValue];
    }
    current_id = current_id + IdSize;
  }
}

int basetype;  //  the type of a delcaration
int expr_type; // the type of an expression
void global_declaration() {
  // global_declaration ::= enum_decl | variable_decl | function_decl
  //
  // enum_decl ::= 'enum' [id] '{' id ['=' 'num'] {',' id ['=' 'num'} '}'
  //
  // variable_decl ::= type {'*'} id { ',' {'*'} id } ';'
  //
  // function_decl ::= type {'*'} id '(' parameter_decl ')' '{' body_decl '}'

  int type; // tmp, actual type for variable
  int i;

  basetype = INT;
  
  // parse enum, this should be treated alone.
  if (token == Enum) {
    // enum [id] {a = 10, b = 20, ... }
    match(Enum);
    if (token != '{') {
      match(Id);
    }
    if (token == '{') {
      // parse the assign part
      match('{');
      enum_declaration();
      match('}');
    }

    match(';');
    return;
  }

  // prase type information
  if (token == Int) {
    match(Int);
  }
  else if (token == Char) {
    match(Char);
    basetype = CHAR;
  }

  // parse the comma seperated variable declaration
  while (token != ';' && token != '}') {
    type = basetype;
    // parse pointer type, note that there may exist 'int ****x'
    while (token == Mul) {
      match(Mul);
      type = type + PTR;
    }

    if (token != Id) {
      // invalid declaration
      printf("%d: bad global declaration\n", line);
      exit(-1);
    }
    if (current_id[Class]) {
      // identifier exists
      printf("%d: duplicate global declaration\n", line);
      exit(-1);
    }
    match(Id);
    current_id[Type] = type;

    if (token == '(') {
      current_id[Class] = Fun;
      current_id[Value] = (int)(text + 1); // the memory address of function
      function_declaration();
    } else {
      // variable declaration
      current_id[Class] = Glo; // global variable
      current_id[Value] = (int)data; // assign memory address
      data = data + sizeof(int);
    }

    if (token == ',') {
      match(',');
    }
  }
  next();
}

void program() {
  next();
  while (token > 0) {
    global_declaration();
  }
}

int eval() {
  int op, *tmp;
  while (1) {
    op = *pc++; //get next op code
    switch (op) {
      case (IMM):  ax = *pc++; break;               // load immediate val to ax
      case (LC):   ax = *(char *)ax; break;         // load char to ax, address in ax
      case (LI):   ax = *(int *)ax; break;          // load int to ax, addresss in ax
      case (SC):   ax = *(char *)*sp++ = ax; break; // load char to address, val in ax, addr on stack
      case (SI):   *(int *)*sp++ = ax; break;       // save int to addr, val in ax, addr on stack
      case (PUSH): *--sp = ax; break;               // push val of ax on stack
      case (JMP):  pc = (int *)*pc; break;          // jump to addr
      case (JZ):   pc = ax ? pc + 1 : (int *)*pc; break; // jump if ax is zero
      case (JNZ):  pc = ax ? (int *)*pc : pc + 1; break; // jump if ax not zero
      // below are function calling
      case (CALL): *--sp = (int)(pc+1); pc = (int *)*pc; break; // call subroutine
      //case (RET): pc = (int *)*sp++; break;                   // return from subrountine
      case (ENT):  *--sp = (int)bp; bp = sp; sp = sp - *pc++; break; // make a new stack frame
      case (ADJ):  sp = sp + *pc++; break;                           // adjusts size of stack, like a special add
      case (LEV):  sp = bp; bp = (int *)*sp++; pc = (int *)*sp++; break; // restore call frame
      case (LEA):  ax = (int)(bp + *pc++); break;                         // load address for arguments
      // below are math functions
      case (OR):   ax = *sp++ | ax; break;
      case (XOR):   ax = *sp++ ^ ax; break;
      case (AND):   ax = *sp++ & ax; break;
      case (EQ):   ax = *sp++ == ax; break;
      case (NE):   ax = *sp++ != ax; break;
      case (LT):   ax = *sp++ < ax; break;
      case (LE):   ax = *sp++ <= ax; break;
      case (GT):   ax = *sp++ > ax; break;
      case (GE):   ax = *sp++ >= ax; break;
      case (SHL):   ax = *sp++ << ax; break;
      case (SHR):   ax = *sp++ >> ax; break;
      case (ADD):   ax = *sp++ + ax; break;
      case (SUB):   ax = *sp++ - ax; break;
      case (MUL):   ax = *sp++ * ax; break;
      case (DIV):   ax = *sp++ / ax; break;
      case (MOD):   ax = *sp++ % ax; break;
      // special built in instructions
      case (EXIT): printf("exit(%d)\n", *sp); return *sp; break;
      case (OPEN): ax = open((char *)sp[1], sp[0]); break;
      case (CLOS): ax = close(*sp); break;
      case (READ): ax = read(sp[2], (char *)sp[1], *sp); break;
      case (PRTF): tmp = sp + pc[1]; ax = printf((char *)tmp[-1], tmp[-2], tmp[-3], tmp[-4], tmp[-5], tmp[-6]); break;
      case (MALC): ax = (int)malloc(*sp); break;
      case (MSET): ax = (int)memset((char *)sp[2], sp[1], *sp); break;
      case (MCMP): ax = memcmp((char *)sp[2], (char *)sp[1], *sp); break;
      default: {
        printf("unknown instruction: %d\n", op);
        return -1;
      }
    }
  }
  return 0;
}

int main(int argc, char **argv) {
  int i, fd;
  argc--;
  argv++;

  poolsize = 256 * 1024; // an arbitrary size
  line = 1;

  if ((fd = open(*argv, 0)) < 0) {
    printf("could not open(%s)\n", *argv);
    return -1;
  }

  if (!(src = old_src = malloc(poolsize))) {
    printf("could not malloc(%d) for source area\n", i);
    return -1;
  }

  if ((i = read(fd, src, poolsize - 1)) <= 0) {
    printf("read() returned %d\n", i);
    return -1;
  }

  src[i] = 0; // add EOF character
  close(fd);

  if (!(text = old_text = malloc(poolsize))) {
    printf("could not malloc(%d) for text area\n", poolsize);
    return -1;
  }
  if (!(data = malloc(poolsize))) {
    printf("cloud not malloc(%d) for data area\n", poolsize);
    return -1;
  }

  if (!(stack = malloc(poolsize))) {
    printf("cloud not malloc(%d) for stack area\n", poolsize);
    return -1;
  }

  memset(text, 0, poolsize);
  memset(data, 0, poolsize);
  memset(stack, 0, poolsize);

  bp = sp = (int *)((int)stack + poolsize);
  ax = 0;

  i = 0;
  text[i++] = IMM;
  text[i++] = 10;
  text[i++] = PUSH;
  text[i++] = IMM;
  text[i++] = 20;
  text[i++] = ADD;
  text[i++] = PUSH;
  text[i++] = EXIT;
  pc = text;

  src = "char else enum if int return sizeof while "
        "open read close printf malloc memset memcp exit void main";
  
  // add keywords to symbol table
  i = Char;
  while (i <= While) {
    next();
    current_id[Token] = i++;
  }

  // add library to symbol table
  i = OPEN;
  while (i <= EXIT) {
    next();
    current_id[Class] = Sys;
    current_id[Type] = INT;
    current_id[Value] = i++;
  }

  next(); current_id[Token] = Char; // handle void type
  next(); idmain = current_id; // keep track of main

  program();

  return eval();
}
