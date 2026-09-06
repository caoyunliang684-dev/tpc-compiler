/* tree.c */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tree.h"
extern int yylineno;      

static const char *StringFromLabel[] = {
    "n_Prog",          
    "n_DeclVars",      
    "n_Declarateurs",  
    "n_GlobalVarDecl",
    "n_DeclFoncts",    
    "n_DeclFonct",      
    "n_EnTeteFonct",   
    "n_Parametres",    
    "n_ListTypVar",   
    "n_Corps",         
    "n_Instr",       
    "n_Exp",                     
    "n_ListExp",       
    "n_Arguments",      
    "n_ADDSUB",         
    "n_DIVSTAR",
    "n_NOT",            
    "n_AND",            
    "n_OR",            
    "n_EQ",             
    "n_ORDER",          
    "n_ASSIGN",         
    "n_TYPE",          
    "n_void",           
    "n_IDENT",         
    "n_NUMBER",        
    "n_CHAR",          
    "n_IF",            
    "n_ELSE",           
    "n_WHILE",          
    "n_RETURN",         
    "n_STATIC",         
    "n_SEMICOLON",      
    "n_LBRACE",         
    "n_RBRACE"          
};


Node *makeNode(label_t label) {
  Node *node = malloc(sizeof(Node));
  if (!node) {
    printf("Run out of memory\n");
    exit(1);
  }
  node->label = label;
  node-> firstChild = node->nextSibling = NULL;
  node->lineno=yylineno;
  return node;
}

Node *makeNodeWithText(label_t label, const char *text) {
    Node *node = makeNode(label);   
    if (text) {
        node->text = strdup(text);  
    }
    return node;
}


void addSibling(Node *node, Node *sibling) {
  Node *curr = node;
  while (curr->nextSibling != NULL) {
    curr = curr->nextSibling;
  }
  curr->nextSibling = sibling;
}

void addChild(Node *parent, Node *child) {
  if (parent->firstChild == NULL) {
    parent->firstChild = child;
  }
  else {
    addSibling(parent->firstChild, child);
  }
}

void deleteTree(Node *node) {
  if (node->firstChild) {
    deleteTree(node->firstChild);
  }
  if (node->nextSibling) {
    deleteTree(node->nextSibling);
  }
  if (node->text) free(node->text);  
  free(node);
}

void printTree(Node *node) {
    static bool rightmost[128];
    static int depth = 0;      
    for (int i = 1; i < depth; i++) { 
        printf(rightmost[i] ? "    " : "\u2502   "); 
    }
    if (depth > 0) { 
        printf(rightmost[depth] ? "\u2514\u2500\u2500 " : "\u251c\u2500\u2500 ");
    }
    printf("%s", StringFromLabel[node->label]);
    if (node->text != NULL) {
        printf(" (%s)", node->text);
    }
    printf("\n");
    depth++;
    for (Node *child = node->firstChild; child != NULL; child = child->nextSibling) {
        rightmost[depth] = (child->nextSibling) ? false : true;
        printTree(child);
    }
    depth--; 
}

