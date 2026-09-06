/* tree.h */
#ifndef TREE_H
#define TREE_H

typedef enum {
    n_Prog,           
    n_DeclVars,        
    n_Declarateurs,    
    n_GlobalVarDecl,
    n_DeclFoncts,     
    n_DeclFonct,       
    n_EnTeteFonct,     
    n_Parametres,     
    n_ListTypVar,     
    n_Corps,              
    n_Instr,          
    n_Exp,                         
    n_ListExp,        
    n_Arguments,       
    n_ADDSUB,        
    n_DIVSTAR,
    n_NOT,            
    n_AND,            
    n_OR,              
    n_EQ,            
    n_ORDER,           
    n_ASSIGN,         
    n_TYPE,           
    n_void,
    n_IDENT,         
    n_NUMBER,       
    n_CHAR,      
    n_IF,          
    n_ELSE,          
    n_WHILE,          
    n_RETURN,         
    n_STATIC,
    n_SEMICOLON,                      
    n_LBRACE,         
    n_RBRACE          
} label_t;


typedef struct Node {
  label_t label;
  struct Node *firstChild, *nextSibling;
  int lineno;
  char *text;         
} Node;

Node *makeNode(label_t label);
Node *makeNodeWithText(label_t label, const char *text);
void addSibling(Node *node, Node *sibling);
void addChild(Node *parent, Node *child);
void deleteTree(Node*node);
void printTree(Node *node);

#define FIRSTCHILD(node) node->firstChild
#define SECONDCHILD(node) node->firstChild->nextSibling
#define THIRDCHILD(node) node->firstChild->nextSibling->nextSibling

#endif