%{
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include "tree.h"
#include "symbol.h"
#include "nasm.h"

extern int yylex();
extern int yylineno;
extern int yycolumn;
extern int yydebug;
int yyerror(const char *s);

int print_tree = 0;
int print_symtab = 0;
const char *output_filename = "_anonymous.asm";
Node *gAstRoot = NULL;
%}

%debug

%union {
    int  num;
    char *str;
    Node *node; 
}


%token <str> TYPE
%token <str> VOID
%token <str> IDENT
%token <num> NUM
%token <num>  CHARACTER

%token RETURN IF ELSE WHILE STATIC
%token <str> EQ        /* ==,!=*/
%token <str> ORDER     /* <, <=, >, >= */
%token <str> ADDSUB    /* +, - */
%token <str> DIVSTAR   /* *, /, % */
%token <str> NOT       /* ! */
%token <str> AND       /* && */
%token <str> OR        /* || */
%token <str> ASSIGN    /* = */

%token <str> SEMICOLON /* ; */
%token <str> COMMA     /* , */
%token <str> LPAREN    /* ( */
%token <str> RPAREN    /* ) */
%token <str> LBRACE    /* { */
%token <str> RBRACE    /* } */


%precedence IF
%nonassoc ELSE
%left OR
%left AND
%left EQ
%left ORDER
%left ADDSUB
%left DIVSTAR
%right NOT

%type <node> Prog DeclVars GlobalVarDecl Declarateurs Declarator DeclFoncts DeclFonct EnTeteFonct Parametres ListTypVar Corps DeclVarsLocal OneLocalDecl StaticOpt SuiteInstr Instr Exp TB FB M E T F Arguments ListExp

%start Prog

%%

Prog:
    DeclVars DeclFoncts
    {
      Node *root = makeNode(n_Prog);
      addChild(root, $1);
      addChild(root, $2);
      gAstRoot = root; 
      $$ = root;
    }
    ;


DeclVars:
    /* empty */
    {
      Node *node = makeNode(n_DeclVars);
      $$ = node;
    }
  | DeclVars GlobalVarDecl
    {
      if ($1 == NULL) {
        $$ = $2;
      } else {
        addSibling($1, $2);
        $$ = $1;
      }
    }
    ;


GlobalVarDecl:
    TYPE Declarateurs SEMICOLON
    {
      Node *node = makeNode(n_GlobalVarDecl); 
      Node *typeNode = makeNodeWithText(n_TYPE, $1);
      addChild(node, typeNode);
      addChild(node, $2);
      $$ = node;
    }
    ;


Declarateurs
    : Declarateurs COMMA Declarator
      { addChild($1, $3); $$ = $1; }
    | Declarator
      {
        Node *list = makeNode(n_Declarateurs);
        addChild(list, $1);
        $$ = list;
      }
    ;

Declarator
    : IDENT                          
      {
        $$ = makeNodeWithText(n_IDENT, $1);
      }
    ;



DeclFoncts:
    DeclFoncts DeclFonct
    {
      Node *list = $1; 
      addChild(list, $2);
      $$ = list;
    }
  | DeclFonct
    {
      Node *list = makeNode(n_DeclFoncts);
      addChild(list, $1);
      $$ = list;
    }
    ;

DeclFonct:
    EnTeteFonct Corps
    {
      Node *node = makeNode(n_DeclFonct);
      addChild(node, $1); 
      addChild(node, $2);  
      $$ = node;
    }
    ;

EnTeteFonct:
    TYPE IDENT LPAREN Parametres RPAREN
    {
      Node *node = makeNode(n_EnTeteFonct);
      Node *typeNode = makeNodeWithText(n_TYPE, $1);
      Node *identNode = makeNodeWithText(n_IDENT, $2);
      addChild(node, typeNode);
      addChild(node, identNode);
      addChild(node, $4);
      $$ = node;
    }
  | VOID IDENT LPAREN Parametres RPAREN
    {
      Node *node = makeNode(n_EnTeteFonct);
      Node *voidNode = makeNode(n_void);
      addChild(node, voidNode);
      Node *identNode = makeNodeWithText(n_IDENT, $2);
      addChild(node, identNode);
      addChild(node, $4); 
      $$ = node;
    }
    ;


Parametres:
    VOID
    {
      Node *node = makeNode(n_Parametres);
      Node *voidNode = makeNode(n_void);
      addChild(node, voidNode);
      $$ = node;
    }
  | ListTypVar
    {
      Node *node = makeNode(n_Parametres);
      addChild(node, $1);
      $$ = node;
    }
    ;

ListTypVar
    : ListTypVar COMMA TYPE Declarator
      {
          Node *list  = $1;                                 
          Node *param = makeNode(n_ListTypVar);             

          Node *typeNode = makeNodeWithText(n_TYPE, $3);    
          addChild(param, typeNode);                       
          addChild(param, $4);                            

          addChild(list, param);                          
          $$ = list;
      }
    | TYPE Declarator
      {
          Node *list  = makeNode(n_ListTypVar);             
          Node *param = makeNode(n_ListTypVar);

          Node *typeNode = makeNodeWithText(n_TYPE, $1);
          addChild(param, typeNode);
          addChild(param, $2);

          addChild(list, param);
          $$ = list;
      }
    ;



Corps:
    LBRACE DeclVarsLocal SuiteInstr RBRACE
    {
      Node *node = makeNode(n_Corps);
      addChild(node, $2); 
      addChild(node, $3); 
      $$ = node;
    }
    ;


DeclVarsLocal:
    /* empty */ { $$ = NULL; }
  | DeclVarsLocal OneLocalDecl
    {
      if ($1 == NULL) {
        $$ = $2;
      } else {
        addSibling($1, $2);
        $$ = $1;
      }
    }
    ;


OneLocalDecl:
    StaticOpt TYPE Declarateurs SEMICOLON
    {
      Node *node = makeNode(n_DeclVars);
      if ($1 != NULL) { 
        addChild(node, $1);
      }
      Node *typeNode = makeNodeWithText(n_TYPE, $2);
      addChild(node, typeNode);
      addChild(node, $3);
      $$ = node;
    }
    ;

StaticOpt:
    STATIC
    {
      Node *node = makeNodeWithText(n_STATIC, "static");
      $$ = node;
    }
  | /* empty */
    {
      $$ = NULL;
    }
    ;


SuiteInstr:
    SuiteInstr Instr
    {
      if ($1 == NULL) {
        $$ = $2;
      } else {
        addSibling($1, $2);
        $$ = $1;
      }
    }
  | /* empty */ 
    { 
      $$ = NULL; 
    }
  ;



Instr:
   IDENT ASSIGN Exp SEMICOLON
    {
      Node *node = makeNode(n_Instr);
      Node *identNode = makeNodeWithText(n_IDENT, $1);
      Node *assignNode = makeNodeWithText(n_ASSIGN,$2);
      addChild(node, identNode);
      addChild(node, assignNode);
      addChild(node, $3);
      $$ = node;
    }
  | IF LPAREN Exp RPAREN Instr %prec IF
    {
      Node *node = makeNode(n_IF);
      addChild(node, $3);
      addChild(node, $5);
      $$ = node;
    }
  | IF LPAREN Exp RPAREN Instr ELSE Instr
    {
      Node *node = makeNode(n_IF); 
      addChild(node, $3);
      addChild(node, $5);
      Node *elseNode = makeNode(n_ELSE);
      addChild(elseNode, $7);
      addChild(node, elseNode);
      $$ = node;
    }
  | WHILE LPAREN Exp RPAREN Instr
    {
      Node *node = makeNode(n_WHILE);
      addChild(node, $3);
      addChild(node, $5);
      $$ = node;
    }
  | IDENT LPAREN Arguments RPAREN SEMICOLON
    {
      Node *node = makeNode(n_Instr);
      Node *call = makeNodeWithText(n_IDENT,$1);
      addChild(call, $3);
      addChild(node, call);
      $$ = node;
    }
  | RETURN Exp SEMICOLON
    {
      Node *node = makeNode(n_RETURN);
      addChild(node, $2);
      $$ = node;
    }
  | RETURN SEMICOLON
    {
      Node *node = makeNode(n_RETURN);
      $$ = node;
    }
  | LBRACE SuiteInstr RBRACE
    {
      Node *node = makeNode(n_LBRACE);
      Node *R = makeNode(n_RBRACE);
      addChild(node, $2);
      addSibling(node, R);
      $$ = node;
    }
  | SEMICOLON
    {
      Node *node = makeNode(n_SEMICOLON);
      $$ = node;
    }
    ;


Exp:
    Exp OR TB
    {
      Node *node = makeNode(n_OR);
      addChild(node, $1);
      addChild(node, $3);
      $$ = node;
    }
  | TB
    {
      $$ = $1;
    }
    ;

TB:
    TB AND FB
    {
      Node *node = makeNode(n_AND);
      addChild(node, $1);
      addChild(node, $3);
      $$ = node;
    }
  | FB
    {
      $$ = $1;
    }
    ;

FB:
    FB EQ M
    {
      Node *eqNode = makeNodeWithText(n_EQ,$2);
      addChild(eqNode, $1);
      addChild(eqNode, $3);
      $$ = eqNode;
    }
  | M
    {
      $$ = $1;
    }
    ;

M:
    M ORDER E
    {
      Node *node = makeNodeWithText(n_ORDER,$2);
      addChild(node, $1);
      addChild(node, $3);
      $$ = node;
    }
  | E
    {
      $$ = $1;
    }
    ;

E:
   E ADDSUB T
    {
      Node *node = makeNodeWithText(n_ADDSUB,$2);
      addChild(node, $1);
      addChild(node, $3);
      $$ = node;
    }
  | T
    ;

T:
    T DIVSTAR F
    {
      Node *node = makeNodeWithText(n_DIVSTAR,$2);
      addChild(node, $1);
      addChild(node, $3);
      $$ = node;
    }
  | F
    {
      $$ = $1;
    }
    ;

F:
    ADDSUB F
    {
      Node *node = makeNodeWithText(n_ADDSUB,$1);
      addChild(node, $2);
      $$ = node;
    }
  | NOT F
    {
      Node *node = makeNode(n_NOT);
      addChild(node, $2);
      $$ = node;
    }
  | LPAREN Exp RPAREN
    {
      $$ = $2;
    }
  | NUM
    {
      Node *node = makeNode(n_NUMBER);
      char buf[32];
      sprintf(buf, "%d", $1);
      node->text = strdup(buf);
      $$ = node;
    }
  | CHARACTER
    {
      Node *node = makeNode(n_CHAR);
      node->text = strdup(yylval.str);       
      $$ = node;
    }
  | IDENT
    {
      Node *node = makeNodeWithText(n_IDENT, $1);
      $$ = node;
    }
  | IDENT LPAREN Arguments RPAREN
    {
      Node *node = makeNode(n_Exp);
      Node *call = makeNodeWithText(n_IDENT, $1); 
      addChild(call, $3); 
      addChild(node, call);
      $$ = node;
    }
    ;


Arguments:
    ListExp
    {
      $$ = makeNode(n_Arguments);
      addChild($$, $1);
    }
  | /* empty */
    {
      Node *args = makeNode(n_Arguments);
      Node *list = makeNode(n_ListExp);   
      addChild(args, list);
      $$ = args;
    }
    ;


ListExp:
    ListExp COMMA Exp
    {
      Node *list = $1;  
      addChild(list, $3);
      $$ = list;
    }
  | Exp
    {
      Node *list = makeNode(n_ListExp);
      addChild(list, $1);
      $$ = list;
    }
    ;

%%

void print_help() {
    printf("Usage: tpcas [OPTIONS] [< input_file]\n");
    printf("Options:\n");
    printf("  -t, --tree         Print abstract syntax tree.\n");
    printf("  -d, --debug        Enable debug mode for parsing.\n");
    printf("  -h, --help         Display this help and exit.\n");
    printf("Input File:\n");
    printf("  Use '< input_file' to provide input via redirection.\n");
}

int handle_args_error() {
    fprintf(stderr, "Invalid arguments. Use -h for help.\n");
    return 3; 
}

int yyerror(const char *s){
    fprintf(stderr,"Syntax Error at line %d, near column %d\n",yylineno,yycolumn);
    return 1;
}


int main(int argc, char *argv[]) {
    int opt;
    yydebug = 0;

    static struct option long_opts[] = {
        {"tree", no_argument, 0, 't'},
        {"symtabs", no_argument, 0, 's'},
        {"debug", no_argument, 0, 'd'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    while ((opt = getopt_long(argc, argv, "tsdh", long_opts, NULL)) != -1) {
        switch (opt) {
            case 't': print_tree = 1; break;
            case 's': print_symtab = 1; break;
            case 'd': yydebug = 1; break;
            case 'h': print_help(); return 0;
            default: return handle_args_error();
        }
    }

    FILE *fin = NULL;
    if (optind < argc) {
        fin = fopen(argv[optind], "r");
        if (!fin) {
            perror("Cannot open input file");
            return 3;
        }
        extern FILE *yyin;
        yyin = fin;
        static char asm_name[256];
        const char *inname = argv[optind];
        const char *dot = strrchr(inname, '.');
        size_t len = dot ? (size_t)(dot - inname) : strlen(inname);
        snprintf(asm_name, sizeof(asm_name), "%.*s.asm", (int)len, inname);
        output_filename = asm_name;
    }

    if (yyparse() == 0) {
        Symtab st;
        st_init(&st);
        build_symbols(&st, gAstRoot);
        check_usage(gAstRoot, &st, NULL, false);
        report_unused(&st);
        fprintf(stderr, "summary: %d errors, %d warnings\n", st.n_error, st.n_warning);

        if (print_symtab){
            st_dump(&st);
        }
        if (print_tree && gAstRoot){
            printTree(gAstRoot);
        }
        if (st.n_error == 0) {
            set_output_filename(output_filename);
            generate_nasm(&st, gAstRoot);
            printf("Parsing completed successfully.\n");
        } else {
            return 2;
        }
        st_free(&st);
        if (fin) {
          fclose(fin);
        }
        return 0;
    } else {
        if (fin) {
          fclose(fin);
        }
        printf("Parsing failed.\n");
        return 1;
    }
}
