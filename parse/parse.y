/*
 * $Date: 1994/03/15 02:08:40 $
 * $Source: /home/src/dosemu0.50pl1/parse/RCS/parse.y,v $
 * $Revision: 1.3 $
 * $State: Exp $
 */

%union {
char *name;
int val;
}

%token <val> CC_START 
%token <val> CC_END
%token <name> WORD
%token <val> NUM
%token <name> STR
%token <name> PORTS
%token <name> DISK

%type <name> sectname

%%

line:  /* empty */
       | line thing

giblet:   WORD  { printf("WORD: %s\n", $1); }
	| NUM { }
	| STR { }

internal: /* empty */
	| internal giblet

thing:	section
	| internal

sectname:   PORTS  { $$ = $1; }
	  | DISK   { $$ = $1; }

section: sectname '{' internal '}'   { printf("heyhey: section %s\n", $1); }

%%

yyerror()
{
	printf("ERR: \n");
	return;
}

main(int argc, char **argv)
{
	yyparse();
	return;
}


