#include <fcntl.h>
#include <memory.h>
#include <stddef.h>
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

// instructions for setting the value of global vars
int *global_var_assign_text, *old_global_assign_text;

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
int basetype;  //  the type of a delcaration
int expr_type; // the type of an expression

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

// looks at the next token and checks if it's the same as the current token
int look_ahead(int tk) {
  // hold the current matching info
  char* prev_src = src;
  int prev_token_val = token_val;
  int prev_token = token;
  int *prev_id = current_id;
  next();

  // restore matching info
  int next_token = token;
  token = prev_token;
  src = prev_src;
  current_id = prev_id;
  if (next_token == tk) {
    return 1;
  } else {
    return 0;
  }
}

void expression(int level, int **text) {
  // expressions have various format.
  // but majorly can be divided into two parts: unit and operator
  // for example `(char) *a[10] = (int *) func(b > 0 ? 10 : 20);
  // `a[10]` is an unit while `*` is an operator.
  // `func(...)` in total is an unit.
  // so we should first parse those unit and unary operators
  // and then the binary ones
  //
  // also the expression can be in the following types:
  //
  // 1. unit_unary ::= unit | unit unary_op | unary_op unit
  // 2. expr ::= unit_unary (bin_op unit_unary ...)

  // unit_unary()

  int *id;
  int tmp;
  int *addr;


  if (token == Num) {
    match(Num);
    
    // emit code

    *++(*text) = IMM;
    *++(*text) = token_val;
    expr_type = INT;
  }
  else if (token == '"') {
    // emit code
    *++(*text) = IMM;
    *++(*text) = token_val;
    match('"');

    // store the rest strings
    while (token == '"') {
      match('"');
    }

    // append the end of string character '\0', all the data are default
    // to 0, so we just move data one position forward.

    data = (char *)(((int)data + sizeof(int)) & (-sizeof(int)));
    expr_type = PTR;
  }
  else if (token == Sizeof) {
    // sizeof is a unary operator
    // for now, only 'sizeof(int', 'sizeof(char)' and 'sizeof(*...)' are 
    // supported

    match(Sizeof);
    match('(');
    expr_type = INT;

    if (token == Int) {
      match(Int);
    } else if (token == Char) {
      match(Char);
      expr_type = CHAR;
    }

    while (token == Mul) {
      match(Mul);
      expr_type = expr_type + PTR;
    }

    match(')');

    // emit code
    *++(*text) = IMM;
    *++(*text) = (expr_type == CHAR) ? sizeof(char) : sizeof(int);
    expr_type = INT;
  }

  else if (token == Id) {
    // there are several cases where Id occurs
    // but there are only a few situations:
    // 1. function call
    // 2. Enum variable
    // 3. global/local variable
    //
    match(Id);

    id = current_id;

    if (token == '(') {
      // function call;
      match('(');

      // pass in arguments
      tmp = 0; // number of arguments
      while (token != ')') {
        expression(Assign, text);
        *++(*text) = PUSH;
        tmp++;

        if (token == ',') {
          match(',');
        }
      }
      match(')');

      // emit code
      if (id[Class] == Sys) {
        // system functions
        *++(*text) = id[Value];
      }
      else if (id[Class] == Fun) {
        // function call
        *++(*text) = CALL;
        *++(*text) = id[Value];
      }
      else {
        printf("%d: bad function call to function: \"%s()\" \n", line, id[Name]);
        exit(-1);
      }

      // clean the stack for arguments
      if (tmp > 0) {
        *++(*text) = ADJ;
        *++(*text) = tmp;
      }
      expr_type = id[Type];
    }
    else if (id[Class] == Num) {
      // enum variable
      *++(*text) = IMM;
      *++(*text) = id[Value];
      expr_type = INT;
    }
    else {
      // variable
      if (id[Class] == Loc) {
        *++(*text) = LEA;
        *++(*text) = index_of_bp - id[Value];
      }
      else if (id[Class] == Glo) {
        *++(*text) = IMM;
        *++(*text) = id[Value];
      }
      else {
        printf("%d: undefined variable: \"%s\"\n", line, id[Name]);
        exit(-1);
      }

      // emit code, default behaviour is to load the value of the
      // address which is stored in 'ax'
      expr_type = id[Type];
      *++(*text) = (expr_type == Char) ? LC : LI;
    }
  }
  else if (token == '(') {
    // cast or parenthesis
    match('(');
    if (token == Int || token == Char) {
      tmp = (token == Char) ? CHAR : INT; // cast type
      match(token);
      while (token == Mul) {
        match(Mul);
        tmp = tmp + PTR;
      }

      match(')');

      expression(Inc, text); // cast has precedence as Inc(++)

      expr_type = tmp;
    } else {
      // normal parenthesis
      expression(Assign, text);
      match(')');
    }
  }
  else if (token == Mul) {
    // dereference *<addr>
    match(Mul);
    expression(Inc, text); // dereference has same precedence as Inc(++)

    if (expr_type >= PTR) {
      expr_type = expr_type - PTR;
    } else {
      printf("%d: bad dereference\n", line);
      exit(-1);
    }

    *++(*text) = (expr_type == CHAR) ? LC : LI;
  }
  else if (token == And) {
    // get the address of a variable (ptr)
    match(And);
    expression(Inc, text); // get the address of

    if (**text == LC || **text == LI) {
      (*text)--;
    } else {
      printf("%d: bad pointer\n", line);
      exit(-1);
    }

    expr_type = expr_type + PTR;
  }
  else if (token == '!') {
    // not
    match('!');
    expression(Inc, text);

    // emit code, use <expr> == 0
    *++(*text) = PUSH;
    *++(*text) = IMM;
    *++(*text) = 0;
    *++(*text) = EQ;

    expr_type = INT;
  }
  else if (token == '~') {
    // bitwise not
    match('~');
    expression(Inc, text);

    // emit code, use <expr> XOR -1
    *++(*text) = PUSH;
    *++(*text) = IMM;
    *++(*text) = -1;
    *++(*text) = XOR;

    expr_type = INT;
  }
  else if (token == Add) {
    // +var, do nothing (it just means positive of number)
    match(Add);
    expression(Inc, text);
    expr_type = INT;
  } 
  else if (token == Sub) {
    // -var
    match(Sub);

    if (token == Num) {
      *++(*text) = IMM;
      *++(*text) = -token_val;
      match(Num);
    } else {
      *++(*text) = IMM;
      *++(*text) = -1;
      *++(*text) = PUSH;
      expression(Inc, text);
      *++(*text) = MUL;
    }

    expr_type = INT;
  }
  else if (token == Inc || token == Dec) {
    tmp = token;
    match(token);
    expression(Inc, text);

    if (**text == LC) {
      **text = PUSH; // to duplicate the address
      *++(*text) = LC;
    } else if (**text == LI) {
      **text = PUSH;
      *++(*text) = LI;
    } else {
      printf("%d: bad lvalue of pre-increment\n", line);
      exit(-1);
    }
    *++(*text) = PUSH;
    *++(*text) = IMM;

    *++(*text) = (expr_type > PTR) ? sizeof(int) : sizeof(char);
    *++(*text) = (tmp == Inc) ? ADD : SUB;
    *++(*text) = (expr_type == CHAR) ? SC : SI;
  }

  // deal with binary operators
  while (token >= level) {
    // parse token for binary operator and postfix operator
    
    tmp = expr_type;
    if (token == Assign) {
      // var = expr;
      match(Assign);
      if (**text == LC || **text == LI) {
        **text = PUSH; // save the lvalue's pointer
      } else {
        printf("%d: bad lvalue in assignment\n", line);
        exit(-1);
      }
      expression(Assign, text);

      *++(*text) = (expr_type == CHAR) ? SC : SI;
    }
    else if (token == Cond) {
      // expr ? a : b;
      match(Cond);
      *++(*text) = JZ;
      addr = ++(*text);
      expression(Assign, text);
      if (token == ':') {
        match(':');
      } else {
        printf("%d: missing colon in conditional\n", line);
        exit(-1);
      }
      *addr = (int)(*text + 3);
      *++(*text) = JMP;
      addr = ++(*text);
      expression(Cond, text);
      *addr = (int)(*text + 1);
    }
    else if (token == Lor) {
      // logic or
      match(Lor);
      *++(*text) = JNZ;
      addr = ++(*text);
      expression(Lan, text);
      *addr = (int)(*text + 1);
      expr_type = INT;
    }
    else if (token == Lan) {
      // logic and
      match(Lan);
      *++(*text) = JZ;
      addr = ++(*text);
      expression(Or, text);
      *addr = (int)(*text + 1);
      expr_type = INT;
    }
    else if (token == Xor) {
      // bitwise xor
      match(Xor);
      *++(*text) = PUSH;
      expression(And, text);
      *++(*text) = XOR;
      expr_type = INT;
    }
    else if (token == And) {
      // bitwise and
      match(And);
      *++(*text) = PUSH;
      expression(Eq, text);
      *++(*text) = AND;
      expr_type = INT;
    }
    else if (token == Eq) {
      // == operator
      match(Eq);
      *++(*text) = PUSH;
      expression(Ne, text);
      *++(*text) = EQ;
      expr_type = INT;
    }
    else if (token == Ne) {
      // not equal !=
      match(Ne);
      *++(*text) = PUSH;
      expression(Lt, text);
      *++(*text) = NE;
      expr_type = INT;
    }
    else if (token == Lt) {
      // less than
      match(Lt);
      *++(*text) = PUSH;
      expression(Shl, text);
      *++(*text) = LT;
      expr_type = INT;
    }
    else if (token == Gt) {
      // greater than
      match (Gt);
      *++(*text) = PUSH;
      expression(Shl, text);
      *++(*text) = GT;
      expr_type = INT;
    }
    else if (token == Le) {
      // less than or equal to
      match(Le);
      *++(*text) = PUSH;
      expression(Shl, text);
      *++(*text) = LE;
      expr_type = INT;
    }
    else if (token == Ge) {
      // greater than or equal to
      match(Ge);
      *++(*text) = PUSH;
      expression(Shl, text);
      *++(*text) = GE;
      expr_type = INT;
    }
    else if (token == Shl) {
      // shift left
      match(Shl);
      *++(*text) = PUSH;
      expression(Add, text);
      *++(*text) = SHL;
      expr_type = INT;
    }
    else if (token == Shr) {
      // shift right
      match(Shr);
      *++(*text) = PUSH;
      expression(Add, text);
      *++(*text) = SHR;
      expr_type = INT;
    }
    else if (token == Add) {
      // add
      match(Add);
      *++(*text) = PUSH;
      expression(Mul, text);
      expr_type = tmp;

      if (expr_type > PTR) {
        // pointer type, and not 'char *'
        *++(*text) = PUSH;
        *++(*text) = IMM;
        *++(*text) = sizeof(int);
        *++(*text) = MUL;
      }
      *++(*text) = ADD;
    }
    else if (token == Sub) {
      // sub
      match(Sub);
      *++(*text) = PUSH;
      expression(Mul, text);

      if (tmp > PTR && tmp == expr_type) {
        // pointer subtraction
        *++(*text) = SUB;
        *++(*text) = PUSH;
        *++(*text) = IMM;
        *++(*text) = sizeof(int);
        *++(*text) = DIV;
        expr_type = INT;
      } else if (tmp > PTR) {
        // pointer movement
        *++(*text) = PUSH;
        *++(*text) = IMM;
        *++(*text) = sizeof(int);
        *++(*text) = MUL;
        *++(*text) = SUB;
        expr_type = tmp;
      } else  {
        // numerical subrraction
        *++(*text) = SUB;
        expr_type = tmp;
      }
    }
    else if (token == Mul) {
      // multiply
      match(Mul);
      *++(*text) = PUSH;
      expression(Inc, text);
      *++(*text) = MUL;
      expr_type = tmp;
    }
    else if (token == Div) {
      // divide
      match(Div);
      *++(*text) = PUSH;
      expression(Inc, text);
      *++(*text) = DIV;
      expr_type = tmp;
    }
    else if (token == Mod) {
      // Modulo
      match(Mod);
      *++(*text) = PUSH;
      expression(Inc, text);
      *++(*text) = MOD;
      expr_type = tmp;
    }
    else if (token == Inc || token == Dec) {
      if (**text == LI) {
        **text = PUSH;
        *++(*text) = LI;
      }
      else if (**text == LC) {
        **text = PUSH;
        *++(*text) = LC;
      }
      else {
        printf("%d: bad value in increment\n", line);
        exit(-1);
      }

      // postfix version
      *++(*text) = PUSH;
      *++(*text) = IMM;
      *++(*text) = (expr_type > PTR) ? sizeof(int) : sizeof(char);
      *++(*text) = (token == Inc) ? ADD : SUB;
      *++(*text) = (expr_type == CHAR) ? SC : SI;
      *++(*text) = PUSH;                                             //
      *++(*text) = IMM;                                              // Inversion of increment/decrement
      *++(*text) = (expr_type > PTR) ? sizeof(int) : sizeof(char);   //
      *++(*text) = (token == Inc) ? SUB : ADD;                       //
      match(token);
    }
    else if (token == Brak) {
      // array access var[xx]
      match(Brak);
      *++(*text) = PUSH;
      expression(Assign, text);
      match(']');

      if (tmp > PTR) {
        // pointer, 'not char *'
        *++(*text) = PUSH;
        *++(*text) = IMM;
        *++(*text) = sizeof(int);
        *++(*text) = MUL;
      }
      else if (tmp < PTR) {
        printf("%d: pointer type expected\n", line);
        exit(-1);
      }
      expr_type = tmp - PTR;
      *++(*text) = ADD;
      *++(*text) = (expr_type == CHAR) ? LC : LI;
    }
    else {
      printf("%d: compiler error, token = %d\n", line, token);
      exit(-1);
    }
  }
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
    expression(Assign, &text); // parse condition
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
    expression(Assign, &text);
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
      expression(Assign, &text);
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

    match('}');
  }
  else if (token == ';') {
    // empty statement
    match(';');
  }
  else {
    // a = b; or function_call();
    expression(Assign, &text);
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

  while (token == Int || token == Char) {
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
    current_id[Type] = type;

    if (look_ahead('(')) {
      match(Id);
      current_id[Class] = Fun;
      current_id[Value] = (int)(text + 1); // the memory address of function
      function_declaration();
    } else {
      // variable declaration
      current_id[Class] = Glo; // global variable
      current_id[Value] = (int)data; // assign memory address
      data = data + sizeof(int);

      if (look_ahead(Assign)) {
        expression(Assign, &global_var_assign_text);
        break;
      } else {
        match(Id);
      }
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

void log_ptr(int * ptr, size_t size) {
  for (int i = 0; i < size; i++) {
    printf("%x ", *(ptr + i));
    if (i % 8 == 0 && i != 0) {
      printf("\n");
    }
  }
  printf("\n\n");
}

int main(int argc, char **argv) {
  int i, fd;
  int *tmp;
  argc--;
  argv++;

  poolsize = 256 * 1024; // an arbitrary size
  line = 1;

  
  // allocate memory for virtual machine
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
  if (!(symbols = malloc(poolsize))) {
    printf("could not malloc(%d) for symbol table\n", poolsize);
    return -1;
  }

  global_var_assign_text = malloc(poolsize);

  memset(text, 0, poolsize);
  memset(data, 0, poolsize);
  memset(stack, 0, poolsize);
  memset(symbols, 0, poolsize);
  memset(global_var_assign_text, 0, poolsize);

  old_global_assign_text = global_var_assign_text;

  bp = sp = (int *)((int)stack + poolsize);
  ax = 0;

  i = 0; 

  src = "char else enum if int return sizeof while "
        "open read close printf malloc memset memcmp exit void main";
  
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

  program();
  
  if (!(pc = (int *)idmain[Value])) {
    printf("main() not defined\n");
    return -1;
  }
  
  // set up global assignments for jump to main func
  *++global_var_assign_text = JMP;
  *++global_var_assign_text = (int)pc;

  pc = old_global_assign_text + 1;
  
  // setup stack
  sp = (int *)((int)stack + poolsize);
  *--sp = EXIT; // call exit if main returns
  *--sp = PUSH; tmp = sp;
  *--sp = argc;
  *--sp = (int)argv;
  *--sp = (int)tmp;

  log_ptr(pc, global_var_assign_text - old_global_assign_text);
  log_ptr(old_text + 1, text - old_text);

  return eval();
}
