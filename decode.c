#define _GNU_SOURCE
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// instructions
enum { LEA ,IMM ,JMP ,CALL,JZ  ,JNZ ,ENT ,ADJ ,LEV ,LI  ,LC  ,SI  ,SC  ,PUSH,
       OR  ,XOR ,AND ,EQ  ,NE  ,LT  ,GT  ,LE  ,GE  ,SHL ,SHR ,ADD ,SUB ,MUL ,DIV ,MOD ,
       OPEN,READ,CLOS,PRTF,MALC,MSET,MCMP,EXIT };

int has_data(int instr) {
  switch (instr) {
    case(IMM): return 1;
    case(JMP): return 1;
    case(JZ): return 1;
    case(JNZ): return 1;
    case(CALL): return 1;
    case(ENT): return 1;
    case(ADJ): return 1;
    case(LEA): return 1;
    default: return 0;
  }
}

void print_instr(int instr, char* data, size_t datalen) {
  char *string = "Invalid Data";
  int prnt_data = 0;
  switch (instr) {
    case (IMM):  string = "IMM"; prnt_data = 1; break; 
    case (LC):   string = "LC"; break;
    case (LI):   string = "LI"; break;
    case (SC):   string = "SC";  break;
    case (SI):   string = "SI";  break;
    case (PUSH): string = "PUSH";  break;
    case (JMP):  string = "JMP"; prnt_data = 1; break; 
    case (JZ):   string = "JZ"; prnt_data = 1; break; 
    case (JNZ):  string = "JNZ"; prnt_data = 1; break; 
    case (CALL): string = "CALL"; prnt_data = 1; break; 
    case (ENT):  string = "ENT"; prnt_data = 1; break; 
    case (ADJ):  string = "ADJ"; prnt_data = 1; break; 
    case (LEV):  string = "LEV";  break;
    case (LEA):  string = "LEA"; prnt_data = 1; break; 
    case (OR):   string = "OR";  break;
    case (XOR):  string = "XOR";  break;
    case (AND):  string = "AND";  break;
    case (EQ):   string = "EQ";  break;
    case (NE):   string = "NE";  break;
    case (LT):   string = "LT";  break;
    case (LE):   string = "LE";  break;
    case (GT):   string = "GT";  break;
    case (GE):   string = "GE";  break;
    case (SHL):  string = "SHL";  break;
    case (SHR):  string = "SHR";  break;
    case (ADD):  string = "ADD";  break;
    case (SUB):  string = "SUB";  break;
    case (MUL):  string = "MUL";  break;
    case (DIV):  string = "DIV";  break;
    case (MOD):  string = "MOD";  break;
    case (EXIT): string = "EXIT";  break;
    case (OPEN): string = "OPEN";  break;
    case (CLOS): string = "CLOS";  break;
    case (READ): string = "READ";  break;
    case (PRTF): string = "PRTF";  break; 
    case (MALC): string = "MALC";  break;
    case (MSET): string = "MSET";  break;
    case (MCMP): string = "MCMP";  break;
  }

  printf("%s ", string);
  if (prnt_data) {
    for (int i = 0; i < datalen; i++) {
      printf("%c", *(data+i));
    }
  }
  printf("\n");
  return;
}

int main(int argc, char *argv[]) {
  int fd;
  char *line = NULL;
  char *line_ptr;
  
  FILE* file;
  
  if (argc < 2) {
    printf("Must provide a valid path\n");
    return 1;
  }
  if (!(file = fopen(argv[1], "r"))) {
    printf("File '%s' not found\n", argv[1]);
    return 1;
  }
  
  int instr = -1;
  char *data;
  int datalen;
  char *current_char;

  size_t linecap = 0;
  ssize_t linelen = 0;
  while ((linelen = getline(&line, &linecap, file)) != -1) {
    line_ptr = line;
    current_char = line;


    while (*current_char != '\n') {
      while (*current_char != ' ' && *current_char != '\n') {
        current_char++;
        if (current_char >= line_ptr + linelen) {
          fclose(file);
          return 0;
        }
      }

      if (instr == -1) {
        instr = strtol(line_ptr, &current_char, 16);
        if(!has_data(instr)) {
          print_instr(instr, data, datalen);
          instr = -1;
        }
      } else {
        data = line_ptr;
        datalen = current_char - line_ptr;
        print_instr(instr, data, datalen);
        instr = -1;
      }
      
      current_char++;
      line_ptr = current_char;
    }

  }

  fclose(file);

  return 0;

}
