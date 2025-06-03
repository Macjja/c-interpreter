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

void expression(int level) {
  // do nothing
}

void program() {
  next();
  while (token > 0) {
    printf("token is: %c\n", token);
    next();
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

enum { CHAR, INT, PTR };
int *idmain; // the 'main' function
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
