#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/stat.h>
#include "tree.h"
#include "symbol.h"

// 64-bit registers 
static const char *REG64[6] = { "rdi","rsi","rdx","rcx","r8","r9" };

// 32-bit registers 
static const char *REG32[6] = { "edi","esi","edx","ecx","r8d","r9d" };

// 8-bit registers
static const char *REG8[6]  = { "dil","sil","dl","cl","r8b","r9b" };

// Indique si une instruction return a été rencontrée dans la fonction courante
static int hasReturn = 0;

// Nom du fichier de sortie pour le code NASM généré
static const char *out_file_name = "_anonymous.asm";

// Compteur global pour générer des étiquettes uniques (labels)
static int lblcnt = 0;

// Pointeur vers la table des symboles globale (conservée pendant la génération)
static Symtab *gSt = NULL;

// Fonction en cours de génération (portée locale)
static FuncSym *cur = NULL;

// Fichier de sortie courant pour le code assembleur
static FILE *fout = NULL;

// Génère un nouveau label numérique unique (ex: L0, L1, …)
#define NEWLBL() (lblcnt++)

// Empile la valeur du registre rax sur la pile
#define PUSH_RAX fprintf(fout,"    push rax\n")

// Dépile la valeur au sommet de la pile dans le registre spécifié
#define POP_TO(reg) fprintf(fout,"    pop %s\n",reg)


static void gen_call(Node *ident);
static void gen_bin(Node *e, const char *op);
static int char_literal_value(const char *s);

// Définit le nom du fichier de sortie pour le code NASM généré
// - filename : nom du fichier (ex: "mon_programme.asm")
void set_output_filename(const char *filename) {
   out_file_name = filename;
}

// Ouvre le fichier assembleur en écriture et affecte le pointeur à *out
// - out : pointeur vers un FILE* à initialiser
// Si l'ouverture échoue, un message d'erreur est affiché et le programme quitte
void openAsmFile(FILE **out) {
    if (!strcmp(out_file_name,"_anonymous.asm")){
        *out = fopen("_anonymous.asm", "w");
        return;
    }
    const char *filename = strrchr(out_file_name, '/');
    filename = (filename != NULL) ? filename + 1 : out_file_name;
    char basename[128];
    strncpy(basename, filename, sizeof(basename));
    char *dot = strrchr(basename, '.');
    if (dot) *dot = '\0'; 
    mkdir("nasm", 0777);
    char output_path[256];
    snprintf(output_path, sizeof(output_path), "nasm/%s.asm", basename);
    *out = fopen(output_path, "w");
    if (!*out) {
        fprintf(stderr, "Error: cannot open %s for writing.\n", output_path);
        exit(3);
    }
}

// Ferme le fichier assembleur si ouvert, puis remet le pointeur à NULL
// - out : pointeur vers un FILE* ouvert en écriture
void closeAsmFile(FILE **out) { 
    if (*out) {
        fclose(*out);   
        *out = NULL;   
    }
}


// Charge la valeur d'une variable dans le registre rax
// - v   : symbole de la variable (locale, paramètre, globale ou static)
// - dst : registre cible (toujours "rax")
static void load_var(const VarSym *v, const char *dst /* 总是 "rax" */)
{
    // Choisir l'instruction de chargement appropriée
    if (v->is_static) {                     // Variable locale static → traitée comme globale
        if (v->size == 1)
            // char → extension de signe depuis 1 octet
            fprintf(fout, "    movsx rax, byte  [%s]\n", v->asm_label);
        else                                // int → extension de signe depuis 4 octets
            fprintf(fout, "    movsxd rax, dword [%s]\n", v->asm_label);
    }
    else if (v->offset < 0) {               // Variable avec offset < 0
        if (v->size == 1)
            fprintf(fout, "    movsx rax, byte  [rbp-%d]\n", -v->offset);
        else
            fprintf(fout, "    movsxd rax, dword [rbp-%d]\n", -v->offset);
    }
    else if (v->offset > 0) {               // Variable avec offset > 0
        if (v->size == 1)
            fprintf(fout, "    movsx rax, byte  [rbp+%d]\n",  v->offset);
        else
            fprintf(fout, "    movsxd rax, dword [rbp+%d]\n",  v->offset);
    }
    else {                                  // Variable globale 
        if (v->size == 1)
            fprintf(fout, "    movsx rax, byte  [%s]\n", v->name);
        else
            fprintf(fout, "    movsxd rax, dword [%s]\n", v->name);
    }
}



// Génère du code assembleur pour une expression donnée
static void gen_expr(Node *e){
    if(!e) return;
    switch(e->label){
    case n_NUMBER:
        // Constante entière → charger directement dans rax
        fprintf(fout,"    mov rax, %s\n",e->text);
        return;

    case n_CHAR: {
        // Constante caractère → convertir en entier signé → mov rax, val
        int val = char_literal_value(e->text);
        fprintf(fout,"    mov rax, %d\n", val);
        return;
    }

    case n_IDENT:
        if(e->firstChild && e->firstChild->label==n_Arguments){
            // Appel de fonction → gérer via gen_call()
            gen_call(e);
            return;
        }
        else {
            // Variable → récupérer via la table des symboles → charger dans rax
            VarSym *v = st_lookup_var(gSt,cur,e->text);
            if(!v){
                fprintf(stderr,"unknown ident %s\n",e->text);
                exit(3);
            }
            load_var(v,"rax");
            return;
        }

    case n_Exp:
        // Expression enveloppante → traiter l'enfant directement
        gen_expr(e->firstChild);
        return;

    case n_ADDSUB: {
        Node *lhs = FIRSTCHILD(e);
        Node *rhs = lhs ? lhs->nextSibling : NULL;

        if (!rhs) {
            // Cas d’un opérateur unaire + / -
            gen_expr(lhs); // rax ← valeur
            if (strcmp(e->text, "+") == 0)
                ; // +unaire : ne rien faire
            else
                fprintf(fout, "    neg rax\n"); // -unaire : négation
            return;
        }

        // Cas d’un opérateur binaire + / -
        const char *op = (strcmp(e->text, "+") == 0) ? "add" : "sub";
        gen_bin(e, op);
        return;
    }

    case n_DIVSTAR: {
        if (!strcmp(e->text, "%")) {
            // Opération de modulo (%)
            Node *lhs = FIRSTCHILD(e);
            Node *rhs = lhs ? lhs->nextSibling : NULL;

            gen_expr(rhs); PUSH_RAX;
            gen_expr(lhs); POP_TO("rcx");

            fprintf(fout,
                "    cqo\n"
                "    idiv rcx\n"       // rax = quotient, rdx = reste
                "    mov rax, rdx\n"); // on récupère le reste
            return;
        }

        // * ou /
        gen_bin(e, !strcmp(e->text, "*") ? "imul" : "idiv");
        return;
    }

    case n_EQ: {
        // Comparaison == ou !=
        Node *lhs = FIRSTCHILD(e);
        Node *rhs = lhs ? lhs->nextSibling : NULL;

        gen_expr(rhs);  PUSH_RAX;
        gen_expr(lhs);  POP_TO("rcx");

        fprintf(fout, "    cmp rax, rcx\n");

        int L_true  = NEWLBL();
        int L_false = NEWLBL();
        int L_end   = NEWLBL();

        if (strcmp(e->text, "==") == 0){
            fprintf(fout, "    je  L%d\n    jmp L%d\n", L_true, L_false);
        }
        else{
            fprintf(fout, "    jne L%d\n    jmp L%d\n", L_true, L_false);
        }
        // Résultat vrai
        fprintf(fout, "L%d:\n    mov rax, 1\n    jmp L%d\n", L_true, L_end);
        // Résultat faux
        fprintf(fout, "L%d:\n    mov rax, 0\n", L_false);
        // Suite
        fprintf(fout, "L%d:\n", L_end);
        return;
    }

    case n_ORDER: {
        // Comparateurs <, >, <=, >=
        Node *lhs = FIRSTCHILD(e);
        Node *rhs = lhs ? lhs->nextSibling : NULL;

        gen_expr(rhs);  PUSH_RAX;
        gen_expr(lhs);  POP_TO("rcx");

        fprintf(fout, "    cmp rax, rcx\n");

        int L_true  = NEWLBL();
        int L_false = NEWLBL();
        int L_end   = NEWLBL();

        const char *op = e->text;
        if(!strcmp(op, "<" )) {
            fprintf(fout,"    jl  L%d\n", L_true);
        }
        else if (!strcmp(op, ">" )) {
            fprintf(fout,"    jg  L%d\n", L_true);
        }
        else if (!strcmp(op, "<=")) {
            fprintf(fout,"    jle L%d\n", L_true);
        }
        else {                        
            fprintf(fout,"    jge L%d\n", L_true);
        }
        fprintf(fout,"    jmp L%d\n", L_false);

        fprintf(fout,"L%d:\n    mov rax, 1\n    jmp L%d\n", L_true, L_end);
        fprintf(fout,"L%d:\n    mov rax, 0\n", L_false);
        fprintf(fout,"L%d:\n", L_end);
        return;
    }
    
    case n_NOT: {
        // Opérateur logique unaire "non" : !expr
        int L_true = NEWLBL();
        int L_end  = NEWLBL();

        // Évaluer l’expression 
        gen_expr(FIRSTCHILD(e));

        // Comparer avec 0 → branchement conditionnel
        fprintf(fout,
            "    cmp rax, 0\n"
            "    je  L%d\n", L_true);

        // Faux → !expr = 0
        fprintf(fout,
            "    mov rax, 0\n"
            "    jmp L%d\n", L_end);

        // Vrai → !expr = 1
        fprintf(fout,
            "L%d:\n"
            "    mov rax, 1\n", L_true);

        // Suite
        fprintf(fout, "L%d:\n", L_end);
        return;
    }

    case n_OR: {
        // Opérateur logique OU ||
        int L_true  = NEWLBL();
        int L_false = NEWLBL();
        int L_end   = NEWLBL();

        // Évaluer le premier opérande
        gen_expr(FIRSTCHILD(e));
        fprintf(fout,
            "    cmp rax, 0\n"
            "    jne L%d            ; lhs non nul → vrai\n", L_true);

        // Évaluer le second opérande
        gen_expr(FIRSTCHILD(e)->nextSibling);
        fprintf(fout,
            "    cmp rax, 0\n"
            "    jne L%d            ; rhs non nul → vrai\n", L_true);

        // Les deux sont faux
        fprintf(fout,"    jmp L%d\n", L_false);

        // Branches vrai / faux
        fprintf(fout,"L%d:\n    mov rax, 1\n    jmp L%d\n", L_true, L_end);
        fprintf(fout,"L%d:\n    mov rax, 0\n", L_false);
        fprintf(fout,"L%d:\n", L_end);
        return;
    }

    case n_AND: {
        // Opérateur logique ET &&
        int L_true  = NEWLBL();
        int L_false = NEWLBL();
        int L_end   = NEWLBL();

        // Évaluer le premier opérande
        gen_expr(FIRSTCHILD(e));
        fprintf(fout,
            "    cmp rax, 0\n"
            "    je L%d\n", L_false);

        // Évaluer le second opérande
        gen_expr(FIRSTCHILD(e)->nextSibling);
        fprintf(fout,
            "    cmp rax, 0\n"
            "    je L%d\n", L_false);

        // Les deux sont vrais
        fprintf(fout,"    jmp L%d\n", L_true);

        // Branches vrai / faux
        fprintf(fout,"L%d:\n    mov rax, 1\n    jmp L%d\n", L_true, L_end);
        fprintf(fout,"L%d:\n    mov rax, 0\n", L_false);
        fprintf(fout,"L%d:\n", L_end);
        return;
    }

    default:
        fprintf(stderr,"expr label %d unhandled\n",e->label);
        exit(3);
    }
}


// Génère le code assembleur pour une opération binaire (+, -, *, /, %)
// - e  : nœud d'expression binaire
// - op : nom de l'opération ("add", "sub", "imul", "idiv", "mod")
static void gen_bin(Node *e, const char *op)
{
    Node *lhs = FIRSTCHILD(e);
    Node *rhs = lhs ? lhs->nextSibling : NULL;

    // Évaluer l'opérande gauche en premier → sauvegarder sur la pile
    gen_expr(lhs);        // rax = lhs
    PUSH_RAX;             // sauvegarder lhs

    // Évaluer l'opérande droite ensuite
    gen_expr(rhs);        // rax = rhs
    POP_TO("rcx");        // rcx = lhs, rax = rhs

    if (!strcmp(op, "add")) {
        // Addition entière : rcx = lhs + rhs → résultat dans rax
        fprintf(fout,
            "    add rcx, rax\n"
            "    mov rax, rcx\n");
    }
    else if (!strcmp(op, "sub")) {
        // Soustraction entière : rcx = lhs - rhs → résultat dans rax
        fprintf(fout,
            "    sub rcx, rax\n"
            "    mov rax, rcx\n");
    }
    else if (!strcmp(op, "imul")) {
        // Multiplication entière signée : rax = lhs * rhs
        fprintf(fout,
            "    imul rax, rcx\n"); // ordre inversé autorisé car commutatif
    }
    else if (!strcmp(op, "idiv") || !strcmp(op, "mod")) {

        //  Division ou modulo entier 
        // rax = rhs (diviseur), rcx = lhs (dividende)

        fprintf(fout,
            "    push rax\n"            // sauvegarder le diviseur (rhs)
            "    mov  rax, rcx\n"       // rax = dividende (lhs)
            "    cqo\n"                 // étendre rax en rdx:rax (sign-extension)
            "    pop  rcx\n"            // rcx = diviseur (rhs)
            "    cmp  rcx, 0\n"
            "    je   .div_by_zero_%d\n"
            "    idiv rcx\n",           lblcnt);

        if (!strcmp(op, "mod"))
            // Pour modulo, on récupère le reste (rdx)
            fprintf(fout, "    mov rax, rdx\n");

        fprintf(fout,
            "    jmp  .div_end_%d\n"
            ".div_by_zero_%d:\n"
            "    mov  rax, 0\n"             // par sécurité : retourner 0 si division par zéro
            ".div_end_%d:\n",
            lblcnt, lblcnt, lblcnt);

        ++lblcnt;
    }
    else {
        // Opérateur non reconnu
        fprintf(stderr, "gen_bin: unknown op %s\n", op);
        exit(3);
    }
}



// Génère le code pour un appel de fonction (avec passage de paramètres)
static void gen_call(Node *ident)
{
    //  Récupérer les arguments (de gauche à droite) 
    Node *argsNode = FIRSTCHILD(ident);          // n_Arguments
    Node *list = argsNode ? FIRSTCHILD(argsNode) : NULL;  // n_ListExp
    Node *arg  = list ? FIRSTCHILD(list) : NULL;

    // Stocker les pointeurs et compter le nombre d'arguments
    Node *argv[64];      // Assez grand pour contenir tous les arguments
    int   argc = 0;
    for (; arg; arg = arg->nextSibling)
        argv[argc++] = arg;

    //Gérer les registres pour les 6 premiers arguments -
    for (int i = 0; i < argc && i < 6; ++i) {
        gen_expr(argv[i]);                // rax = valeur de l'argument (64 bits)
        fprintf(fout, "    mov %s, rax\n", REG64[i]); // mov rdi, rax / rsi, rax etc.
    }

    // Empiler les arguments restants (au-delà de 6) en ordre inverse
    for (int i = argc - 1; i >= 6; --i) {
        gen_expr(argv[i]);                // rax ← valeur
        PUSH_RAX;
    }

    // Appel de la fonction 
    fprintf(fout,"    call %s\n", ident->text);

}


// Convertit une séquence de caractère (ex: "\\n", "a", "\\x41")
// en une valeur entière signée (-128 à 127)
static int char_literal_value(const char *s)
{
    int val;  // décoder vers une valeur non signée entre 0 et 255

    if (s[0] != '\\') {                 // Cas simple : caractère normal, ex: 'a'
        val = (unsigned char)s[0];
    } else {
        // Cas échappé : commence par '\'
        switch (s[1]) {
            case 'n':  val = 10;  break;   // Saut de ligne
            case 't':  val = 9;   break;   // Tabulation horizontale
            case 'r':  val = 13;  break;   // Retour chariot (CR)
            case '\\': val = 92;  break;   // Antislash '\'
            case '\'': val = 39;  break;   // Apostrophe '\''
            case '0':  val = 0;   break;   // Caractère nul '\0'
            case 'x':               // Hexadécimal, ex: \x41
                sscanf(s + 2, "%x", &val);
                break;
            default:
                // Cas d’un octal (ex: \123) ou inconnu
                if (s[1] >= '0' && s[1] <= '7')
                    sscanf(s + 1, "%o", &val);  // Lecture octale
                else
                    val = (unsigned char)s[1];  // Autre caractère après '\'
        }
    }

    // convertir 0..255 en -128..127 si nécessaire
    if (val & 0x80)  // Si bit de poids fort à 1 → valeur négative
        val -= 256;

    return val;  // Résultat final signé
}


// Vérifie si le nœud représente un appel valide à getchar()
// Renvoie 1 si c’est un appel à getchar() sans argument, sinon 0
static int is_getchar_call(Node *callIdent)
{
    // Le nœud doit exister et être un identifiant
    if (!callIdent || callIdent->label != n_IDENT) {
        return 0;
    }
    // Le nom de l’identifiant doit être exactement "getchar"
    if (strcmp(callIdent->text, "getchar") != 0) {
        return 0;
    }
    // Vérifier qu’il possède des arguments (n_Arguments)
    Node *args = FIRSTCHILD(callIdent);
    if (!args || args->label != n_Arguments) {
        return 0;
    }
    // L’argument doit être une liste d’expressions (n_ListExp)
    Node *list = FIRSTCHILD(args);
    if (!list) {
        return 1;  // Aucun argument → appel correct à getchar()
    }
    if (list->label != n_ListExp) {
        return 0;
    }

    // La liste doit être vide (aucun enfant) → getchar()
    return (FIRSTCHILD(list) == NULL);
}

// Génère le code assembleur pour les nœuds de l'AST de manière récursive
static void cg_traveser(Node *n, Node *parent) {
    if (!n) return;

    switch (n->label) {
    case n_Corps:
        // Corps de fonction : on ne traite que le premier enfant,
        // les autres instructions seront traitées par le code en bas
        cg_traveser(n->firstChild, n);
        return; // on ne descend pas plus bas ici

    case n_Instr: {
        Node *lhs = n->firstChild;
        Node *eq  = lhs ? lhs->nextSibling : NULL;
        Node *rhs = eq  ? eq->nextSibling : NULL;

        // Cas d'une affectation (lhs = rhs)
        if (eq && eq->label == n_ASSIGN) {

            // Si RHS est un appel encapsulé dans n_Exp, on le déplie
            Node *maybeCall = rhs;
            if (maybeCall && maybeCall->label == n_Exp){
                maybeCall = maybeCall->firstChild;
            }
            int needFlush = is_getchar_call(maybeCall); // Appel à getchar()

            // Recherche du symbole correspondant au LHS
            VarSym *v = st_lookup_var(gSt, cur, lhs->text);
            if (!v) {
                fprintf(stderr, "unknown LHS %s\n", lhs->text);
                exit(3);
            }
            // Générer l'expression RHS normalement      
            gen_expr(rhs); 
            // Générer le stockage vers la variable   
            if (v->is_static) {
                if (v->size == 1)
                    fprintf(fout, "    mov byte  [%s], al\n", v->asm_label);
                else
                    fprintf(fout, "    mov dword [%s], eax\n", v->asm_label);
            } else if (v->offset < 0) {
                if (v->size == 1)
                    fprintf(fout, "    mov byte  [rbp-%d], al\n", -v->offset);
                else
                    fprintf(fout, "    mov dword [rbp-%d], eax\n", -v->offset);
            } else if (v->offset > 0) { 
                if (v->size == 1)
                    fprintf(fout, "    mov byte  [rbp+%d], al\n", v->offset);
                else
                    fprintf(fout, "    mov dword [rbp+%d], eax\n", v->offset);
            } else { 
                if (v->size == 1)
                    fprintf(fout, "    mov byte  [%s], al\n", v->name);
                else
                    fprintf(fout, "    mov dword [%s], eax\n", v->name);
            }
        

            if (needFlush){
                fprintf(fout, "    call clrbuf\n"); // Nettoyer ligne de saisie
            }
        }

        // Cas d’un appel de fonction comme instruction (ex: putint(c);)
        else if (lhs && lhs->label == n_IDENT && lhs->firstChild && lhs->firstChild->label == n_Arguments) {
            gen_call(lhs);
            if (is_getchar_call(lhs)) // Nettoyer la ligne uniquement si getchar()
                fprintf(fout, "    call clrbuf\n");
        }

        break;
    }

    case n_LBRACE:
    case n_RBRACE:
        // Bloc d’instructions : traiter récursivement l’intérieur
        cg_traveser(n->firstChild, n);
        break;

    case n_IF: {
        // Extraire condition et blocs
        Node *cond = FIRSTCHILD(n);  // condition
        Node *trueBlk = NULL;        // bloc si vrai
        Node *falseBlk = NULL;       // bloc si faux

        // Bloc "if"
        Node *p = cond ? cond->nextSibling : NULL;
        if (p) {
            trueBlk = (p->label == n_LBRACE) ? FIRSTCHILD(p) : p;
            p = p->nextSibling;
        }

        // Bloc "else" s’il existe
        for (; p; p = p->nextSibling) {
            if (p->label == n_ELSE) {
                Node *q = p->firstChild;
                falseBlk = (q && q->label == n_LBRACE) ? FIRSTCHILD(q) : q;
                break;
            }
        }

        // Génération du code 
        int L_else = NEWLBL();
        int L_end  = NEWLBL();

        gen_expr(cond); 
        fprintf(fout,
            "    cmp rax, 0\n"
            "    je  L%d\n", falseBlk ? L_else : L_end); // saut si faux

        cg_traveser(trueBlk, n); // Bloc "if"
        fprintf(fout, "    jmp L%d\n", L_end);

        // Bloc "else" 
        if (falseBlk) {
            fprintf(fout, "L%d:\n", L_else);
            cg_traveser(falseBlk, n);
        }

        // Étiquette de sortie commune
        fprintf(fout, "L%d:\n", L_end);
        break;
    }

    case n_ELSE:
        // Ne pas traverser ses enfants, mais passer directement à son frère
        cg_traveser(n->nextSibling, parent);
        return;

    case n_WHILE: {
        Node *cond = FIRSTCHILD(n);
        Node *p = cond ? cond->nextSibling : NULL;
        Node *body = NULL;

        if (p) {
            body = (p->label == n_LBRACE) ? FIRSTCHILD(p) : p;
        }
        int L_test = NEWLBL(); // boucle → test
        int L_end  = NEWLBL(); // sortie boucle

        fprintf(fout, "L%d:\n", L_test);

        gen_expr(cond); 
        fprintf(fout,
            "    cmp rax, 0\n"
            "    je  L%d\n", L_end);

        cg_traveser(body, n); // Corps de boucle

        fprintf(fout,
            "    jmp L%d\n"
            "L%d:\n", L_test, L_end);

        break;
    }

    case n_RETURN: {
        hasReturn = 1;
        Node *rexpr = n->firstChild;

        if (rexpr){
            gen_expr(rexpr); 
        }
        else {
            fprintf(fout, "    xor eax, eax\n"); // return vide → rax = 0
        }

        fprintf(fout, "    jmp ret_%s\n\n", cur->name);
        break;
    }

    default:
        // Traitement par défaut : parcourir les enfants
        cg_traveser(n->firstChild, n);
        break;
    }

    // Ne pas oublier de continuer avec les frères du nœud actuel
    cg_traveser(n->nextSibling, parent);
}


// Calcule le nombre total d’octets nécessaires pour la pile d’une fonction
// Cela inclut les variables locales et les paramètres passés via la pile (offset < 0)
static int compute_frame_bytes(FuncSym *fn)
{
    int minOff = 0;  // Le plus petit offset (négatif) → indique la profondeur maximale nécessaire

    //  Examiner les paramètres formels (certains sont passés par registre mais ont quand même un offset < 0)
    for (int i = 0; i < fn->params.len; ++i) {
        VarSym *v = &fn->params.data[i];
        if (v->offset < minOff)
            minOff = v->offset;
    }

    // Examiner les variables locales (stockées avec des offsets négatifs depuis rbp)
    for (int i = 0; i < fn->locals.len; ++i) {
        VarSym *v = &fn->locals.data[i];
        if (v->offset < minOff)
            minOff = v->offset;
    }

    // On retourne la taille du cadre de pile à allouer (valeur positive)
    return -minOff;
}


// Génère le code assembleur pour une fonction donnée
// - fn   : symbole de la fonction (paramètres, variables locales...)
// - body : AST du corps de la fonction (n_Corps)
static void emit_function(FuncSym *fn, Node *body) {
    cur = fn;

    // Titre et étiquette de la fonction
    fprintf(fout, "\n; ----- function %s -----\n%s:\n", fn->name, fn->name);

    // Prologue standard : sauvegarder base pointer
    fprintf(fout, "    push rbp\n    mov rbp, rsp\n");

    // Calcul du nombre d'octets à réserver pour les variables locales et les paramètres sur la pile
    // Cela couvre tous les offsets négatifs nécessaires, arrondis implicitement à 16 octets via l’appelant
    int frameBytes = compute_frame_bytes(fn);
    if (frameBytes){
        fprintf(fout, "    sub rsp, %d\n", frameBytes); // Réserver de l’espace sur la pile
    }

    // Initialisation des paramètres passés par registre (jusqu’à 6)
    for (int i = 0; i < fn->params.len && i < 6; ++i) {
        VarSym *p = &fn->params.data[i];

        // Déplacement depuis le registre vers son emplacement dans le cadre de pile
        if (p->size == 1){
            fprintf(fout, "    mov byte [rbp%+d], %s\n", p->offset, REG8[i]);
        }
        else {
            fprintf(fout, "    mov dword [rbp%+d], %s\n", p->offset, REG32[i]);
        }
    }

    // Marqueur de présence de return ; sert pour "main" à générer un retour 0 par défaut
    hasReturn = 0;

    // Générer le corps de la fonction (instructions)
    cg_traveser(body, NULL);

    // Si c’est main() et qu’aucun return n’a été généré → retourner 0 explicitement
    if (strcmp(fn->name, "main") == 0 && !hasReturn) {
        fprintf(fout, "    mov rax, 0\n");
        fprintf(fout, "    jmp ret_main\n");
    }

    // Étiquette de retour commun pour tous les return de cette fonction
    fprintf(fout, "ret_%s:\n    mov rsp, rbp\n    pop rbp\n    ret\n", fn->name);
}

// Génère les déclarations des variables globales dans la section .bss (non initialisées)
// Chaque variable est allouée avec 1 unité : 1 octet pour char (resb), 4 octets pour int (resd)
static void emit_globals() {
    // Section NASM pour données non initialisées (valeurs implicites à 0)
    fprintf(fout, "section .bss\n");

    // Parcourir toutes les variables globales enregistrées
    for (int i = 0; i < gSt->globals.len; ++i) {
        VarSym *v = &gSt->globals.data[i];

        // Émettre l’étiquette et la directive de réservation :
        // - resb 1 pour un char (1 octet)
        // - resd 1 pour un int (4 octets)
        fprintf(fout, "%s: %s 1\n", v->name,
            (v->size == 1 ? "resb" : "resd"));
    }
}

// Génère les déclarations en assembleur pour les variables locales `static`
// Chaque variable est enregistrée dans .bss, identifiée par un label unique
static void emit_statics()
{
    // Parcourt toutes les fonctions du programme
    for (int f = 0; f < gSt->func_len; ++f) {
        FuncSym *fn = &gSt->funcs[f];

        // Parcourt les variables locales de cette fonction
        for (int i = 0; i < fn->locals.len; ++i) {
            VarSym *v = &fn->locals.data[i];

            // On ne traite que les variables déclarées `static`
            if (v->is_static) {
                const char *directive = NULL;

                // Choix de la directive NASM selon la taille du type
                if (v->size == 1)
                    directive = "resb";   // 1 octet → char
                else if (v->size == 4)
                    directive = "resd";   // 4 octets → int
                else {
                    fprintf(stderr, "Unknown static variable size: %d\n", v->size);
                    exit(3); // Cas d’erreur : type inconnu
                }

                // Génère la déclaration NASM 
                fprintf(fout, "%s: %s 1\n", v->asm_label, directive);
            }
        }
    }
}


// Génère le code assembleur NASM complet à partir de l’arbre de syntaxe (AST)
// - st    : table des symboles globale, contenant fonctions et variables
// - root  : racine de l’AST (n_Prog)
 void generate_nasm(Symtab *st, Node *root)
 {
     gSt = st;              // Sauvegarder la table des symboles globale
     openAsmFile(&fout);    // Ouvrir le fichier de sortie .asm
 
     fprintf(fout,"; ========= builtin =========\n");
     fprintf(fout,
"; ======== builtin: clrbuf ========\n"
"global clrbuf\n"
"clrbuf:\n"
"    push    rbp\n"
"    mov     rbp, rsp\n"
".read_more:\n"
"    call    getchar\n"
"    test    rax, rax\n"
"    je      .done_clrbuf\n"
"    cmp     al, 10\n"
"    je      .done_clrbuf\n"
"    cmp     al, 13\n"
"    je      .done_clrbuf\n"
"    jmp     .read_more\n"
".done_clrbuf:\n"
"    mov     rsp, rbp\n"
"    pop     rbp\n"
"    ret\n\n"
);

     fprintf(fout,
    "; ======== builtin: getchar ========\n"
    "global getchar\n"
    "getchar:\n"
    "    push    rbp\n"
    "    mov     rbp, rsp\n"
    "    sub     rsp, 1\n"
    "    mov     rax, 0\n"
    "    mov     rdi, 0\n"
    "    mov     rsi, rsp\n"
    "    mov     rdx, 1\n"
    "    syscall\n"
    "    cmp     rax, 1\n"
    "    jne     .fail_getchar\n"
    "    xor     eax, eax\n"
    "    mov     al, [rsp]\n"
    "    add     rsp, 1\n"
    "    mov     rsp, rbp\n"
    "    pop     rbp\n"
    "    ret\n"
    ".fail_getchar:\n"
    "    add     rsp, 1\n"
    "    mov     rsp, rbp\n"
    "    pop     rbp\n"
    "    mov     rax, 0\n"
    "    ret\n\n"
);
    fprintf(fout,
"; ======== builtin: getint ========\n"
"global getint\n"
"getint:\n"
"    push    rbp\n"
"    mov     rbp, rsp\n"
"    push    r12\n"
"    push    r13\n"
"    xor     r12, r12\n"
"    xor     r13, r13\n"
"read_first:\n"
"    call    getchar\n"
"    cmp     rax, 0\n"
"    je      io_fail\n"
"    cmp     al, '-'\n"
"    je      minus_sign\n"
"    cmp     al, '+'\n"
"    je      plus_sign\n"
"    jmp     check_digit_first\n"
"minus_sign:\n"
"    mov     r13, 1\n"
"    jmp     need_digit\n"
"plus_sign:\n"
"    jmp     need_digit\n"
"bad_first:\n"
"    jmp     io_fail\n"
"io_fail:\n"
"    mov     rax, 60\n"
"    mov     rdi, 5\n"
"    syscall\n"
"need_digit:\n"
"    call    getchar\n"
"    cmp     rax, 0\n"
"    je      io_fail\n"
"check_digit_first:\n"
"    cmp     al, '0'\n"
"    jl      bad_first\n"
"    cmp     al, '9'\n"
"    jg      bad_first\n"
"    jmp     convert_digit\n"
"loop_read_digit:\n"
"    call    getchar\n"
"    cmp     rax, 0\n"
"    je      io_fail\n"
"    cmp     al, '0'\n"
"    jl      maybe_eol\n"
"    cmp     al, '9'\n"
"    jg      maybe_eol\n"
"convert_digit:\n"
"    imul    r12, 10\n"
"    sub     rax, '0'\n"
"    add     r12, rax\n"
"    jmp     loop_read_digit\n"
"maybe_eol:\n"
"    cmp     al,10\n"
"    jne     io_fail\n"
"    jmp     finish\n"
"finish:\n"
"    mov     rax, r12\n"
"    cmp     r13, 0\n"
"    je      store_result\n"
"    neg     rax\n"
"store_result:\n"
"    pop     r13\n"
"    pop     r12\n"
"    mov     rsp, rbp\n"
"    pop     rbp\n"
"    ret\n\n"
);
    fprintf(fout,
"; ======== builtin: putchar ========\n"
"global putchar\n"
"putchar:\n"
"    push    rbp\n"
"    mov     rbp, rsp\n"
"    sub     rsp, 8\n"
"    mov     byte [rsp], dil\n"
"    mov     rax, 1\n"
"    mov     rdi, 1\n"
"    mov     rsi, rsp\n"
"    mov     rdx, 1\n"
"    syscall\n"
"    add     rsp, 8\n"
"    pop     rbp\n"
"    ret\n\n"
);

fprintf(fout,
"; ======== builtin: putint ========\n"
"global putint\n"
"putint:\n"
"    push    rbp\n"
"    mov     rbp, rsp\n"
"    push    rbx\n"
"    push    r12\n"
"    push    r13\n"
"    xor     r13, r13\n"
"    mov     r13, rdi\n"
"    xor     r12, r12\n"
"    cmp     r13, 0\n"
"    jne     check_sign\n"
"    mov     rdi, '0'\n"
"    call    putchar\n"
"    jmp     done\n"
"check_sign:\n"
"    jge     convert_digits\n"
"    mov     rdi, '-'\n"
"    call    putchar\n"
"    neg     r13\n"
"    mov     rax, r13\n"
"convert_digits:\n"
"    mov     rbx, 10\n"
"conv_loop:\n"
"    xor     rdx, rdx\n"
"    idiv    rbx\n"
"    push    rdx\n"
"    inc     r12\n"
"    cmp     rax, 0\n"
"    jne     conv_loop\n"
"print_loop:\n"
"    pop     rax\n"
"    add     al, '0'\n"
"    mov     rdi, rax\n"
"    call    putchar\n"
"    dec     r12\n"
"    jnz     print_loop\n"
"done:\n"
"    pop     r13\n"
"    pop     r12\n"
"    pop     rbx\n"
"    mov     rsp, rbp\n"
"    pop     rbp\n"
"    ret\n\n"
);

 
    emit_globals();   // Génère les déclarations pour les variables globales (section .bss)
    emit_statics();   // Génère les labels pour les variables locales static (section .bss)

    // Point d’entrée du programme 
    fprintf(fout, "section .text\n\n");
    fprintf(fout, "global _start\n");
    fprintf(fout, "_start:\n");
    fprintf(fout, "    call main\n");         // Appeler main()
    fprintf(fout, "    mov rdi, rax\n");      // Transmettre code retour
    fprintf(fout, "    mov rax, 60\n");       // syscall: exit
    fprintf(fout, "    syscall\n");

    // Rechercher les définitions de fonctions 
    Node *declFoncts = FIRSTCHILD(root);
    while (declFoncts && declFoncts->label != n_DeclFoncts)
        declFoncts = declFoncts->nextSibling;

    if (!declFoncts) {
        closeAsmFile(&fout);
        return; // Aucun corps de fonction trouvé
    }

    // Générer chaque fonction déclarée 
    for (Node *fnNode = FIRSTCHILD(declFoncts); fnNode; fnNode = fnNode->nextSibling) {
        Node *header = FIRSTCHILD(fnNode);      // n_EnTeteFonct
        Node *body   = SECONDCHILD(fnNode);     // n_Corps
        if (!header || !body) {
            continue;
        }
        Node *ident = SECONDCHILD(header);      // n_IDENT
        if (!ident || ident->label != n_IDENT) {
            continue;
        }
        FuncSym *fn = st_lookup_func(gSt, ident->text);
        if (fn){
            emit_function(fn, body); 
        }           // Générer la fonction entière
    }

    closeAsmFile(&fout);  // Fermer proprement le fichier
 }
 