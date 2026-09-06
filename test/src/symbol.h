#ifndef SYMBOL_H
#define SYMBOL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "tree.h"


typedef struct {
    const char *type_name;   // Nom du type (ex : "int", "char", "void")
} TypeInfo;


/* Structure représentant une variable (locale, globale ou paramètre de fonction) */
typedef struct {
    char     name[64];           // Nom de la variable
    TypeInfo type;               // Type de la variable (int, char, void)
    int      lineno, colno;      // Position dans le code source (ligne, colonne)
    int      size;               // Taille mémoire allouée 
    int      offset;             // Décalage par rapport à %rbp 
    bool     used;               // Indique si la variable est effectivement utilisée
    bool     is_static;          // Vrai si variable locale statique (restant en mémoire entre appels)
    char     asm_label[256];     // Étiquette unique générée pour les variables statiques locales
} VarSym;


/* Table des variables (liste dynamique)*/
typedef struct {
    VarSym *data;        // Tableau dynamique de symboles de variables (VarSym)
    int     len;         // Nombre de variables actuellement stockées
    int     cap;         // Capacité maximale actuelle du tableau (avant redimensionnement)
    int     cur_offset;  // Offset courant pour l'allocation des variables locales (en négatif depuis %rbp)
} VarList;


/* Entrée de fonction (contient uniquement les symboles locaux et paramètres) */
typedef struct {
    char    name[64];           // Nom de la fonction
    TypeInfo ret_type;          // Type de retour (int, void...)
    VarList params;             // Paramètres formels
    VarList locals;             // Variables locales
    int     lineno, colno;      // Position dans le code source
    bool    used;               // Fonction utilisée ou non
    bool    has_return;         // Présence explicite de 'return'
    int     locals_bytes;       // Taille totale (en octets) de la pile locale, alignée
} FuncSym;

/* Table des symboles principale : variables globales + fonctions */
typedef struct {
    VarList  globals;          // Liste des variables globales
    FuncSym *funcs;            // Tableau dynamique des fonctions définies dans le programme
    int      func_len;         // Nombre de fonctions actuellement enregistrées
    int      func_cap;         // Capacité actuelle du tableau 'funcs' (avant redimensionnement)
    uint32_t n_error;          // Nombre total d’erreurs détectées
    uint32_t n_warning;        // Nombre d’avertissements générés
} Symtab;


// Libère toute la mémoire associée à la table des symboles (variables, fonctions, etc.)
void st_free(Symtab *st);


// Affiche le contenu de la table des symboles pour le débogage (variables globales, fonctions, offsets...)
void st_dump(const Symtab *st);


// Parcourt l'arbre syntaxique (AST) et remplit la table des symboles (déclarations de variables et fonctions)
void build_symbols(Symtab *st, Node *root);


// Effectue les vérifications sémantiques sur les utilisations de variables et fonctions (existence, types, portée, etc.)
void check_usage(Node *n, Symtab *st, FuncSym *cur_fn, bool in_decl);


// Signale les variables ou fonctions déclarées mais jamais utilisées (génère des avertissements)
void report_unused(Symtab *st);


// Recherche une variable par son nom dans la portée donnée (fonction ou globale)
VarSym *st_lookup_var(const Symtab *st,const FuncSym *scope,const char *name);


// Recherche une fonction par son nom dans la table des symboles
FuncSym *st_lookup_func(const Symtab *st, const char *name);


// Initialise une table des symboles vide
void st_init(Symtab *st);

// Déduit le type d’une expression à partir de l’AST et de la table des symboles
TypeInfo infer_expr_type(Node *e, Symtab *st, FuncSym *cur_fn);

#endif