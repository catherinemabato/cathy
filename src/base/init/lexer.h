/* 
 * (C) Copyright 1992, ..., 2003 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

extern int yylex(YYSTYPE* yylval);
extern int include_stack_ptr;
extern char *include_fnames[];
extern int include_lines[];
extern int line_count;
extern int last_include;
