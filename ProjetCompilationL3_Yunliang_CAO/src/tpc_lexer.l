%{
#include <stdio.h>
#include <stdlib.h>
#include <tree.h>
#include "tpc_parser.h"   

int yycolumn = 1;
%}



IDENT      [a-zA-Z_][a-zA-Z0-9_]*
NUMBER     [0-9]+
CHAR       \'(\\(x[0-9a-fA-F]+|[0-7]{1,3}|.)|[^\\'\n])\'
STRING     \"(\\.|[^\\"])*\"
%x COMMENT
%option nounput
%option noyywrap
%option yylineno


%%

"/*"            { BEGIN COMMENT; }
<COMMENT>"*/"   { BEGIN INITIAL; }
<COMMENT>.|\n   

"//".*                      ;

"if"         { yycolumn += yyleng; return IF; }
"else"       { yycolumn += yyleng; return ELSE; }
"return"     { yycolumn += yyleng; return RETURN; }
"int"        { yycolumn += yyleng; yylval.str = strdup(yytext); return TYPE; }
"char"       { yycolumn += yyleng; yylval.str = strdup(yytext); return TYPE; }
"void"       { yycolumn += yyleng; yylval.str = strdup(yytext); return VOID; }
"static"     { yycolumn += yyleng; return STATIC; }
"while"      { yycolumn += yyleng; return WHILE; }



{IDENT}      { yycolumn += yyleng; yylval.str = strdup(yytext); return IDENT; }
{NUMBER}     { yycolumn += yyleng; yylval.num = atoi(yytext); return NUM; }
{CHAR}      {
                yycolumn += yyleng;
                char *raw = strdup(yytext + 1);
                raw[strlen(raw) - 1] = '\0';
                yylval.str = raw;
                return CHARACTER;

            }



"=="         { yycolumn += yyleng; yylval.str = strdup(yytext);return EQ; }
"!="         { yycolumn += yyleng; yylval.str = strdup(yytext); return EQ; }
"<="         { yycolumn += yyleng; yylval.str = strdup(yytext); return ORDER; }
">="         { yycolumn += yyleng; yylval.str = strdup(yytext); return ORDER; }
"<"          { yycolumn += yyleng; yylval.str = strdup(yytext); return ORDER; }
">"          { yycolumn += yyleng; yylval.str = strdup(yytext); return ORDER; }
"+"          { yycolumn += yyleng; yylval.str = strdup(yytext); return ADDSUB; }
"-"          { yycolumn += yyleng; yylval.str = strdup(yytext); return ADDSUB; }
"*"          { yycolumn += yyleng; yylval.str = strdup(yytext); return DIVSTAR; }
"/"          { yycolumn += yyleng; yylval.str = strdup(yytext); return DIVSTAR; }
"%"          { yycolumn += yyleng; yylval.str = strdup(yytext); return DIVSTAR; }
"="          { yycolumn += yyleng; yylval.str = strdup(yytext); return ASSIGN; }
"!"          { yycolumn += yyleng; yylval.str = strdup(yytext); return NOT; }

"&&"         { yycolumn += yyleng; yylval.str = strdup(yytext); return AND; }
"||"         { yycolumn += yyleng; yylval.str = strdup(yytext); return OR; }

";"          { yycolumn += yyleng; yylval.str = strdup(yytext); return SEMICOLON; }
","          { yycolumn += yyleng; yylval.str = strdup(yytext); return COMMA; }
"("          { yycolumn += yyleng; yylval.str = strdup(yytext); return LPAREN; }
")"          { yycolumn += yyleng; yylval.str = strdup(yytext); return RPAREN; }
"{"          { yycolumn += yyleng; yylval.str = strdup(yytext); return LBRACE; }
"}"          { yycolumn += yyleng; yylval.str = strdup(yytext); return RBRACE; }

         
         
[ \n]        {yycolumn = 1;}        
[ \t\r]+    { yycolumn += yyleng; }


.            { fprintf(stderr, "illegal: %s\\n", yytext); exit(1); }

%%

