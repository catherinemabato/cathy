#ifndef DAVIS_SLANG_H_
#define DAVIS_SLANG_H_
/* -*- mode: C; mode: fold; -*- */
/* Copyright (c) 1992, 1998 John E. Davis
 * This file is part of the S-Lang library.
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Perl Artistic License.
 */
#define SLANG_VERSION 10003
/*{{{ System Dependent Macros and Typedefs */

#if defined(__WATCOMC__) && defined(DOS)
# ifndef __MSDOS__
#  define __MSDOS__
# endif
# ifndef  DOS386
#  define  DOS386
# endif
# ifndef IBMPC_SYSTEM
#  define IBMPC_SYSTEM
# endif
#endif /* __watcomc__ */

#ifdef unix
# ifndef __unix__
#  define __unix__ 1
# endif
#endif

#if !defined(__GO32__)
# ifdef __unix__
#  define REAL_UNIX_SYSTEM
# endif
#endif

/* Set of the various defines for pc systems.  This includes OS/2 */
#ifdef __GO32__
# ifndef __DJGPP__
#  define __DJGPP__ 1
# endif
# ifndef IBMPC_SYSTEM
#   define IBMPC_SYSTEM
# endif
#endif

#ifdef __BORLANDC__
# ifndef IBMPC_SYSTEM
#  define IBMPC_SYSTEM
# endif
#endif

#ifdef __MSDOS__
# ifndef IBMPC_SYSTEM
#   define IBMPC_SYSTEM
# endif
#endif

#if defined(OS2) || defined(__os2__)
# ifndef IBMPC_SYSTEM
#   define IBMPC_SYSTEM
# endif
# ifndef __os2__
#  define __os2__
# endif
#endif

#if defined(__NT__) || defined(__MINGW32__) || defined(__CYGWIN32__)
# ifndef IBMPC_SYSTEM
#  define IBMPC_SYSTEM
# endif
#endif

#if defined(IBMPC_SYSTEM) || defined(VMS)
# ifdef REAL_UNIX_SYSTEM
#  undef REAL_UNIX_SYSTEM
# endif
#endif

#ifdef __cplusplus
extern "C" {
#endif
#if 0
}
#endif
#include <stdio.h>
#include <stdarg.h>

/* ---------------------------- Generic Macros ----------------------------- */

/*  __SC__ is defined for Symantec C++
   DOS386 is defined for -mx memory model, 32 bit DOS extender. */

#if defined(__SC__) && !defined(DOS386)
# include <dos.h>
#endif

#if defined(__BORLANDC__)
# include <alloc.h>
#endif

#if defined (__cplusplus) || defined(__STDC__)
  typedef void *VOID_STAR;
#else
  typedef unsigned char *VOID_STAR;
#endif

typedef int (*FVOID_STAR)(void);

#if defined(__BORLANDC__)
# define SLFREE(buf)  farfree((void far *)(buf))
# define SLMALLOC(x) farmalloc((unsigned long) (x))
# define SLREALLOC(buf, n) farrealloc((void far *) (buf), (unsigned long) (n))
# define SLCALLOC(n, m) farcalloc((unsigned long) (n), (unsigned long) (m))
#else
# if defined(VMS) && !defined(__DECC)
#  define SLFREE VAXC$FREE_OPT
#  define SLMALLOC VAXC$MALLOC_OPT
#  define SLREALLOC VAXC$REALLOC_OPT
#  define SLCALLOC VAXC$CALLOC_OPT
# else
#  define SLFREE(x) free((char *)(x))
#  define SLMALLOC malloc
#  if defined(__cplusplus) && !defined(__BEOS__)
#   define SLREALLOC(p,n) realloc((malloc_t) (p), (n))
#  else
#   define SLREALLOC realloc
#  endif
#  define SLCALLOC calloc
# endif
#endif

   extern char *SLdebug_malloc (unsigned long);
   extern char *SLdebug_calloc (unsigned long, unsigned long);
   extern char *SLdebug_realloc (char *, unsigned long);
   extern void SLdebug_free (char *);
   extern void SLmalloc_dump_statistics (void);
   extern char *SLstrcpy(register char *, register char *);
   extern int SLstrcmp(register char *, register char *);
   extern char *SLstrncpy(char *, register char *, register  int);

   extern void SLmemset (char *, char, int);
   extern char *SLmemchr (register char *, register char, register int);
   extern char *SLmemcpy (char *, char *, int);
   extern int SLmemcmp (char *, char *, int);

/*}}}*/

/*{{{ Interpreter Typedefs */

typedef struct _SLang_Name_Type
{
   char *name;
   struct _SLang_Name_Type *next;
   char name_type;
#define SLANG_LVARIABLE		0x01
#define SLANG_GVARIABLE 	0x02
#define SLANG_IVARIABLE 	0x03           /* intrinsic variables */
   /* Note!!! For Macro MAKE_VARIABLE below to work, SLANG_IVARIABLE Must
    be 1 less than SLANG_RVARIABLE!!! */
#define SLANG_RVARIABLE		0x04	       /* read only variable */
#define SLANG_INTRINSIC 	0x05
#define SLANG_FUNCTION  	0x06
#define SLANG_MATH_UNARY  	0x07
#define SLANG_APP_UNARY  	0x08

   /* Rest of fields depend on name type */
}
SLang_Name_Type;

typedef struct
{
   char *name;
   struct _SLang_Name_Type *next;      /* this is for the hash table */
   char name_type;

   FVOID_STAR i_fun;		       /* address of object */
   
   /* Do not change this without modifying slang.c:execute_intrinsic_fun */
#define SLANG_MAX_INTRIN_ARGS	7
   unsigned char arg_types [SLANG_MAX_INTRIN_ARGS];
   unsigned char num_args;
   unsigned char return_type;
}
SLang_Intrin_Fun_Type;

typedef struct
{
   char *name;
   SLang_Name_Type *next;
   char name_type;

   VOID_STAR addr;
   unsigned char type;
}
SLang_Intrin_Var_Type;

typedef struct
{
   char *name;
   SLang_Name_Type *next;
   char name_type;

   int unary_op;
}
SLang_App_Unary_Type;

typedef struct
{
   char *name;
   SLang_Name_Type *next;
   char name_type;

   int unary_op;
}
SLang_Math_Unary_Type;



extern int SLadd_intrin_fun_table (SLang_Intrin_Fun_Type *, char *);
extern int SLadd_intrin_var_table (SLang_Intrin_Var_Type *, char *);
extern int SLadd_app_unary_table (SLang_App_Unary_Type *, char *);
extern int SLadd_math_unary_table (SLang_Math_Unary_Type *, char *);

typedef struct SLang_Load_Type
{
   int type;

   VOID_STAR client_data;
   /* Pointer to data that client needs for loading */

   int auto_declare_globals;
   /* if non-zero, undefined global variables are declared */

   char *(*read)(struct SLang_Load_Type *);
   /* function to call to read next line from obj. */

   unsigned int line_num;
   /* Number of lines read, used for error reporting */

   int parse_level;
   /* 0 if at top level of parsing */

   char *name;
   /* Name of this object, e.g., filename */

   unsigned long reserved[4];
   /* For future expansion */
} SLang_Load_Type;

extern SLang_Load_Type *SLallocate_load_type (char *);
extern void SLdeallocate_load_type (SLang_Load_Type *);
extern int SLang_load_object (SLang_Load_Type *);
extern int (*SLang_Load_File_Hook)(char *);

/* Returns SLang_Error upon failure */

#if defined(ultrix) && !defined(__GNUC__)
# ifndef NO_PROTOTYPES
#  define NO_PROTOTYPES
# endif
#endif

#ifndef NO_PROTOTYPES
# define _PROTO(x) x
#else
# define _PROTO(x) ()
#endif

typedef struct SL_OOBinary_Type
{
   unsigned char data_type;	       /* partner type for binary op */

    int (*binary_function)_PROTO((int,
				 unsigned char, VOID_STAR, unsigned int,
				 unsigned char, VOID_STAR, unsigned int,
				 VOID_STAR));

   int (*binary_result) _PROTO((int, unsigned char, unsigned char, unsigned char *));
   struct SL_OOBinary_Type *next;
}
SL_OOBinary_Type;

typedef struct _SL_Typecast_Type
{
   unsigned char data_type;	       /* to_type */
   int allow_implicit;

   int (*typecast)_PROTO((unsigned char, VOID_STAR, unsigned int,
			  unsigned char, VOID_STAR));
   struct _SL_Typecast_Type *next;
}
SL_Typecast_Type;

#ifdef _SLANG_SOURCE_
typedef struct _SLang_Struct_Type SLang_Struct_Type;
#else
typedef int SLang_Struct_Type;
#endif

typedef struct
{
   unsigned char cl_class_type;
#define SLANG_CLASS_TYPE_MMT		0
#define SLANG_CLASS_TYPE_SCALAR		1
#define SLANG_CLASS_TYPE_VECTOR		2
#define SLANG_CLASS_TYPE_PTR		3

   unsigned int cl_data_type;	       /* SLANG_INTEGER_TYPE, etc... */
   char *cl_name;			       /* slstring type */

   unsigned int cl_sizeof_type;
   VOID_STAR cl_transfer_buf;	       /* cl_sizeof_type bytes*/

   /* Methods */

   /* Most of the method functions are prototyped:
    * int method (unsigned char type, VOID_STAR addr);
    * Here, @type@ represents the type of object that the method is asked
    * to deal with.  The second parameter @addr@ will contain the ADDRESS of
    * the object.  For example, if type is SLANG_INT_TYPE, then @addr@ will
    * actually be int *.  Similary, if type is SLANG_STRING_TYPE,
    * then @addr@ will contain the address of the string, i.e., char **.
    */

   void (*cl_destroy)_PROTO((unsigned char, VOID_STAR));
   /* Prototype: void destroy(unsigned type, VOID_STAR val)
    * Called to delete/free the object */

   char *(*cl_string)_PROTO((unsigned char, VOID_STAR));
   /* Prototype: char *to_string (unsigned char t, VOID_STAR p);
    * Here p is a pointer to the object for which a string representation
    * is to be returned.  The returned pointer is to be a MALLOCED string.
    */

   /* Prototype: void push(unsigned char type, VOID_STAR v);
    * Push a copy of the object of type @type@ at address @v@ onto the
    * stack.
    */
   int (*cl_push)_PROTO((unsigned char, VOID_STAR));

   /* Prototype: int pop(unsigned char type, VOID_STAR v);
    * Pops value from stack and assign it to object, whose address is @v@.
    */
   int (*cl_pop)_PROTO((unsigned char, VOID_STAR));

   int (*cl_unary_op_result_type)_PROTO((int, unsigned char, unsigned char *));
   int (*cl_unary_op)_PROTO((int, unsigned char, VOID_STAR, unsigned int, VOID_STAR));

   int (*cl_app_unary_op_result_type)_PROTO((int, unsigned char, unsigned char *));
   int (*cl_app_unary_op)_PROTO((int, unsigned char, VOID_STAR, unsigned int, VOID_STAR));

   /* If this function is non-NULL, it will be called for sin, cos, etc... */
#define SLMATH_SIN	1
#define SLMATH_COS	2
#define SLMATH_TAN	3
#define SLMATH_ATAN	4
#define SLMATH_ASIN	5
#define SLMATH_ACOS	6
#define SLMATH_EXP	7
#define SLMATH_LOG	8
#define SLMATH_SQRT	9
#define SLMATH_LOG10	10
#define SLMATH_REAL	11
#define SLMATH_IMAG	12
#define SLMATH_SINH	13
#define SLMATH_COSH	14
#define SLMATH_TANH	15
#define SLMATH_ATANH	16
#define SLMATH_ASINH	17
#define SLMATH_ACOSH	18
#define SLMATH_TODOUBLE	19

   int (*cl_math_op)_PROTO((int, unsigned char, VOID_STAR, unsigned int, VOID_STAR));
   int (*cl_math_op_result_type)_PROTO((int, unsigned char, unsigned char *));

   SL_OOBinary_Type *cl_binary_ops;
   SL_Typecast_Type *cl_typecast_funs;

   void (*cl_byte_code_destroy)_PROTO((unsigned char, VOID_STAR));
   void (*cl_user_destroy_fun)_PROTO((unsigned char, VOID_STAR));
   int (*cl_init_array_object)_PROTO((unsigned char, VOID_STAR));
   int (*cl_datatype_deref)_PROTO((unsigned char));
   SLang_Struct_Type *cl_struct_def;
   int (*cl_dereference) _PROTO((unsigned char, VOID_STAR));
   int (*cl_acopy) (unsigned char, VOID_STAR, VOID_STAR);
   int (*cl_apop) _PROTO((unsigned char, VOID_STAR));
   int (*cl_apush) _PROTO((unsigned char, VOID_STAR));
   int (*cl_push_literal) _PROTO((unsigned char, VOID_STAR));
   void (*cl_adestroy)_PROTO((unsigned char, VOID_STAR));
   int (*cl_push_intrinsic)_PROTO((unsigned char, VOID_STAR));
   int (*cl_void_typecast)_PROTO((unsigned char, VOID_STAR, unsigned int, unsigned char, VOID_STAR));
} SLang_Class_Type;

extern SLang_Class_Type *SLclass_allocate_class (char *);
extern int SLclass_register_class (SLang_Class_Type *, unsigned char, unsigned int, unsigned char);
extern int SLclass_set_string_function (SLang_Class_Type *, char *(*)(unsigned char, VOID_STAR));
extern int SLclass_set_destroy_function (SLang_Class_Type *, void (*)(unsigned char, VOID_STAR));
extern int SLclass_set_push_function (SLang_Class_Type *, int (*)(unsigned char, VOID_STAR));
extern int SLclass_set_pop_function (SLang_Class_Type *, int (*)(unsigned char, VOID_STAR));

extern int SLclass_add_unary_op (unsigned char,
				 int (*) (int,
					  unsigned char, VOID_STAR, unsigned int,
					  VOID_STAR),
				 int (*) (int, unsigned char, unsigned char *));

extern int
SLclass_add_app_unary_op (unsigned char,
			  int (*) (int,
				   unsigned char, VOID_STAR, unsigned int,
				   VOID_STAR),
			  int (*) (int, unsigned char, unsigned char *));

extern int
SLclass_add_binary_op (unsigned char, unsigned char,
		       int (*) (int,
				unsigned char, VOID_STAR, unsigned int,
				unsigned char, VOID_STAR, unsigned int,
				VOID_STAR),
		       int (*) (int, unsigned char, unsigned char, unsigned char *));

extern int
SLclass_add_math_op (unsigned char,
		     int (*)(int,
			     unsigned char, VOID_STAR, unsigned int,
			     VOID_STAR),
		     int (*)(int, unsigned char, unsigned char *));

extern int
SLclass_add_typecast (unsigned char /* from */, unsigned char /* to */,
		      int (*)_PROTO((unsigned char, VOID_STAR, unsigned int,
				     unsigned char, VOID_STAR)),
		      int	       /* allow implicit typecasts */
		      );

extern char *SLclass_get_datatype_name (unsigned char);

extern double SLcomplex_abs (double *);
extern double *SLcomplex_times (double *, double *, double *);
extern double *SLcomplex_divide (double *, double *, double *);
extern double *SLcomplex_sin (double *, double *);
extern double *SLcomplex_cos (double *, double *);
extern double *SLcomplex_tan (double *, double *);
extern double *SLcomplex_asin (double *, double *);
extern double *SLcomplex_acos (double *, double *);
extern double *SLcomplex_atan (double *, double *);
extern double *SLcomplex_exp (double *, double *);
extern double *SLcomplex_log (double *, double *);
extern double *SLcomplex_log10 (double *, double *);
extern double *SLcomplex_sqrt (double *, double *);
extern double *SLcomplex_sinh (double *, double *);
extern double *SLcomplex_cosh (double *, double *);
extern double *SLcomplex_tanh (double *, double *);
extern double *SLcomplex_pow (double *, double *, double *);
extern double SLmath_hypot (double x, double y);

/* Not implemented yet */
extern double *SLcomplex_asinh (double *, double *);
extern double *SLcomplex_acosh (double *, double *);
extern double *SLcomplex_atanh (double *, double *);

#ifdef _SLANG_SOURCE_
typedef struct _SLang_MMT_Type SLang_MMT_Type;
#else
typedef int SLang_MMT_Type;
#endif

extern void SLang_free_mmt (SLang_MMT_Type *);
extern VOID_STAR SLang_object_from_mmt (SLang_MMT_Type *);
extern SLang_MMT_Type *SLang_create_mmt (unsigned char, VOID_STAR);
extern int SLang_push_mmt (SLang_MMT_Type *);
extern SLang_MMT_Type *SLang_pop_mmt (unsigned char);
extern void SLang_inc_mmt (SLang_MMT_Type *);

/*}}}*/
/*{{{ Interpreter Function Prototypes */

  extern volatile int SLang_Error;
/* Non zero if error occurs.  Must be reset to zero to continue. */
/* error codes, severe errors are less than 0 */
#define SL_NOT_IMPLEMENTED		-1
#define SL_APPLICATION_ERROR		-2
#define SL_VARIABLE_UNINITIALIZED	-3
#define SL_MALLOC_ERROR			-4
#define SL_INTERNAL_ERROR		-5
#define SL_STACK_OVERFLOW		-6
#define SL_STACK_UNDERFLOW		-7
#define SL_UNDEFINED_NAME		-8
#define SL_SYNTAX_ERROR			-9
#define SL_DUPLICATE_DEFINITION		-10
#define SL_TYPE_MISMATCH		-11
#define SL_READONLY_ERROR		-12
#define SL_OBJ_UNKNOWN			-13
#define SL_UNKNOWN_ERROR		-14
#define SL_INVALID_PARM			-15
#define SL_TYPE_UNDEFINED_OP_ERROR	-16

#define SL_INTRINSIC_ERROR		1
/* Intrinsic error is an error generated by intrinsic functions */
#define SL_USER_BREAK			2
#define SL_DIVIDE_ERROR			3
#define SL_OBJ_NOPEN			4
#define SL_USER_ERROR			5

/* Compatibility */
#define USER_BREAK SL_USER_BREAK
#define INTRINSIC_ERROR SL_INTRINSIC_ERROR

  extern int SLang_Traceback;
  /* If non-zero, dump an S-Lang traceback upon error.  Available as
     _traceback in S-Lang. */

  extern char *SLang_User_Prompt;
  /* Prompt to use when reading from stdin */
  extern int SLang_Version;

extern void (*SLang_VMessage_Hook) (char *, va_list);
extern void SLang_vmessage (char *, ...);

  extern void (*SLang_Error_Hook)(char *);
  /* Pointer to application dependent error messaging routine.  By default,
     messages are displayed on stderr. */

  extern void (*SLang_Exit_Error_Hook)(char *, va_list);
  extern void SLang_exit_error (char *, ...);
  extern void (*SLang_Dump_Routine)(char *);
  /* Called if S-Lang traceback is enabled as well as other debugging
     routines (e.g., trace).  By default, these messages go to stderr. */

  extern void (*SLang_Interrupt)(void);
  /* function to call whenever inner interpreter is entered.  This is
     a good place to set SLang_Error to USER_BREAK. */

  extern void (*SLang_User_Clear_Error)(void);
  /* function that gets called when '_clear_error' is called. */

  /* If non null, these call C functions before and after a slang function. */
  extern void (*SLang_Enter_Function)(char *);
extern void (*SLang_Exit_Function)(char *);

extern int SLang_Num_Function_Args;

/* Functions: */

   extern int SLang_init_slang (void);
   /* This function is mandatory and must be called by all applications that
    * use the interpreter
    */
   extern int SLang_init_slfile (void);
   /* called if fputs, fgets, etc are need in S-Lang */
   extern int SLang_init_slmath (void);
   /* called if math functions sin, cos, etc... are needed. */
   extern int SLang_init_slunix (void);
   /* unix system functions chmod, stat, etc... */

   extern int SLang_load_file (char *);
   /* Load a file of S-Lang code for interpreting.  If the parameter is
    * NULL, input comes from stdin. */

   extern void SLang_restart(int);
   /* should be called if an error occurs.  If the passed integer is
    * non-zero, items are popped off the stack; otherwise, the stack is
    * left intact.  Any time the stack is believed to be trashed, this routine
    * should be called with a non-zero argument (e.g., if setjmp/longjmp is
    * called). */

   extern int SLang_byte_compile_file(char *, int);
   /* takes a file of S-Lang code and ``byte-compiles'' it for faster
    * loading.  The new filename is equivalent to the old except that a `c' is
    * appended to the name.  (e.g., init.sl --> init.slc).  The second
    * specified the method; currently, it is not used.
    */

   extern int SLang_autoload(char *, char *);
   /* Automatically load S-Lang function p1 from file p2.  This function
      is also available via S-Lang */

   extern int SLang_load_string(char *);
   /* Like SLang_load_file except input is from a null terminated string. */

   extern int SLdo_pop(void);
   /* pops item off stack and frees any memory associated with it */
   extern int SLdo_pop_n(unsigned int);
   /* pops n items off stack and frees any memory associated with them */

   extern int SLang_pop_integer(int *);
   /* pops integer *p0 from the stack.  Returns 0 upon success and non-zero
    * if the stack is empty or a type mismatch occurs, setting SLang_Error.
    */

   extern int SLpop_string (char **);
   extern int SLang_pop_string(char **, int *);
   /* pops string *p0 from stack.  If *p1 is non-zero, the string must be
    * freed after its use.  DO NOT FREE p0 if *p1 IS ZERO! Returns 0 upon
    * success */

   extern int SLang_pop_double(double *, int *, int *);
   /* Pops double *p1 from stack.  If *p3 is non-zero, *p1 was derived
      from the integer *p2. Returns zero upon success. */

   extern int SLang_pop_complex (double *, double *);
   extern int SLang_push_complex (double, double);

   extern int SLang_push_double(double);
   /* Push double onto stack */

   extern int SLang_push_string(char *);
   /* Push string p1 onto stack */

   extern int SLang_push_integer(int);
   /* push integer p1 on stack */

   extern int SLang_push_malloced_string(char *);
   /* The normal SLang_push_string mallocs space for the string.  This one
    * does not.  DO NOT FREE IT IF YOU USE THIS ROUTINE.  Also do not
    * use this routine if the string was a product of SLcreate_string.
    * If -1 is returned, it failed and the original string is not freed.
    */

   extern int SLang_is_defined(char *);
   /* Return non-zero is p1 is defined otherwise returns 0. */

   extern int SLang_run_hooks(char *, unsigned int, ...);
   /* calls S-Lang function p1 pushing p2 strings in the variable argument
    * list onto the stack first.
    * Returns -1 upon error, 1 if hooks exists and it ran,
    * or 0 if hook does not exist.  Thus it returns non-zero is hook was called.
    */

extern int SLexecute_function (SLang_Name_Type *);
extern int SLang_execute_function(char *);
extern int SLang_end_arg_list (void);
extern int SLang_start_arg_list (void);

extern void SLang_verror (int, char *, ...);

extern void SLang_doerror(char *);
   /* set SLang_Error and display p1 as error message */

extern int SLang_add_intrinsic_array (char *,   /* name */
				      unsigned char,   /* type */
				      int,   /* readonly */
				      VOID_STAR,   /* data */
				      unsigned int, ...);   /* num dims */


extern int SLextract_list_element (char *, unsigned int, char,
				   char *, unsigned int);

extern void SLexpand_escaped_string (register char *, register char *,
				     register char *);

extern SLang_Name_Type *SLang_get_function (char *);

extern int SLang_peek_at_stack (void);
/* Returns type of next object on stack-- -1 upon stack underflow. */
extern void SLmake_lut (unsigned char *, unsigned char *, unsigned char);

   extern int SLang_guess_type (char *);

/*}}}*/

/*{{{ Misc Functions */

extern char *SLmake_string (char *);
extern char *SLmake_nstring (char *, unsigned int);
/* Returns a null terminated string made from the first n characters of the
 * string.
 */

/* The string created by this routine must be freed by SLang_free_slstring
 * and nothing else!!  Also these strings must not be modified.  Use
 * SLmake_string if you intend to modify them!!
 */
extern char *SLang_create_nslstring (char *, unsigned int);
extern char *SLang_create_slstring (char *);
extern void SLang_free_slstring (char *);    /* handles NULL */
extern int SLang_pop_slstring (char **);   /* free with SLang_free_slstring */
extern char *SLang_concat_slstrings (char *a, char *b);
extern char *SLang_create_static_slstring (char *);   /* adds a string that will not get deleted */
extern void SLstring_dump_stats (void);

extern char *SLmalloc (unsigned int);
extern char *SLcalloc (unsigned int, unsigned int);
extern void SLfree(char *);	       /* This function handles NULL */
extern char *SLrealloc (char *, unsigned int);

extern char *SLcurrent_time_string (void);

extern int SLatoi(unsigned char *);

extern int SLang_pop_fileptr (SLang_MMT_Type **, FILE **);

/*}}}*/

/*{{{ SLang getkey interface Functions */

#ifdef REAL_UNIX_SYSTEM
extern int SLang_TT_Baud_Rate;
extern int SLang_TT_Read_FD;
#endif

extern int SLang_init_tty (int, int, int);
/* Initializes the tty for single character input.  If the first parameter *p1
 * is in the range 0-255, it will be used for the abort character;
 * otherwise, (unix only) if it is -1, the abort character will be the one
 * used by the terminal.  If the second parameter p2 is non-zero, flow
 * control is enabled.  If the last parmeter p3 is zero, output processing
 * is NOT turned on.  A value of zero is required for the screen management
 * routines. Returns 0 upon success. In addition, if SLang_TT_Baud_Rate ==
 * 0 when this function is called, SLang will attempt to determine the
 * terminals baud rate.  As far as the SLang library is concerned, if
 * SLang_TT_Baud_Rate is less than or equal to zero, the baud rate is
 * effectively infinite.
 */

extern void SLang_reset_tty (void);
/* Resets tty to what it was prior to a call to SLang_init_tty */
#ifdef REAL_UNIX_SYSTEM
extern void SLtty_set_suspend_state (int);
   /* If non-zero argument, terminal driver will be told to react to the
    * suspend character.  If 0, it will not.
    */
extern int (*SLang_getkey_intr_hook) (void);
#endif

#define SLANG_GETKEY_ERROR 0xFFFF
extern unsigned int SLang_getkey (void);
/* reads a single key from the tty.  If the read fails,  0xFFFF is returned. */

#ifdef IBMPC_SYSTEM
extern int SLgetkey_map_to_ansi (int);
#endif

extern int SLang_ungetkey_string (unsigned char *, unsigned int);
extern int SLang_buffer_keystring (unsigned char *, unsigned int);
extern int SLang_ungetkey (unsigned char);
extern void SLang_flush_input (void);
extern int SLang_input_pending (int);
extern int SLang_Abort_Char;
/* The value of the character (0-255) used to trigger SIGINT */
extern int SLang_Ignore_User_Abort;
/* If non-zero, pressing the abort character will not result in USER_BREAK
 * SLang_Error. */

extern int SLang_set_abort_signal (void (*)(int));
/* If SIGINT is generated, the function p1 will be called.  If p1 is NULL
 * the SLang_default signal handler is called.  This sets SLang_Error to
 * USER_BREAK.  I suspect most users will simply want to pass NULL.
 */
extern unsigned int SLang_Input_Buffer_Len;

extern volatile int SLKeyBoard_Quit;

#ifdef VMS
/* If this function returns -1, ^Y will be added to input buffer. */
extern int (*SLtty_VMS_Ctrl_Y_Hook) (void);
#endif
/*}}}*/

/*{{{ SLang Keymap routines */

typedef struct SLKeymap_Function_Type
  {
      char *name;
      int (*f)(void);
  }
SLKeymap_Function_Type;

typedef struct SLang_Key_Type
  {
     unsigned char str[13];	       /* key sequence */
#define SLKEY_F_INTERPRET	0x01
#define SLKEY_F_INTRINSIC	0x02
#define SLKEY_F_KEYSYM		0x03
     unsigned char type;	       /* type of function */
     union
       {
	  char *s;
	  FVOID_STAR f;
	  unsigned int keysym;
       }
     f;
     struct SLang_Key_Type *next;      /* */
  }
SLang_Key_Type;

#define MAX_KEYMAP_NAME_LEN 8
typedef struct SLKeyMap_List_Type
{
   char name[MAX_KEYMAP_NAME_LEN + 1];
   SLang_Key_Type *keymap;
   SLKeymap_Function_Type *functions;  /* intrinsic functions */
}
SLKeyMap_List_Type;

/* This is arbitrary but I have got to start somewhere */
#if defined(__MSDOS__) && !defined(__unix__)
# define SLANG_MAX_KEYMAPS 10
#else
# define SLANG_MAX_KEYMAPS 30
#endif
extern SLKeyMap_List_Type SLKeyMap_List[SLANG_MAX_KEYMAPS];

extern char *SLang_process_keystring(char *);

extern int SLkm_define_key (char *, FVOID_STAR, SLKeyMap_List_Type *);

extern int SLang_define_key(char *, char *, SLKeyMap_List_Type *);
/* Like define_key1 except that p2 is a string that is to be associated with
 * a function in the functions field of p3.  This routine calls define_key1.
 */

extern int SLkm_define_keysym (char *, unsigned int, SLKeyMap_List_Type *);

extern void SLang_undefine_key(char *, SLKeyMap_List_Type *);

extern SLKeyMap_List_Type *SLang_create_keymap(char *, SLKeyMap_List_Type *);
/* create and returns a pointer to a new keymap named p1 created by copying
 * keymap p2.  If p2 is NULL, it is up to the calling routine to initialize
 * the keymap.
 */

extern char *SLang_make_keystring(unsigned char *);

extern SLang_Key_Type *SLang_do_key(SLKeyMap_List_Type *, int (*)(void));
/* read a key using keymap p1 with getkey function p2 */

extern
     FVOID_STAR
     SLang_find_key_function(char *, SLKeyMap_List_Type *);

extern SLKeyMap_List_Type *SLang_find_keymap(char *);

extern int SLang_Last_Key_Char;
extern int SLang_Key_TimeOut_Flag;

/*}}}*/

/*{{{ SLang Readline Interface */

typedef struct SLang_Read_Line_Type
{
   struct SLang_Read_Line_Type *prev, *next;
   unsigned char *buf;
   int buf_len;			       /* number of chars in the buffer */
   int num;			       /* num and misc are application specific*/
   int misc;
} SLang_Read_Line_Type;

/* Maximum size of display */
#define SLRL_DISPLAY_BUFFER_SIZE 256

typedef struct
{
   SLang_Read_Line_Type *root, *tail, *last;
   unsigned char *buf;		       /* edit buffer */
   int buf_len;			       /* sizeof buffer */
   int point;			       /* current editing point */
   int tab;			       /* tab width */
   int len;			       /* current line size */

   /* display variables */
   int edit_width;		       /* length of display field */
   int curs_pos;			       /* current column */
   int start_column;		       /* column offset of display */
   int dhscroll;		       /* amount to use for horiz scroll */
   char *prompt;

   FVOID_STAR last_fun;		       /* last function executed by rl */

   /* These two contain an image of what is on the display */
   unsigned char upd_buf1[SLRL_DISPLAY_BUFFER_SIZE];
   unsigned char upd_buf2[SLRL_DISPLAY_BUFFER_SIZE];
   unsigned char *old_upd, *new_upd;   /* pointers to previous two buffers */
   int new_upd_len, old_upd_len;       /* length of output buffers */

   SLKeyMap_List_Type *keymap;

   /* tty variables */
   unsigned int flags;		       /*  */
#define SL_RLINE_NO_ECHO	1
#define SL_RLINE_USE_ANSI	2
#define SL_RLINE_BLINK_MATCH	4
   unsigned int (*getkey)(void);       /* getkey function -- required */
   void (*tt_goto_column)(int);
   void (*tt_insert)(char);
   void (*update_hook)(unsigned char *, int, int);
   /* The update hook is called with a pointer to a buffer p1 that contains
    * an image of what the update hook is suppoed to produce.  The length
    * of the buffer is p2 and after the update, the cursor is to be placed
    * in column p3.
    */
   /* This function is only called when blinking matches */
   int (*input_pending)(int);
} SLang_RLine_Info_Type;

extern int SLang_RL_EOF_Char;

extern SLang_Read_Line_Type * SLang_rline_save_line (SLang_RLine_Info_Type *);
extern int SLang_init_readline (SLang_RLine_Info_Type *);
extern int SLang_read_line (SLang_RLine_Info_Type *);
extern int SLang_rline_insert (char *);
extern void SLrline_redraw (SLang_RLine_Info_Type *);
extern int SLang_Rline_Quit;

/*}}}*/

/*{{{ Low Level Screen Output Interface */

extern unsigned long SLtt_Num_Chars_Output;
extern int SLtt_Baud_Rate;

typedef unsigned long SLtt_Char_Type;

#define SLTT_BOLD_MASK	0x01000000
#define SLTT_BLINK_MASK	0x02000000
#define SLTT_ULINE_MASK	0x04000000
#define SLTT_REV_MASK	0x08000000
#define SLTT_ALTC_MASK  0x10000000

extern int SLtt_Screen_Rows;
extern int SLtt_Screen_Cols;
extern int SLtt_Term_Cannot_Insert;
extern int SLtt_Term_Cannot_Scroll;
extern int SLtt_Use_Ansi_Colors;
extern int SLtt_Ignore_Beep;
#if defined(REAL_UNIX_SYSTEM)
extern int SLtt_Force_Keypad_Init;
#endif

#ifndef __GO32__
#if defined(VMS) || defined(REAL_UNIX_SYSTEM)
extern int SLtt_Blink_Mode;
extern int SLtt_Use_Blink_For_ACS;
extern int SLtt_Newline_Ok;
extern int SLtt_Has_Alt_Charset;
extern int SLtt_Has_Status_Line;       /* if 0, NO.  If > 0, YES, IF -1, ?? */
# ifndef VMS
extern int SLtt_Try_Termcap;
# endif
#endif
#endif

#if defined(IBMPC_SYSTEM)
extern int SLtt_Msdos_Cheap_Video;
#endif

extern int SLtt_flush_output (void);
extern void SLtt_set_scroll_region(int, int);
extern void SLtt_reset_scroll_region(void);
extern void SLtt_reverse_video (int);
extern void SLtt_bold_video (void);
extern void SLtt_begin_insert(void);
extern void SLtt_end_insert(void);
extern void SLtt_del_eol(void);
extern void SLtt_goto_rc (int, int);
extern void SLtt_delete_nlines(int);
extern void SLtt_delete_char(void);
extern void SLtt_erase_line(void);
extern void SLtt_normal_video(void);
extern void SLtt_cls(void);
extern void SLtt_beep(void);
extern void SLtt_reverse_index(int);
extern void SLtt_smart_puts(unsigned short *, unsigned short *, int, int);
extern void SLtt_write_string (char *);
extern void SLtt_putchar(char);
extern int SLtt_init_video (void);
extern int SLtt_reset_video (void);
extern void SLtt_get_terminfo(void);
extern void SLtt_get_screen_size (void);
extern int SLtt_set_cursor_visibility (int);

extern int SLtt_set_mouse_mode (int, int);

#if defined(VMS) || defined(REAL_UNIX_SYSTEM)
extern int SLtt_initialize (char *);
extern void SLtt_enable_cursor_keys(void);
extern void SLtt_set_term_vtxxx(int *);
extern void SLtt_set_color_esc (int, char *);
extern void SLtt_wide_width(void);
extern void SLtt_narrow_width(void);
extern void SLtt_set_alt_char_set (int);
extern int SLtt_write_to_status_line (char *, int);
extern void SLtt_disable_status_line (void);
# ifdef REAL_UNIX_SYSTEM
   extern char *SLtt_tgetstr (char *);
   extern int SLtt_tgetnum (char *);
   extern int SLtt_tgetflag (char *);
   extern char *SLtt_tigetent (char *);
   extern char *SLtt_tigetstr (char *, char **);
   extern int SLtt_tigetnum (char *, char **);
# endif
#endif

extern SLtt_Char_Type SLtt_get_color_object (int);
extern void SLtt_set_color_object (int, SLtt_Char_Type);
extern void SLtt_set_color (int, char *, char *, char *);
extern void SLtt_set_mono (int, char *, SLtt_Char_Type);
extern void SLtt_add_color_attribute (int, SLtt_Char_Type);
extern void SLtt_set_color_fgbg (int, SLtt_Char_Type, SLtt_Char_Type);

/*}}}*/

/*{{{ SLang Preprocessor Interface */

typedef struct
{
   int this_level;
   int exec_level;
   int prev_exec_level;
   char preprocess_char;
   char comment_char;
   unsigned char flags;
#define SLPREP_BLANK_LINES_OK	1
#define SLPREP_COMMENT_LINES_OK	2
}
SLPreprocess_Type;

extern int SLprep_open_prep (SLPreprocess_Type *);
extern void SLprep_close_prep (SLPreprocess_Type *);
extern int SLprep_line_ok (char *, SLPreprocess_Type *);
   extern int SLdefine_for_ifdef (char *);
   /* Adds a string to the SLang #ifdef preparsing defines. SLang already
      defines MSDOS, UNIX, and VMS on the appropriate system. */
extern int (*SLprep_exists_hook) (char *, char);

/*}}}*/

/*{{{ SLsmg Screen Management Functions */

extern void SLsmg_fill_region (int, int, unsigned int, unsigned int, unsigned char);
extern void SLsmg_set_char_set (int);
#ifndef IBMPC_SYSTEM
extern int SLsmg_Scroll_Hash_Border;
#endif
extern int SLsmg_suspend_smg (void);
extern int SLsmg_resume_smg (void);
extern void SLsmg_erase_eol (void);
extern void SLsmg_gotorc (int, int);
extern void SLsmg_erase_eos (void);
extern void SLsmg_reverse_video (void);
extern void SLsmg_set_color (int);
extern void SLsmg_normal_video (void);
extern void SLsmg_printf (char *, ...);
extern void SLsmg_vprintf (char *, va_list);
extern void SLsmg_write_string (char *);
extern void SLsmg_write_nstring (char *, unsigned int);
extern void SLsmg_write_char (char);
extern void SLsmg_write_nchars (char *, unsigned int);
extern void SLsmg_write_wrapped_string (char *, int, int, unsigned int, unsigned int, int);
extern void SLsmg_cls (void);
extern void SLsmg_refresh (void);
extern void SLsmg_touch_lines (int, unsigned int);
extern int SLsmg_init_smg (void);
extern void SLsmg_reset_smg (void);
extern unsigned short SLsmg_char_at(void);
extern void SLsmg_set_screen_start (int *, int *);
extern void SLsmg_draw_hline (unsigned int);
extern void SLsmg_draw_vline (int);
extern void SLsmg_draw_object (int, int, unsigned char);
extern void SLsmg_draw_box (int, int, unsigned int, unsigned int);
extern int SLsmg_get_column(void);
extern int SLsmg_get_row(void);
extern void SLsmg_forward (int);
extern void SLsmg_write_color_chars (unsigned short *, unsigned int);
extern unsigned int SLsmg_read_raw (unsigned short *, unsigned int);
extern unsigned int SLsmg_write_raw (unsigned short *, unsigned int);

extern int SLsmg_Display_Eight_Bit;
extern int SLsmg_Tab_Width;
extern int SLsmg_Newline_Moves;
extern int SLsmg_Backspace_Moves;

#ifdef IBMPC_SYSTEM
# define SLSMG_HLINE_CHAR	0xC4
# define SLSMG_VLINE_CHAR	0xB3
# define SLSMG_ULCORN_CHAR	0xDA
# define SLSMG_URCORN_CHAR	0xBF
# define SLSMG_LLCORN_CHAR	0xC0
# define SLSMG_LRCORN_CHAR	0xD9
# define SLSMG_RTEE_CHAR	0xB4
# define SLSMG_LTEE_CHAR	0xC3
# define SLSMG_UTEE_CHAR	0xC2
# define SLSMG_DTEE_CHAR	0xC1
# define SLSMG_PLUS_CHAR	0xC5
/* There are several to choose from: 0xB0, 0xB1, and 0xB2 */
# define SLSMG_CKBRD_CHAR	0xB0
#else
# define SLSMG_HLINE_CHAR	'q'
# define SLSMG_VLINE_CHAR	'x'
# define SLSMG_ULCORN_CHAR	'l'
# define SLSMG_URCORN_CHAR	'k'
# define SLSMG_LLCORN_CHAR	'm'
# define SLSMG_LRCORN_CHAR	'j'
# define SLSMG_CKBRD_CHAR	'a'
# define SLSMG_RTEE_CHAR	'u'
# define SLSMG_LTEE_CHAR	't'
# define SLSMG_UTEE_CHAR	'w'
# define SLSMG_DTEE_CHAR	'v'
# define SLSMG_PLUS_CHAR	'n'
#endif

#ifndef IBMPC_SYSTEM
# define SLSMG_COLOR_BLACK		0x000000
# define SLSMG_COLOR_RED		0x000001
# define SLSMG_COLOR_GREEN		0x000002
# define SLSMG_COLOR_BROWN		0x000003
# define SLSMG_COLOR_BLUE		0x000004
# define SLSMG_COLOR_MAGENTA		0x000005
# define SLSMG_COLOR_CYAN		0x000006
# define SLSMG_COLOR_LGRAY		0x000007
# define SLSMG_COLOR_GRAY		0x000008
# define SLSMG_COLOR_BRIGHT_RED		0x000009
# define SLSMG_COLOR_BRIGHT_GREEN	0x00000A
# define SLSMG_COLOR_BRIGHT_BROWN	0x00000B
# define SLSMG_COLOR_BRIGHT_BLUE	0x00000C
# define SLSMG_COLOR_BRIGHT_CYAN	0x00000D
# define SLSMG_COLOR_BRIGHT_MAGENTA	0x00000E
# define SLSMG_COLOR_BRIGHT_WHITE	0x00000F
#endif

/*}}}*/

/*{{{ SLang Keypad Interface */

#define SL_KEY_ERR		0xFFFF

#define SL_KEY_UP		0x101
#define SL_KEY_DOWN		0x102
#define SL_KEY_LEFT		0x103
#define SL_KEY_RIGHT		0x104
#define SL_KEY_PPAGE		0x105
#define SL_KEY_NPAGE		0x106
#define SL_KEY_HOME		0x107
#define SL_KEY_END		0x108
#define SL_KEY_A1		0x109
#define SL_KEY_A3		0x10A
#define SL_KEY_B2		0x10B
#define SL_KEY_C1		0x10C
#define SL_KEY_C3		0x10D
#define SL_KEY_REDO		0x10E
#define SL_KEY_UNDO		0x10F
#define SL_KEY_BACKSPACE	0x110
#define SL_KEY_ENTER		0x111
#define SL_KEY_IC		0x112
#define SL_KEY_DELETE		0x113

#define SL_KEY_F0		0x200
#define SL_KEY_F(X)		(SL_KEY_F0 + X)

/* I do not intend to use keysymps > 0x1000.  Applications can use those. */
/* Returns 0 upon success or -1 upon error. */
extern int SLkp_define_keysym (char *, unsigned int);

/* This function must be called AFTER SLtt_get_terminfo and not before. */
extern int SLkp_init (void);

/* This function uses SLang_getkey and assumes that what ever initialization
 * is required for SLang_getkey has been performed.
 */
extern int SLkp_getkey (void);

/*}}}*/

/*{{{ SLang Scroll Interface */

typedef struct _SLscroll_Type
{
   struct _SLscroll_Type *next;
   struct _SLscroll_Type *prev;
   unsigned int flags;
}
SLscroll_Type;

typedef struct
{
   unsigned int flags;
   SLscroll_Type *top_window_line;   /* list element at top of window */
   SLscroll_Type *bot_window_line;   /* list element at bottom of window */
   SLscroll_Type *current_line;    /* current list element */
   SLscroll_Type *lines;	       /* first list element */
   unsigned int nrows;		       /* number of rows in window */
   unsigned int hidden_mask;	       /* applied to flags in SLscroll_Type */
   unsigned int line_num;	       /* current line number (visible) */
   unsigned int num_lines;	       /* total number of lines (visible) */
   unsigned int window_row;	       /* row of current_line in window */
   unsigned int border;		       /* number of rows that form scroll border */
   int cannot_scroll;		       /* should window scroll or recenter */
}
SLscroll_Window_Type;

extern int SLscroll_find_top (SLscroll_Window_Type *);
extern int SLscroll_find_line_num (SLscroll_Window_Type *);
extern unsigned int SLscroll_next_n (SLscroll_Window_Type *, unsigned int);
extern unsigned int SLscroll_prev_n (SLscroll_Window_Type *, unsigned int);
extern int SLscroll_pageup (SLscroll_Window_Type *);
extern int SLscroll_pagedown (SLscroll_Window_Type *);

/*}}}*/

/*{{{ Signal Routines */

typedef void SLSig_Fun_Type (int);
extern SLSig_Fun_Type *SLsignal (int, SLSig_Fun_Type *);
extern SLSig_Fun_Type *SLsignal_intr (int, SLSig_Fun_Type *);
extern int SLsig_block_signals (void);
extern int SLsig_unblock_signals (void);
extern int SLsystem (char *);
/*}}}*/

/*{{{ Interpreter Macro Definitions */

/* The definitions here are for objects that may be on the run-time stack.
 * They are actually sub_types of literal and data main_types.
 * Note that the return types for MAKE_INTRINSIC defined intrinsic functions
 * must be numbered from 0x1 - 0x0F.
 */
#define SLANG_UNDEFINED_TYPE	0x00   /* MUST be 0 */
#define SLANG_VOID_TYPE		0x01   /* also matches ANY type */
#define SLANG_INT_TYPE 		0x02
#define SLANG_DOUBLE_TYPE	0x03
#define SLANG_CHAR_TYPE		0x04
#define SLANG_INTP_TYPE		0x05
/* An object of SLANG_INTP_TYPE should never really occur on the stack.  Rather,
 * the integer to which it refers will be there instead.  It is defined here
 * because it is a valid type for MAKE_VARIABLE.
 */
#define SLANG_REF_TYPE		0x06
/* SLANG_REF_TYPE refers to an object on the stack that is a pointer (reference)
 * to some other object.
 */
#define SLANG_COMPLEX_TYPE	0x07
#define SLANG_NULL_TYPE		0x08
#define SLANG_STRING_TYPE	0x0F

#define SLANG_STRUCT_TYPE	0x11
#define SLANG_ARRAY_TYPE	0x20
#define SLANG_DATATYPE_TYPE	0x21
#define SLANG_FILE_PTR_TYPE	0x22

/* Compatibility */
#ifdef FLOAT_TYPE
# undef FLOAT_TYPE
#endif
#define VOID_TYPE SLANG_VOID_TYPE
#define INT_TYPE SLANG_INT_TYPE
#define INTP_TYPE SLANG_INTP_TYPE
#define FLOAT_TYPE SLANG_DOUBLE_TYPE
#define ARRAY_TYPE SLANG_ARRAY_TYPE
#define CHAR_TYPE SLANG_CHAR_TYPE
#define STRING_TYPE SLANG_STRING_TYPE

/* I am reserving values greater than or equal to 128 for user applications.
 * The first 127 are reserved for S-Lang.
 */

/* Binary and Unary Subtypes */
/* Since the application can define new types and can overload the binary
 * and unary operators, these definitions must be present in this file.
 * The current implementation assumes both unary and binary are distinct.
 */
#define SLANG_PLUS		0x01
#define SLANG_MINUS		0x02
#define SLANG_TIMES		0x03
#define SLANG_DIVIDE		0x04
#define SLANG_EQ		0x05
#define SLANG_NE		0x06
#define SLANG_GT		0x07
#define SLANG_GE		0x08
#define SLANG_LT		0x09
#define SLANG_LE		0x0A
#define SLANG_POW		0x0B
#define SLANG_OR		0x0C
#define SLANG_AND		0x0D
#define SLANG_BAND		0x0E
#define SLANG_BOR		0x0F
#define SLANG_BXOR		0x10
#define SLANG_SHL		0x11
#define SLANG_SHR		0x12
#define SLANG_MOD		0x13

/* UNARY subtypes  (may be overloaded) */
#define SLANG_PLUSPLUS		0x20
#define SLANG_MINUSMINUS	0x21
#define SLANG_ABS		0x22
#define SLANG_SIGN		0x23
#define SLANG_SQR		0x24
#define SLANG_MUL2		0x25
#define SLANG_CHS		0x26
#define SLANG_NOT		0x27
#define SLANG_BNOT		0x28


extern char *SLang_Error_Message;

int SLadd_intrinsic_variable (char *, VOID_STAR, unsigned char, int);
int SLadd_intrinsic_function (char *, FVOID_STAR, unsigned char, unsigned int,...);

extern void SLadd_at_handler (long *, char *);

#define MAKE_INTRINSIC_N(n,f,out,in,a1,a2,a3,a4,a5,a6,a7) \
    {(n), NULL, SLANG_INTRINSIC, (FVOID_STAR) (f), \
      {a1,a2,a3,a4,a5,a6,a7}, (in), (out)}

#define MAKE_INTRINSIC_7(n,f,out,a1,a2,a3,a4,a5,a6,a7) \
    MAKE_INTRINSIC_N(n,f,out,7,a1,a2,a3,a4,a5,a6,a7)
#define MAKE_INTRINSIC_6(n,f,out,a1,a2,a3,a4,a5,a6) \
    MAKE_INTRINSIC_N(n,f,out,6,a1,a2,a3,a4,a5,a6,0)
#define MAKE_INTRINSIC_5(n,f,out,a1,a2,a3,a4,a5) \
    MAKE_INTRINSIC_N(n,f,out,5,a1,a2,a3,a4,a5,0,0)
#define MAKE_INTRINSIC_4(n,f,out,a1,a2,a3,a4) \
    MAKE_INTRINSIC_N(n,f,out,4,a1,a2,a3,a4,0,0,0)
#define MAKE_INTRINSIC_3(n,f,out,a1,a2,a3) \
    MAKE_INTRINSIC_N(n,f,out,3,a1,a2,a3,0,0,0,0)
#define MAKE_INTRINSIC_2(n,f,out,a1,a2) \
    MAKE_INTRINSIC_N(n,f,out,2,a1,a2,0,0,0,0,0)
#define MAKE_INTRINSIC_1(n,f,out,a1) \
    MAKE_INTRINSIC_N(n,f,out,1,a1,0,0,0,0,0,0)
#define MAKE_INTRINSIC_0(n,f,out) \
    MAKE_INTRINSIC_N(n,f,out,0,0,0,0,0,0,0,0)



#define MAKE_INTRINSIC_S(n,f,r) \
   MAKE_INTRINSIC_1(n,f,r,SLANG_STRING_TYPE)
#define MAKE_INTRINSIC_I(n,f,r) \
   MAKE_INTRINSIC_1(n,f,r,SLANG_INT_TYPE)

#define MAKE_INTRINSIC_SS(n,f,r) \
   MAKE_INTRINSIC_2(n,f,r,SLANG_STRING_TYPE,SLANG_STRING_TYPE)
#define MAKE_INTRINSIC_SI(n,f,r) \
   MAKE_INTRINSIC_2(n,f,r,SLANG_STRING_TYPE,SLANG_INT_TYPE)
#define MAKE_INTRINSIC_IS(n,f,r) \
   MAKE_INTRINSIC_2(n,f,r,SLANG_INT_TYPE,SLANG_STRING_TYPE)
#define MAKE_INTRINSIC_II(n,f,r) \
   MAKE_INTRINSIC_2(n,f,r,SLANG_INT_TYPE,SLANG_INT_TYPE)

#define MAKE_INTRINSIC_SSS(n,f,r) \
   MAKE_INTRINSIC_3(n,f,r,SLANG_STRING_TYPE,SLANG_STRING_TYPE,SLANG_STRING_TYPE)
#define MAKE_INTRINSIC_SSI(n,f,r) \
   MAKE_INTRINSIC_3(n,f,r,SLANG_STRING_TYPE,SLANG_STRING_TYPE,SLANG_INT_TYPE)
#define MAKE_INTRINSIC_SIS(n,f,r) \
   MAKE_INTRINSIC_3(n,f,r,SLANG_STRING_TYPE,SLANG_INT_TYPE,SLANG_STRING_TYPE)
#define MAKE_INTRINSIC_SII(n,f,r) \
   MAKE_INTRINSIC_3(n,f,r,SLANG_STRING_TYPE,SLANG_INT_TYPE,SLANG_INT_TYPE)
#define MAKE_INTRINSIC_ISS(n,f,r) \
   MAKE_INTRINSIC_3(n,f,r,SLANG_INT_TYPE,SLANG_STRING_TYPE,SLANG_STRING_TYPE)
#define MAKE_INTRINSIC_ISI(n,f,r) \
   MAKE_INTRINSIC_3(n,f,r,SLANG_INT_TYPE,SLANG_STRING_TYPE,SLANG_INT_TYPE)
#define MAKE_INTRINSIC_IIS(n,f,r) \
   MAKE_INTRINSIC_3(n,f,r,SLANG_INT_TYPE,SLANG_INT_TYPE,SLANG_STRING_TYPE)
#define MAKE_INTRINSIC_III(n,f,r) \
   MAKE_INTRINSIC_3(n,f,r,SLANG_INT_TYPE,SLANG_INT_TYPE,SLANG_INT_TYPE)

#define MAKE_INTRINSIC(n, f, out, in) \
    MAKE_INTRINSIC_N(n,f,out,in,0,0,0,0,0,0,0)


#define MAKE_VARIABLE(n, v, t, r)     \
    {n, NULL, SLANG_IVARIABLE + (r), (VOID_STAR)(v), (t)}

#define MAKE_APP_UNARY(n,op) \
    {(n), NULL, SLANG_APP_UNARY, (op)}

#define MAKE_MATH_UNARY(n,op) \
    {(n), NULL, SLANG_MATH_UNARY, (op)}

#define SLANG_END_TABLE  {NULL}

/*}}}*/

/*{{{ Upper/Lowercase Functions */

extern void SLang_define_case(int *, int *);
extern void SLang_init_case_tables (void);

extern unsigned char _SLChg_UCase_Lut[256];
extern unsigned char _SLChg_LCase_Lut[256];
#define UPPER_CASE(x) (_SLChg_UCase_Lut[(unsigned char) (x)])
#define LOWER_CASE(x) (_SLChg_LCase_Lut[(unsigned char) (x)])
#define CHANGE_CASE(x) (((x) == _SLChg_LCase_Lut[(unsigned char) (x)]) ?\
			_SLChg_UCase_Lut[(unsigned char) (x)] : _SLChg_LCase_Lut[(unsigned char) (x)])

/*}}}*/

/*{{{ Regular Expression Interface */

typedef struct
{
   /* These must be set by calling routine. */
   unsigned char *pat;		       /* regular expression pattern */
   unsigned char *buf;		       /* buffer for compiled regexp */
   unsigned int buf_len;	       /* length of buffer */
   int case_sensitive;		       /* 1 if match is case sensitive  */

   /* The rest are set by SLang_regexp_compile */

   int must_match;		       /* 1 if line must contain substring */
   int must_match_bol;		       /* true if it must match beginning of line */
   unsigned char must_match_str[16];   /* 15 char null term substring */
   int osearch;			       /* 1 if ordinary search suffices */
   unsigned int min_length;	       /* minimum length the match must be */
   int beg_matches[10];		       /* offset of start of \( */
   unsigned int end_matches[10];       /* length of nth submatch
					* Note that the entire match corresponds
					* to \0
					*/
   int offset;			       /* offset to be added to beg_matches */
   int reserved[10];
} SLRegexp_Type;

extern unsigned char *SLang_regexp_match(unsigned char *,
					 unsigned int,
					 SLRegexp_Type *);

/* Returns 0 upon success.  If failure, the offset into the
 * pattern is returned (start = 1).
 */
extern int SLang_regexp_compile (SLRegexp_Type *);
extern char *SLregexp_quote_string (char *, char *, unsigned int);

/*}}}*/

/*{{{ SLang Command Interface */

#define SLCMD_MAX_ARGS 10
struct _SLcmd_Cmd_Type; /* Pre-declaration is needed below */
typedef struct
{
   struct _SLcmd_Cmd_Type *table;
   int argc;
   char *string_args[SLCMD_MAX_ARGS];
   int int_args[SLCMD_MAX_ARGS];
   double double_args[SLCMD_MAX_ARGS];
   unsigned char arg_type[SLCMD_MAX_ARGS];
} SLcmd_Cmd_Table_Type;

typedef struct _SLcmd_Cmd_Type
{
   int (*cmdfun)(int, SLcmd_Cmd_Table_Type *);
   char cmd[32];
   char arg_type[SLCMD_MAX_ARGS];
} SLcmd_Cmd_Type;

extern int SLcmd_execute_string (char *, SLcmd_Cmd_Table_Type *);

/*}}}*/

/*{{{ SLang Search Interface */

typedef struct
{
   int cs;			       /* case sensitive */
   unsigned char key[256];
   int ind[256];
   int key_len;
   int dir;
} SLsearch_Type;

extern int SLsearch_init (char *, int, int, SLsearch_Type *);
/* This routine must first be called before any search can take place.
 * The second parameter specifies the direction of the search: greater than
 * zero for a forwrd search and less than zero for a backward search.  The
 * third parameter specifies whether the search is case sensitive or not.
 * The last parameter is a pointer to a structure that is filled by this
 * function and it is this structure that must be passed to SLsearch.
 */

extern unsigned char *SLsearch (unsigned char *, unsigned char *, SLsearch_Type *);
/* To use this routine, you must first call 'SLsearch_init'.  Then the first
 * two parameters p1 and p2 serve to define the region over which the search
 * is to take place.  The third parameter is the structure that was previously
 * initialized by SLsearch_init.
 *
 * The routine returns a pointer to the match if found otherwise it returns
 * NULL.
 */

/*}}}*/

#if 0
{
#endif
#ifdef __cplusplus
}
#endif

#endif  /* _DAVIS_SLANG_H_ */
