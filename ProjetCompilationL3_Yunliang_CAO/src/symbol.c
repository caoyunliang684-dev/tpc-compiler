#include "symbol.h"
#include "tree.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define INIT_CAP 16

// Signale une erreur sémantique : incrémente le compteur d'erreurs et affiche un message formaté sur stderr.
#define ERR(st, fmt, ...)   do { \
    (st)->n_error++; \
    fprintf(stderr, "error: " fmt "\n", ##__VA_ARGS__); \
} while(0)

// Signale un avertissement : incrémente le compteur de warnings et affiche un message formaté sur stderr.
#define WARN(st, fmt, ...)  do { \
    (st)->n_warning++; \
    fprintf(stderr, "warning: " fmt "\n", ##__VA_ARGS__); \
} while(0)


static const char *BUILTINS[] = {
    "getint", "putint", "getchar", "putchar"
};

// Vérifie si un nom donné correspond à une fonction prédéfinie (builtin).
// Retourne vrai si 'name' est présent dans la liste BUILTINS[], sinon faux.
static bool is_builtin(const char *name)
{
    for (size_t i = 0; i < 4; ++i){
        if (strcmp(name, BUILTINS[i]) == 0){
            return true;
        }
    }
    return false;
}



// Recherche une fonction par son nom dans la table des symboles.
// Retourne un pointeur vers l'entrée de fonction correspondante si elle existe, sinon NULL.
FuncSym *st_lookup_func(const Symtab *st, const char *name) {
    for (int i = 0; i < st->func_len; ++i) {
        if (strcmp(st->funcs[i].name, name) == 0) {
            return &((FuncSym *)st->funcs)[i];
        }
    }
    return NULL;
}




// Initialise une VarList : allocation du tableau et mise à zéro des champs.
// Utilisée pour les listes de variables locales, globales ou paramètres.
void vl_init(VarList *vl) {
    vl->data = NULL;
    vl->len  = vl->cap = 0;
    vl->cap  = INIT_CAP; 
    vl->len  = 0;
    vl->data = malloc(vl->cap * sizeof(VarSym)); 
    vl->cur_offset = 0; 
    if (!vl->data) {
        perror("malloc");
        exit(3);
    }
}


// Recherche l’index d’une variable dans la VarList par son nom.
// Retourne l’index si trouvée, sinon -1.
int vl_find(const VarList *vl, const char *name) {
    for (int i = 0; i < vl->len; ++i) {
        if (strcmp(vl->data[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}


 //Utilitaire interne : garantit la capacité du VarList
static void ensure_varlist_cap(VarList *vl) {
    if (vl->len < vl->cap) return; 
    int new_cap = vl->cap ? vl->cap * 2 : INIT_CAP;  
    VarSym *tmp = realloc(vl->data, new_cap * sizeof(VarSym));
    if (!tmp) { 
        perror("realloc"); 
        exit(3); 
    }
    vl->data = tmp;   
    vl->cap  = new_cap;
}

// Ajoute une variable à la VarList (après avoir garanti la capacité).
// Retourne toujours 1.
int vl_add(VarList *vl, VarSym v) {
    ensure_varlist_cap(vl);    
    vl->data[vl->len++] = v;   
    return 1;
}


// Libère la mémoire allouée pour une VarList et réinitialise ses champs.
// À utiliser en fin de portée (fonction, global, etc.).
void vl_free(VarList *vl) {
    free(vl->data);      
    vl->data = NULL;
    vl->len = vl->cap = 0;  
}

// Recherche une variable par son nom dans un contexte donné (local puis global).
// - Si 'scope' est non NULL, cherche d'abord dans les variables locales et les paramètres.
// - Sinon (ou si non trouvé localement), cherche dans les variables globales.
// Retourne un pointeur vers la VarSym trouvée, ou NULL si introuvable.
VarSym *st_lookup_var(const Symtab *st,
                      const FuncSym *scope,
                      const char *name)
{
    if (scope) {
        int idx = vl_find(&scope->locals, name);
        if (idx != -1) {
            return &scope->locals.data[idx];
        }
        idx = vl_find(&scope->params, name);
        if (idx != -1) {
            return &scope->params.data[idx];
        }
    }
    int gidx = vl_find(&st->globals, name);
    return gidx == -1 ? NULL : &st->globals.data[gidx];
}


// Initialise la table des symboles principale :
// - alloue les structures pour les fonctions et les variables globales,
// - initialise les compteurs d’erreurs et d’avertissements.
void st_init(Symtab *st) {
    vl_init(&st->globals);        // Initialise la liste des variables globales
    st->n_error   = 0;            // Réinitialise le compteur d'erreurs
    st->n_warning = 0;            // Réinitialise le compteur d'avertissements
    st->func_cap  = INIT_CAP;     // Capacité initiale du tableau de fonctions
    st->func_len  = 0;            // Aucune fonction enregistrée pour l’instant
    st->funcs     = malloc(st->func_cap * sizeof(FuncSym)); // Allocation du tableau de fonctions
    if (!st->funcs) {
        perror("malloc");
        exit(3);
    }
}

// Garantit que la table des fonctions (st->funcs) a une capacité suffisante.
// Si elle est pleine, double la capacité via realloc.
static void ensure_func_cap(Symtab *st) {
    int new_cap = st->func_cap ? st->func_cap * 2 : INIT_CAP;  
    FuncSym *tmp = realloc(st->funcs, new_cap * sizeof(FuncSym));
    if (!tmp) {
        perror("realloc");
        exit(3);
    }
    st->funcs = tmp;           
    st->func_cap = new_cap;    
}


// Ajoute une variable globale dans la table des symboles.
// - Vérifie les redéfinitions de variables globales.
// - Vérifie les conflits avec les noms de fonctions ou de fonctions prédéfinies (builtins).
// Retourne 1 si l’ajout a réussi, 0 sinon.
int st_add_global(Symtab *st, VarSym v)
{
    if (vl_find(&st->globals, v.name) != -1) {
        ERR(st, "redeclaration of global '%s' at line %d", v.name,v.lineno);  // Déjà déclaré comme variable globale
        return 0;
    }
    if (st_lookup_func(st, v.name) || is_builtin(v.name)) {
        ERR(st, "global '%s' conflicts with function name at line %d", v.name,v.lineno); // Conflit avec une fonction
        return 0;
    }
    vl_add(&st->globals, v);  // Ajout à la liste des variables globales
    return 1;
}



// Ajoute une fonction dans la table des symboles.
// - Vérifie les conflits avec les fonctions déjà déclarées, les builtins et les variables globales.
// - Initialise les listes de paramètres et de variables locales.
// Retourne 1 si l’ajout est réussi, sinon 0.
int st_add_func(Symtab *st, FuncSym f)
{
    if (is_builtin(f.name)) {
        ERR(st, "redefinition of builtin function '%s' at line %d", f.name,f.lineno);  // Conflit avec une fonction prédéfinie
        return 0;
    }

    if (st_lookup_func(st, f.name)) {
        ERR(st, "redefinition of function '%s' at line %d", f.name,f.lineno);  // Fonction déjà déclarée
        return 0;
    }

    if (st_lookup_var(st, NULL, f.name)) {
        ERR(st, "function '%s' conflicts with global variable at line %d", f.name,f.lineno);  // Conflit avec une variable globale
        return 0;
    }

    ensure_func_cap(st);       // Agrandit le tableau de fonctions si besoin
    vl_init(&f.params);        // Initialise la liste des paramètres
    f.params.cur_offset = 0;   // À compléter plus tard selon l’appel
    vl_init(&f.locals);        // Initialise la liste des variables locales
    f.locals.cur_offset = 0;   // Pour allocation sur la pile

    st->funcs[st->func_len++] = f;  // Ajoute la fonction au tableau
    return 1;
}


// Libère toute la mémoire associée à la table des symboles principale.
// - Libère les variables globales.
// - Libère les paramètres et variables locales de chaque fonction.
// - Réinitialise tous les champs.
void st_free(Symtab *st) {
    vl_free(&st->globals);  // Libère la liste des variables globales

    for (int i = 0; i < st->func_len; ++i) {
        vl_free(&st->funcs[i].params);  // Libère les paramètres de la fonction
        vl_free(&st->funcs[i].locals);  // Libère les variables locales de la fonction
    }

    free(st->funcs);              // Libère le tableau des fonctions
    st->funcs = NULL;
    st->func_len = st->func_cap = 0;  // Réinitialisation
}


// Affiche le contenu d’une VarList (variables) avec indentation, pour le débogage.
// - Affiche : nom, type, taille, offset, et ligne de déclaration.
// - 'indent' permet d’ajuster la mise en forme (par ex. "    ")
static void dump_varlist(const VarList *vl, const char *indent) {
    for (int i = 0; i < vl->len; ++i) {
        VarSym *v = &vl->data[i];
        printf("%s%-10s : %s  size : %d offset : %d (line %d)\n",
               indent,
               v->name,
               v->type.type_name,
               v->size,
               v->offset,
               v->lineno);
    }
}

// Affiche tout le contenu de la table des symboles pour le débogage :
// - variables globales
// - fonctions, avec leurs paramètres et variables locales
void st_dump(const Symtab *st) {
    puts("=== Global variables ===");
    dump_varlist(&st->globals, "  ");  // Affiche les variables globales

    puts("\n=== Functions ===");
    for (int i = 0; i < st->func_len; ++i) {
        FuncSym *f = &st->funcs[i];
        printf("- %s()  returns %s  (line %d)\n",
               f->name, f->ret_type.type_name, f->lineno);

        puts("  > params:");
        dump_varlist(&f->params, "    ");  // Paramètres de la fonction

        puts("  > locals:");
        dump_varlist(&f->locals, "    ");  // Variables locales

        putchar('\n');  // Séparation entre fonctions
    }
}



// Extrait le type depuis un nœud de type dans l’AST et le convertit en TypeInfo.
// - Accepte les nœuds de type : n_TYPE (ex : "int", "char"), ou n_void ("void").
// - Par défaut (ou si erreur), retourne "int".
static TypeInfo tpc_type(Node *typeNode)
{
    if (!typeNode) {
        return (TypeInfo){ "int" };  // Valeur par défaut de secours
    }

    switch (typeNode->label) {
    case n_TYPE:
        return (TypeInfo){ typeNode->text };  // Exemple : "int", "char"

    case n_void:
        return (TypeInfo){ "void" };

    default:
        fprintf(stderr,
                "tpc_type: unexpected label %d at line %d\n",
                typeNode->label, typeNode->lineno);
        return (TypeInfo){ "int" };  // Sécurité en cas de structure AST incorrecte
    }
}





// Insère récursivement tous les paramètres (n_ListTypVar) dans la fonction donnée.
// - Chaque nœud de type (TYPE IDENT) est transformé en VarSym et ajouté à fn->params.
// - Assigne un offset mémoire (pile ou registre) selon l’indice du paramètre.
// - Signale les redéfinitions, les void illégaux, ou les conflits avec des fonctions.
static void insert_param_list(Node *list, FuncSym *fn,
    Symtab *st, int *idx)
{
    if (!list) return;

    if (list->label == n_ListTypVar) {
        Node *first  = FIRSTCHILD(list);                  // Doit être un n_TYPE
        Node *second = first ? first->nextSibling : NULL; // Doit être un n_IDENT

        // Cas feuille : n_ListTypVar ::= n_TYPE n_IDENT
        if (first && first->label == n_TYPE &&
            second && second->label == n_IDENT)
        {
            // Vérifie redéfinition du nom
            if (vl_find(&fn->params, second->text) != -1) {
                ERR(st, "duplicate parameter '%s' in %s() at line %d",
                    second->text, fn->name,second->lineno);
                return;
            }

            // Construction du symbole de paramètre 
            VarSym p = {0};
            strcpy(p.name, second->text);
            TypeInfo ti = tpc_type(first);

            if (st_lookup_func(st, p.name) || is_builtin(p.name)) {
                WARN(st, "parameter '%s' conflicts with function name (line %d)",
                     p.name, p.lineno);
            }

            p.type   = ti;
            p.size   = (strcmp(ti.type_name, "char") == 0) ? 1 : 4;
            p.lineno = second->lineno;

            // Affectation offset et registre
            if (*idx < 6) {
                // Paramètres transmis par registre (rdi à r9)
                p.offset   = -8 * (*idx + 1);  // Empilé à l'entrée : rbp-8, -16, ..., -48
            } else {
                // Paramètres transmis sur la pile (appelant)
                p.offset   = 16 + 8 * (*idx - 6);  // rbp+16, +24, ...
            }

            vl_add(&fn->params, p);
            (*idx)++;  // Passe au paramètre suivant
        }
        else {
            // Cas récursif : liste de paramètres
            for (Node *c = first; c; c = c->nextSibling){
                insert_param_list(c, fn, st, idx);
            }
        }
    }
}



// Insère les variables déclarées (n_DeclVars) dans la fonction courante :
// - Gère les déclarations multiples séparées par virgules (int x, y, z;)
// - Différencie les variables locales normales et les variables locales static
// - Vérifie les redéfinitions dans les paramètres et locaux
// - Attribue un offset mémoire pour les variables normales
static void insert_declvars(Node *dv, Symtab *st, FuncSym *cur_fn)
{
    if (!dv || dv->label != n_DeclVars) {
        return;
    }
    // Vérifie la présence du mot-clé 'static'
    Node *p = FIRSTCHILD(dv);
    bool is_static = false;
    if (p && p->label == n_STATIC && strcmp(p->text, "static") == 0) {
        is_static = true;
        p = p->nextSibling;  // Saute vers le nœud de type (n_TYPE)
    }

    // Le nœud de type est obligatoire 
    if (!p || p->label != n_TYPE) {
        return;
    }    
    TypeInfo ti = tpc_type(p);  // Convertit vers TypeInfo

    Node *decList = p->nextSibling;  // n_Declarateurs
    if (!decList) {
        return;
    }
    // Parcourt chaque identificateur déclaré
    for (Node *id = FIRSTCHILD(decList); id; id = id->nextSibling) {
        if (id->label != n_IDENT) {
            continue;
        }
        VarSym v = {0};
        strcpy(v.name, id->text);
        v.type = ti;
        v.size = (strcmp(ti.type_name, "char") == 0) ? 1 : 4;  // Alignement
        v.lineno = id->lineno;

        // Cas des variables locales static
        if (cur_fn && is_static) {
            v.is_static = true;

            // Vérifie redondance avec paramètres ou locaux existants
            if (vl_find(&cur_fn->params, v.name) != -1 || vl_find(&cur_fn->locals, v.name) != -1) {
                ERR(st, "static symbol '%s' already declared at line %d", v.name, v.lineno);
                continue;
            }

            // Génère un label unique pour le fichier assembleur
            snprintf(v.asm_label, sizeof(v.asm_label), "%s.Lf_%s", cur_fn->name, v.name);

            // Insère dans la liste des locaux
            vl_add(&cur_fn->locals, v);
        }

        // Cas des variables locales normales
        else if (cur_fn && !is_static) {
            // Vérifie redondance
            if (vl_find(&cur_fn->params, v.name) != -1 || vl_find(&cur_fn->locals, v.name) != -1) {
                ERR(st, "symbol '%s' already declared at line %d", v.name, v.lineno);
                continue;
            }

            // Calcule le décalage mémoire dans la pile
            cur_fn->locals.cur_offset += v.size;
            int local_offset = -8 * (cur_fn->params.len + cur_fn->locals.len + 1);
            v.offset = local_offset;

            vl_add(&cur_fn->locals, v);
        }
    }
}





// Parcours récursif de l’AST et construction de la table des symboles
static void walk(Node *n, Symtab *st, FuncSym *cur)
{
    if (!n) { 
        return;
    }
    switch (n->label) {

    // Déclaration de variables globales 
    case n_GlobalVarDecl: {
        Node *typeNode = FIRSTCHILD(n);          // n_TYPE
        TypeInfo ti    = tpc_type(typeNode);  

        Node *decList = SECONDCHILD(n);          // n_Declarateurs
        for (Node *id = FIRSTCHILD(decList); id; id = id->nextSibling) {
            if (id->label != n_IDENT) {
                continue; 
            }
            VarSym v = {0};
            strcpy(v.name, id->text);          
            v.type  = ti;                      
            v.size  = (strcmp(ti.type_name, "char") == 0) ? 1 : 4;                    
            v.offset   = 0;                    
            v.lineno   = id->lineno;        
            st_add_global(st, v);                // insère dans la table globale
        }
        break;
    }

    //  Définition de fonction : création d’une entrée 
    case n_DeclFonct: {
        Node *header = FIRSTCHILD(n);            // n_EnTeteFonct
        Node *retTy  = FIRSTCHILD(header);       // type de retour
        Node *fName  = SECONDCHILD(header);      // identificateur
        Node *params = THIRDCHILD(header);       // paramètres

        FuncSym f = {0};
        strcpy(f.name, fName->text);
        f.ret_type = tpc_type(retTy);
        f.lineno   = fName->lineno;
        if (!st_add_func(st, f)) {
            break;        
        }
        cur = st_lookup_func(st, f.name);        // pointeur sur l’entrée
        //  Paramètres
        if (FIRSTCHILD(params)->label != n_void) {
            int param_index = 0;
            insert_param_list(FIRSTCHILD(params), cur, st, &param_index);
        }

        // Nombre de bytes occupés par les variables locales déjà comptés
        cur->locals_bytes = cur->locals.cur_offset;
        break;
    }

    // Déclarations Variables
    case n_DeclVars: {
        insert_declvars(n, st, cur);
        break;
    }

    default:
        break;
    }

    // Appel récursif : d’abord les enfants, puis les frères
    walk(n->firstChild,  st, cur);
    walk(n->nextSibling, st, cur);
}

// Déduit le type de l'expression donnée, en réalisant des vérifications sémantiques.
// Retourne un TypeInfo correspondant à "int", "char", "void", ou "func".
// Lève des erreurs si des opérations sont effectuées sur des types invalides (void, etc).
TypeInfo infer_expr_type(Node *e, Symtab *st, FuncSym *cur_fn)
{
    if (!e) return (TypeInfo){ "void" };  // expression vide → type void par défaut

    switch (e->label) {

    // Appel de fonction encapsulé : n_Exp → n_IDENT
    case n_Exp: {
        Node *id = e->firstChild;
        if (id && id->label == n_IDENT) {
            FuncSym *f = st_lookup_func(st, id->text);
            // Fonctions intégrées
            if (!f && is_builtin(id->text)) {
                if (!strcmp(id->text, "getchar") || !strcmp(id->text, "getint"))
                    return (TypeInfo){"int"};
                if (!strcmp(id->text, "putchar") || !strcmp(id->text, "putint"))
                    return (TypeInfo){"void"};
            }
            // Si non déclarée, on suppose int par défaut
            return f ? f->ret_type : (TypeInfo){"int"};
        }
        return (TypeInfo){"int"};  // sécurité
    }

    // Constantes : littéraux numériques ou caractères
    case n_NUMBER:  return (TypeInfo){ "int" };
    case n_CHAR:    return (TypeInfo){ "char" };

    // Identifiant pouvant être une variable, une fonction ou un appel
    case n_IDENT: {
        // Appel de fonction explicite
        if (e->firstChild && e->firstChild->label == n_Arguments) {
            FuncSym *f = st_lookup_func(st, e->text);
            if (f) return f->ret_type;

            if (is_builtin(e->text)) {
                if (!strcmp(e->text, "getint") || !strcmp(e->text, "getchar"))
                    return (TypeInfo){"int"};
                if (!strcmp(e->text, "putint") || !strcmp(e->text, "putchar"))
                    return (TypeInfo){"void"};
            }

            return (TypeInfo){"int"};  // Par défaut si inconnue
        }

        // Variable locale/globale
        VarSym *v = st_lookup_var(st, cur_fn, e->text);
        if (v) {return v->type;}

        // Nom de fonction sans appel → type spécial 'func'
        // if (st_lookup_func(st, e->text))
        //     return (TypeInfo){ "func" };

        return (TypeInfo){ "int" };  // Défaut
    }

    // Opérations + et - (unaires ou binaires)
    case n_ADDSUB: {
        Node *lhs = FIRSTCHILD(e);
        Node *rhs = lhs ? lhs->nextSibling : NULL;

        TypeInfo tL = infer_expr_type(lhs, st, cur_fn);
        if (!strcmp(tL.type_name, "void")) {
            ERR(st, "void expression cannot be used in this context (line %d)", e->lineno);
        }

        // Opérateur unaire : +x / -x
        if (!rhs) {
            return tL;
        }

        // Opérateur binaire : x + y
        TypeInfo tR = infer_expr_type(rhs, st, cur_fn);
        if (!strcmp(tR.type_name, "void")) {
            ERR(st, "void expression cannot be used in this context (line %d)", e->lineno);
        }
        return (TypeInfo){"int"};
    }

    // *, /, %
    case n_DIVSTAR: {
        Node *lhs = FIRSTCHILD(e);
        Node *rhs = lhs ? lhs->nextSibling : NULL;

        TypeInfo tL = infer_expr_type(lhs, st, cur_fn);
        if (!strcmp(tL.type_name, "void"))
            ERR(st, "void expression cannot be used in this context (line %d)", e->lineno);
        TypeInfo tR = infer_expr_type(rhs, st, cur_fn);
        if (!strcmp(tR.type_name, "void"))
            ERR(st, "void expression cannot be used in this context (line %d)", e->lineno);

        // Détection statique : division par 0
        if (rhs->label == n_NUMBER && strcmp(rhs->text, "0") == 0) {
            ERR(st, "division or modulo by zero at line %d", e->lineno);
        }

        return (TypeInfo){"int"};
    }

    // Comparateurs <, <=, >, >=
    case n_ORDER: {
        Node *lhs = FIRSTCHILD(e);
        Node *rhs = lhs ? lhs->nextSibling : NULL;

        TypeInfo tL = infer_expr_type(lhs, st, cur_fn);
        if (!strcmp(tL.type_name, "void"))
            ERR(st, "void expression cannot be used in this context (line %d)", e->lineno);
        TypeInfo tR = infer_expr_type(rhs, st, cur_fn);
        if (!strcmp(tR.type_name, "void"))
            ERR(st, "void expression cannot be used in this context (line %d)", e->lineno);

        return (TypeInfo){"int"};
    }

    // Opérateurs logiques && et ||
    case n_AND:
    case n_OR: {
        Node *lhs = FIRSTCHILD(e);
        Node *rhs = lhs ? lhs->nextSibling : NULL;

        TypeInfo tL = infer_expr_type(lhs, st, cur_fn);
        if (!strcmp(tL.type_name, "void"))
            ERR(st, "void expression cannot be used in this context (line %d)", e->lineno);
        TypeInfo tR = infer_expr_type(rhs, st, cur_fn);
        if (!strcmp(tR.type_name, "void"))
            ERR(st, "void expression cannot be used in this context (line %d)", e->lineno);
        return (TypeInfo){"int"};
    }

    // Opérateur unaire logique : !
    case n_NOT: {
        Node *sub = FIRSTCHILD(e);
        TypeInfo t = infer_expr_type(sub, st, cur_fn);
        if (!strcmp(t.type_name, "void")) {
            ERR(st, "void expression cannot be used in logical NOT (line %d)", e->lineno);
        } 
        return (TypeInfo){"int"};
    }

    // Autres cas (par défaut) : supposé int
    default:
        return (TypeInfo){"int"};
    }
}

// Vérifie sémantiquement l’appel à une fonction intégrée (builtin).
// Cette fonction est appelée pendant l’analyse sémantique pour détecter les erreurs d’arguments.
// Elle s’applique aux fonctions : getint, getchar, putint, putchar.
static void check_builtin_call(Node *callNode, Symtab *st, int lineno, FuncSym *cur_fn)
{
    const char *fname = callNode->text;               // Nom de la fonction appelée
    Node *argsNode = callNode->firstChild;            // n_Arguments
    Node *list = argsNode ? FIRSTCHILD(argsNode) : NULL;  // n_ListExp

    // Calcule le nombre d’arguments fournis
    int argc = 0;
    for (Node *e = list ? FIRSTCHILD(list) : NULL; e; e = e->nextSibling)
        ++argc;

    // getint / getchar → ne prennent aucun argument 
    if (!strcmp(fname, "getint") || !strcmp(fname, "getchar")) {
        if (argc != 0) {
            ERR(st, "builtin function '%s' takes no arguments (line %d)", fname, lineno);
        }
    }

    // putint / putchar → doivent recevoir exactement 1 argument
    else if (!strcmp(fname, "putint") || !strcmp(fname, "putchar")) {
        if (argc != 1) {
            ERR(st, "builtin function '%s' takes exactly 1 argument (line %d)", fname, lineno);
        } else {
            // Vérifie le type de l’argument
            TypeInfo arg_type = infer_expr_type(FIRSTCHILD(list), st, cur_fn);

            // Interdit les expressions de type void
            if (!strcmp(arg_type.type_name, "void")) {
                ERR(st, "argument to '%s' cannot be void (line %d)", fname, lineno);
            }

            // putchar : avertissement si l’argument n’est pas de type char
            if (!strcmp(fname, "putchar") && strcmp(arg_type.type_name, "char") != 0) {
                WARN(st, "argument to 'putchar' should be char (line %d)", lineno);
            }
        }
    }
}



void check_usage(Node *n, Symtab *st,
    FuncSym *cur_fn, bool in_decl)
{
    if (!n) { return;}
    bool child_decl = in_decl; 
    // Les nœuds de déclaration rendent in_decl vrai 
    switch (n->label) {
        case n_GlobalVarDecl:
        case n_DeclVars:
        case n_ListTypVar:
        case n_EnTeteFonct:
        case n_Parametres:
            child_decl = true;          // On entre dans une zone de déclaration
            break;

        case n_DeclFonct: {             // Changement de portée fonctionnelle
            Node *fname = SECONDCHILD(FIRSTCHILD(n));   // n_IDENT
            cur_fn = st_lookup_func(st, fname->text);
            if(!cur_fn){
                return;
            }
            break;
        }

        case n_Exp: {
            Node *call = FIRSTCHILD(n);             // Peut être un n_IDENT
            if (call && call->label == n_IDENT) {   
                FuncSym *f = st_lookup_func(st, call->text); // Vérifier appel de fonction
                
                // Fonction non déclarée
                if (!f && !is_builtin(call->text)) {
                    ERR(st, "call to undeclared function '%s' (line %d)",
                        call->text, n->lineno);
                }

                // Compter le nombre d’arguments
                int argc = 0;
                Node *argsNode = call->firstChild;
                Node *list = argsNode ? FIRSTCHILD(argsNode) : NULL;
            
                if (list && list->label == n_ListExp) {
                    for (Node *e = FIRSTCHILD(list); e; e = e->nextSibling)
                        ++argc;
                }

                if (f && argc != f->params.len) {
                    ERR(st, "incorrect function '%s' call: expect %d argument%s but got %d (line %d)",
                        f->name, f->params.len,
                        f->params.len == 1 ? "" : "s",
                        argc, n->lineno);
                }

                if (f && argc == f->params.len) {
                    Node *arg_node = list ? FIRSTCHILD(list) : NULL;
                    for (int i = 0; arg_node && i < f->params.len; ++i, arg_node = arg_node->nextSibling) {
                        TypeInfo arg_type = infer_expr_type(arg_node, st, cur_fn);

                        const char *param_type = f->params.data[i].type.type_name;

                        // int → char : OK (promotion), char → int : avertissement, sinon erreur
                        if (!strcmp(param_type, "char") && !strcmp(arg_type.type_name, "int")) {
                            WARN(st, "argument %d to '%s' loses precision: int to char (line %d)",
                                 i + 1, f->name, arg_node->lineno);
                        }
                        else if (strcmp(param_type, arg_type.type_name) != 0 &&
                                   !(strcmp(param_type, "int") == 0 && strcmp(arg_type.type_name, "char") == 0)) {
                            ERR(st, "type mismatch for argument %d in call to '%s': expect %s but got %s (line %d)",
                                i + 1, f->name, param_type, arg_type.type_name, arg_node->lineno);
                        }
                    }
                }

                if (f) f->used = true;
            }

            Node *argsNode = call->firstChild;      // Peut être NULL
            if (argsNode) {
                Node *list = FIRSTCHILD(argsNode);  // n_ListExp
                for (Node *e = FIRSTCHILD(list); e; e = e->nextSibling)
                    check_usage(e, st, cur_fn, false);
            }
            check_usage(n->nextSibling, st, cur_fn, in_decl);
            return;   // Fin de ce branchement, ne pas continuer vers n_IDENT
        }

        // Identifiants vraiment "utilisés"
        case n_IDENT: {
            if (in_decl) break;          // Ignorer les identifiants dans les déclarations

            // Appel de fonction 
            if (n->firstChild && n->firstChild->label == n_Arguments) {
                FuncSym *f = st_lookup_func(st, n->text);
                if (f){
                    f->used = true;
                }
                break;
            }

            // Recherche de variable
            if (st_lookup_var(st, cur_fn, n->text)) {
                st_lookup_var(st, cur_fn, n->text)->used = true;
                break;
            }

            // Non déclaré
            ERR(st, "undeclared symbol '%s' (line %d)", n->text, n->lineno);
            break;
        }

        // Instruction : peut être une affectation ou un appel 
        case n_Instr: {
            Node *lhs = FIRSTCHILD(n);
            if (!lhs) break;                         // Sécurité pour instruction vide

            // Vérifier appel de fonction (sans affectation)
            if (lhs->label == n_IDENT && lhs->firstChild && lhs->firstChild->label == n_Arguments) {
                const char *fname = lhs->text;
                if (is_builtin(fname)) {
                    check_builtin_call(lhs, st, lhs->lineno, cur_fn);
                }

                VarSym *v = st_lookup_var(st, cur_fn, fname);
                if (v) {
                    ERR(st, "entry '%s' of type %s is not a function and cannot be called (line %d)",
                        fname, v->type.type_name, lhs->lineno);
                    break;
                }

                FuncSym *f = st_lookup_func(st, fname);

                // Compter le nombre d’arguments
                int argc = 0;
                Node *argsNode = lhs->firstChild;
                Node *list = argsNode ? FIRSTCHILD(argsNode) : NULL;

                for (Node *e = list ? FIRSTCHILD(list) : NULL; e; e = e->nextSibling)
                    ++argc;

                if (!f && !is_builtin(fname)) {
                    ERR(st, "call to undeclared function '%s' (line %d)", fname, lhs->lineno);
                } else if (f && argc != f->params.len) {
                    ERR(st, "incorrect function '%s' call: expect %d argument%s but got %d (line %d)",
                        fname, f->params.len, f->params.len == 1 ? "" : "s", argc, lhs->lineno);
                }

                if (f) f->used = true;

                // Vérification récursive des expressions d’arguments
                check_usage(lhs->firstChild, st, cur_fn, in_decl);
            }

            Node *eq_token = lhs->nextSibling;     
            if (!(eq_token && eq_token->label == n_ASSIGN))
                break;

            Node *rhs = eq_token->nextSibling;       // Assuré que c’est une affectation
            if (rhs && rhs->label == n_Exp) {
                Node *callee = FIRSTCHILD(rhs);  
                if (callee && callee->label == n_IDENT && callee->firstChild && callee->firstChild->label == n_Arguments) {
                    if (is_builtin(callee->text)) {
                        check_builtin_call(callee, st, callee->lineno, cur_fn);
                    }
                }
            }

            // Vérification de type 
            TypeInfo tL = infer_expr_type(lhs,  st, cur_fn);
            TypeInfo tR = infer_expr_type(rhs,  st, cur_fn);

            if (!strcmp(tL.type_name,"char") && !strcmp(tR.type_name,"int")) {
                WARN(st, "assignment from int to char may lose data (line %d)",
                    n->lineno);
            }
            else if (!strcmp(tR.type_name, "void")) {
                ERR(st, "cannot assign void expression to variable (line %d)", n->lineno);
            }
            break;
        }

        case n_IF:
        case n_WHILE: {
            Node *cond = FIRSTCHILD(n);  // Expression conditionnelle
            if (cond) {
                TypeInfo t = infer_expr_type(cond, st, cur_fn);
                if (!strcmp(t.type_name, "void")) {
                    ERR(st, "condition in %s statement cannot be void (line %d)",
                        n->label == n_IF ? "if" : "while", n->lineno);
                }
            }
            break;
        }

        case n_RETURN: {
            if (!cur_fn) break;
            cur_fn->has_return = true;
            Node *rexpr = FIRSTCHILD(n);         
            const char *rt = cur_fn->ret_type.type_name;

            // vérifier si le return appelle une fonction interne
            if (rexpr && rexpr->label == n_Exp) {
                Node *callee = FIRSTCHILD(rexpr);
                if (callee && callee->label == n_IDENT && callee->firstChild && callee->firstChild->label == n_Arguments) {
                    if (is_builtin(callee->text)) {
                        check_builtin_call(callee, st, callee->lineno, cur_fn);
                    }
                }
            }

            if (!strcmp(rt, "void")) {
                if (!rexpr) break;  // return void correct
                ERR(st, "void function '%s' cannot return a value (line %d)",
                    cur_fn->name, n->lineno);
                break;
            }


            TypeInfo et = infer_expr_type(rexpr, st, cur_fn);

            if (!strcmp(et.type_name, "void")) {
                ERR(st, "returning void expression from non-void function '%s' (line %d)",
                     cur_fn->name, n->lineno);
                break;
            }

            if (!strcmp(rt, "char")) {
                if (strcmp(et.type_name, "char")) {
                    WARN(st, "return type mismatch in %s(): expect char but got %s (line %d)",
                         cur_fn->name, et.type_name, n->lineno);
                }
                break;
            }

            break;
        }

        default:
            break;
    }

    //  Parcours récursif en profondeur
    check_usage(n->firstChild,  st, cur_fn, child_decl);
    check_usage(n->nextSibling, st, cur_fn, in_decl);
}


void report_unused(Symtab *st)
{
    // Variables globales
    for (int i = 0; i < st->globals.len; ++i) {
        VarSym *v = &st->globals.data[i];
        if (!v->used)
            WARN(st, "unused global variable '%s' at line %d", v->name,v->lineno);  // Avertir si la variable globale n’est jamais utilisée
    }

    // Parcours de chaque fonction
    for (int f = 0; f < st->func_len; ++f) {
        FuncSym *fn = &st->funcs[f];

        // Fonction jamais appelée (sauf main et fonctions internes)
        if (!is_builtin(fn->name) && strcmp(fn->name, "main") && !fn->used) {
            WARN(st, "function '%s' never called at line %d", fn->name,fn->lineno);
        }

        // Paramètres non utilisés
        for (int i = 0; i < fn->params.len; ++i) {
            VarSym *p = &fn->params.data[i];
            if (!p->used)
                WARN(st, "unused parameter '%s' in %s() at line %d", p->name, fn->name,p->lineno);
        }

        // Variables locales non utilisées
        for (int i = 0; i < fn->locals.len; ++i) {
            VarSym *l = &fn->locals.data[i];
            if (!l->used)
                WARN(st, "unused local variable '%s' in %s() at line %d ", l->name, fn->name,l->lineno);
        }

        // Fonction non-void sans return (sauf main)
        if (strcmp(fn->ret_type.type_name, "void") != 0      // Retour attendu
            && strcmp(fn->name, "main")                      // main() exempté
            && !fn->has_return) {
            ERR(st, "non-void function '%s' has invalid return statement at line %d", fn->name,fn->lineno);
        }
    }
}

void build_symbols(Symtab *st, Node *root) {
    walk(root, st, NULL);  

    // s’assurer qu’une fonction main() existe et est correcte 
    FuncSym *main_fn = st_lookup_func(st, "main");
    if (!main_fn) {
        ERR(st, "missing main function");  // Erreur : main() manquante
    } else {
        // Fvérifier que la signature est bien int main(void)
        if (strcmp(main_fn->ret_type.type_name, "int") != 0) {
            ERR(st, "main() must return int");  // Erreur : mauvais type de retour
        }
        if (!(main_fn->params.len == 0)) {
            ERR(st, "main() must take no arguments but void");  // Erreur : main() ne doit pas avoir de paramètres
        }
    }
}

