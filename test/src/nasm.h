#ifndef NASM_H
#define NASM_H

#include <stdio.h>
#include <stdlib.h>


// Génère le code assembleur NASM à partir de l'AST et de la table des symboles
void generate_nasm(Symtab *st, Node *root);

// Définit le nom du fichier de sortie pour le code assembleur généré
void set_output_filename(const char *filename);


#endif