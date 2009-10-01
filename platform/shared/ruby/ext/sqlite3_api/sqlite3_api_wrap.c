#include "sqlite3.h"
#include "ruby.h"

#include "logging/RhoLog.h"
#undef DEFAULT_LOGCATEGORY
#define DEFAULT_LOGCATEGORY "DB"

static VALUE mSqlite3;
static VALUE mDatabase;

extern int rho_sync_openDB(const char* szDBPath);
extern int rho_sync_closeDB();
extern int rho_db_startUITransaction();
extern int rho_db_commitUITransaction();
extern int rho_db_rollbackUITransaction();
extern int rho_db_destroy_table(const char* szTableName);
extern void* rho_db_get_handle();

static VALUE db_allocate(VALUE klass)
{
	//sqlite3 **db = malloc(sizeof(sqlite3 **));
	return Data_Wrap_Struct(klass, 0, 0, 0);
}

static VALUE db_init(int argc, VALUE *argv, VALUE self)
{
	const char *szDbName = NULL;
	sqlite3 **ppDB = NULL;
	int result;
	
	if ((argc < 1) || (argc > 1))
		rb_raise(rb_eArgError, "wrong # of arguments(%d for 1)",argc);
	
	Data_Get_Struct(self, sqlite3 *, ppDB);
	szDbName = STR2CSTR(argv[0]);
	
	result = (int)rho_sync_openDB(szDbName);//sqlite3_open(szDbName,ppDB);
	if ( result != SQLITE_OK )
		rb_raise(rb_eArgError, "could open database:%d",result);
	
	return self;
}

static VALUE db_close(int argc, VALUE *argv, VALUE self){
	//sqlite3 * db = NULL;
	sqlite3 **ppDB = NULL;		
	int rc = 0;
	
	if (argc > 0)
		rb_raise(rb_eArgError, "wrong # of arguments(%d for 0)",argc);
	
	Data_Get_Struct(self, sqlite3 *, ppDB);
	
	rc = rho_sync_closeDB();//sqlite3_close(db);
	
	return INT2NUM(rc);
}

static VALUE db_start_transaction(int argc, VALUE *argv, VALUE self){
	//sqlite3 * db = NULL;
	sqlite3 **ppDB = NULL;		
	int rc = 0;
	
	if (argc > 0)
		rb_raise(rb_eArgError, "wrong # of arguments(%d for 0)",argc);
	
	Data_Get_Struct(self, sqlite3 *, ppDB);
	
	rc = rho_db_startUITransaction();
	
	return INT2NUM(rc);
}

static VALUE db_commit(int argc, VALUE *argv, VALUE self){
	//sqlite3 * db = NULL;
	sqlite3 **ppDB = NULL;		
	int rc = 0;
	
	if (argc > 0)
		rb_raise(rb_eArgError, "wrong # of arguments(%d for 0)",argc);
	
	Data_Get_Struct(self, sqlite3 *, ppDB);
	
	rc = rho_db_commitUITransaction();
	
	return INT2NUM(rc);
}

static VALUE db_rollback(int argc, VALUE *argv, VALUE self){
	//sqlite3 * db = NULL;
	sqlite3 **ppDB = NULL;		
	int rc = 0;
	
	if (argc > 0)
		rb_raise(rb_eArgError, "wrong # of arguments(%d for 0)",argc);
	
	Data_Get_Struct(self, sqlite3 *, ppDB);
	
	rc = rho_db_rollbackUITransaction();
	
	return INT2NUM(rc);
}

static VALUE* getColNames(sqlite3_stmt* statement, int nCount)
{
    int nCol = 0;
    VALUE* res = malloc(sizeof(VALUE)*nCount);

	for(;nCol<nCount;nCol++)
    {
		const char* szColName = sqlite3_column_name(statement,nCol);
		res[nCol] = rb_str_new2(szColName);
    }

    return res;
}

static VALUE db_destroy_table(int argc, VALUE *argv, VALUE self)
{
	//sqlite3 * db = NULL;
	sqlite3 **ppDB = NULL;		
    const char* szTableName = NULL;
    int rc = 0;

	if ((argc < 1) || (argc > 1))
		rb_raise(rb_eArgError, "wrong # of arguments(%d for 1)",argc);

	Data_Get_Struct(self, sqlite3 *, ppDB);
	szTableName = RSTRING_PTR(argv[0]);

    rc = rho_db_destroy_table(szTableName);

    return INT2NUM(rc);
}

static VALUE db_execute(int argc, VALUE *argv, VALUE self)
{
	sqlite3 * db = NULL;
	sqlite3 **ppDB = NULL;		
	sqlite3_stmt *statement = NULL;
	const char* sql = NULL;
	VALUE arRes = rb_ary_new();
    VALUE* colNames = NULL;
	int nRes = 0;
    char * szErrMsg = 0;
	
	if ((argc < 1) || (argc > 2))
		rb_raise(rb_eArgError, "wrong # of arguments(%d for 2)",argc);
	
	Data_Get_Struct(self, sqlite3 *, ppDB);
	db = (sqlite3 *)rho_db_get_handle();
	sql = RSTRING_PTR(argv[0]);
	
    RAWTRACE1("db_execute: %s", sql);
	if ( (nRes = sqlite3_prepare_v2(db, sql, -1, &statement, NULL)) != SQLITE_OK)
    {
        szErrMsg = (char *)sqlite3_errmsg(db);

        rb_raise(rb_eArgError, "could not prepare statement: %d; Message: %s",nRes, (szErrMsg?szErrMsg:""));
    }

    if ( argc > 1 )
    {
        int i = 0;
        VALUE args = argv[1];
        if ( RARRAY_LEN(args) > 0 && TYPE(RARRAY_PTR(args)[0]) == T_ARRAY )
            args = RARRAY_PTR(args)[0];

        for( ; i < RARRAY_LEN(args); i++ )
        {
            VALUE arg = RARRAY_PTR(args)[i];
            if (NIL_P(arg))
            {
                sqlite3_bind_null(statement, i+1);
                continue;
            }

            switch( TYPE(arg) )
            {
            case T_STRING:
                sqlite3_bind_text(statement, i+1, RSTRING_PTR(arg), -1, SQLITE_TRANSIENT);
                break;
            case T_FIXNUM:
            case T_FLOAT:
            case T_BIGNUM:
                sqlite3_bind_int64(statement, i+1, NUM2ULONG(arg));
                break;
            }
        }
    }

	while( (nRes=sqlite3_step(statement)) == SQLITE_ROW) {
		int nCount = sqlite3_data_count(statement);
		int nCol = 0;
		VALUE hashRec = rb_hash_new();

        //if ( !colNames )
        //    colNames = getColNames(statement, nCount);

		for(;nCol<nCount;nCol++){
			int nColType = sqlite3_column_type(statement,nCol);
			const char* szColName = sqlite3_column_name(statement,nCol);
			VALUE colName = rb_str_new2(szColName);
			VALUE colValue = Qnil;
			
			switch(nColType){
				case SQLITE_NULL:
					break;
                case SQLITE_INTEGER:
                {
                    int nVal = sqlite3_column_int(statement, nCol);
                    colValue = INT2NUM(nVal);
                    break;
                }
				default:{
					char *text = (char *)sqlite3_column_text(statement, nCol);
					colValue = rb_str_new2(text);
					break;
				}
			}
			
			rb_hash_aset(hashRec, colName/*colNames[nCol]*/, colValue);
		}
		
		rb_ary_push(arRes, hashRec);
	}
	
    if ( nRes != SQLITE_OK && nRes != SQLITE_ROW && nRes != SQLITE_DONE )
    {
        szErrMsg = (char*)sqlite3_errmsg(db);

        rb_raise(rb_eArgError, "could not execute statement: %d; Message: %s",nRes, (szErrMsg?szErrMsg:""));
    }

	sqlite3_finalize(statement);
	
    if ( colNames )
        free(colNames);

	return arRes;
}

void Init_sqlite3_api(void)
{
	mSqlite3 = rb_define_module("SQLite3");
	mDatabase = rb_define_class_under(mSqlite3, "Database", rb_cObject);
	
	rb_define_alloc_func(mDatabase, db_allocate);
	rb_define_method(mDatabase, "initialize", db_init, -1);
	rb_define_method(mDatabase, "close", db_close, -1);
	rb_define_method(mDatabase, "execute", db_execute, -1);	
	rb_define_method(mDatabase, "start_transaction", db_start_transaction, -1);	
	rb_define_method(mDatabase, "commit", db_commit, -1);	
    rb_define_method(mDatabase, "rollback", db_rollback, -1);	
    rb_define_method(mDatabase, "destroy_table", db_destroy_table, -1);	
}

#if 0

/* ----------------------------------------------------------------------------
 * This file was automatically generated by SWIG (http://www.swig.org).
 * Version 1.3.27
 * 
 * This file is not intended to be easily readable and contains a number of 
 * coding conventions designed to improve portability and efficiency. Do not make
 * changes to this file unless you know what you are doing--modify the SWIG 
 * interface file instead. 
 * ----------------------------------------------------------------------------- */

/***********************************************************************
 *
 *  This section contains generic SWIG labels for method/variable
 *  declarations/attributes, and other compiler dependent labels.
 *
 ************************************************************************/

/* template workaround for compilers that cannot correctly implement the C++ standard */
#ifndef SWIGTEMPLATEDISAMBIGUATOR
#  if defined(__SUNPRO_CC) && (__SUNPRO_CC <= 0x560)
#    define SWIGTEMPLATEDISAMBIGUATOR template
#  else
#    define SWIGTEMPLATEDISAMBIGUATOR 
#  endif
#endif

/* inline attribute */
#ifndef SWIGINLINE
# if defined(__cplusplus) || (defined(__GNUC__) && !defined(__STRICT_ANSI__))
#   define SWIGINLINE inline
# else
#   define SWIGINLINE
# endif
#endif

/* attribute recognised by some compilers to avoid 'unused' warnings */
#ifndef SWIGUNUSED
# if defined(__GNUC__) || defined(__ICC)
#   define SWIGUNUSED __attribute__ ((unused)) 
# else
#   define SWIGUNUSED 
# endif
#endif

/* internal SWIG method */
#ifndef SWIGINTERN
# define SWIGINTERN static SWIGUNUSED
#endif

/* internal inline SWIG method */
#ifndef SWIGINTERNINLINE
# define SWIGINTERNINLINE SWIGINTERN SWIGINLINE
#endif

/* exporting methods for Windows DLLs */
#ifndef SWIGEXPORT
# if defined(_WIN32) || defined(__WIN32__) || defined(__CYGWIN__)
#   if defined(STATIC_LINKED)
#     define SWIGEXPORT
#   else
#     define SWIGEXPORT __declspec(dllexport)
#   endif
# else
#   define SWIGEXPORT
# endif
#endif

/* calling conventions for Windows */
#ifndef SWIGSTDCALL
# if defined(_WIN32) || defined(__WIN32__) || defined(__CYGWIN__)
#   define SWIGSTDCALL __stdcall
# else
#   define SWIGSTDCALL
# endif 
#endif


/* ruby.swg */
/* Implementation : RUBY */
#define SWIGRUBY 1

#include "ruby.h"

/* Flags for pointer conversion */
#define SWIG_POINTER_EXCEPTION     0x1
#define SWIG_POINTER_OWN           0x1
#define SWIG_POINTER_DISOWN        0x2
#define SWIG_TRACK_OBJECTS	        0x4

#define NUM2USHRT(n) (\
    (0 <= NUM2UINT(n) && NUM2UINT(n) <= USHRT_MAX)\
    ? (unsigned short) NUM2UINT(n) \
    : (rb_raise(rb_eArgError, "integer %d out of range of `unsigned short'",\
               NUM2UINT(n)), (short)0)\
)

#define NUM2SHRT(n) (\
    (SHRT_MIN <= NUM2INT(n) && NUM2INT(n) <= SHRT_MAX)\
    ? (short)NUM2INT(n)\
    : (rb_raise(rb_eArgError, "integer %d out of range of `short'",\
               NUM2INT(n)), (short)0)\
)

/* Ruby 1.7 defines NUM2LL(), LL2NUM() and ULL2NUM() macros */
#ifndef NUM2LL
#define NUM2LL(x) NUM2LONG((x))
#endif
#ifndef LL2NUM
#define LL2NUM(x) INT2NUM((long) (x))
#endif
#ifndef ULL2NUM
#define ULL2NUM(x) UINT2NUM((unsigned long) (x))
#endif

/* Ruby 1.7 doesn't (yet) define NUM2ULL() */
#ifndef NUM2ULL
#ifdef HAVE_LONG_LONG
#define NUM2ULL(x) rb_num2ull((x))
#else
#define NUM2ULL(x) NUM2ULONG(x)
#endif
#endif

/*
 * Need to be very careful about how these macros are defined, especially
 * when compiling C++ code or C code with an ANSI C compiler.
 *
 * VALUEFUNC(f) is a macro used to typecast a C function that implements
 * a Ruby method so that it can be passed as an argument to API functions
 * like rb_define_method() and rb_define_singleton_method().
 *
 * VOIDFUNC(f) is a macro used to typecast a C function that implements
 * either the "mark" or "free" stuff for a Ruby Data object, so that it
 * can be passed as an argument to API functions like Data_Wrap_Struct()
 * and Data_Make_Struct().
 */
 
#ifdef __cplusplus
#  ifndef RUBY_METHOD_FUNC /* These definitions should work for Ruby 1.4.6 */
#    define PROTECTFUNC(f) ((VALUE (*)()) f)
#    define VALUEFUNC(f) ((VALUE (*)()) f)
#    define VOIDFUNC(f)  ((void (*)()) f)
#  else
#    ifndef ANYARGS /* These definitions should work for Ruby 1.6 */
#      define PROTECTFUNC(f) ((VALUE (*)()) f)
#      define VALUEFUNC(f) ((VALUE (*)()) f)
#      define VOIDFUNC(f)  ((RUBY_DATA_FUNC) f)
#    else /* These definitions should work for Ruby 1.7+ */
#      define PROTECTFUNC(f) ((VALUE (*)(VALUE)) f)
#      define VALUEFUNC(f) ((VALUE (*)(ANYARGS)) f)
#      define VOIDFUNC(f)  ((RUBY_DATA_FUNC) f)
#    endif
#  endif
#else
#  define VALUEFUNC(f) (f)
#  define VOIDFUNC(f) (f)
#endif

typedef struct {
  VALUE klass;
  VALUE mImpl;
  void  (*mark)(void *);
  void  (*destroy)(void *);
} swig_class;

/* Don't use for expressions have side effect */
#ifndef RB_STRING_VALUE
#define RB_STRING_VALUE(s) (TYPE(s) == T_STRING ? (s) : (*(volatile VALUE *)&(s) = rb_str_to_str(s)))
#endif
#ifndef StringValue
#define StringValue(s) RB_STRING_VALUE(s)
#endif
#ifndef StringValuePtr
#define StringValuePtr(s) RSTRING(RB_STRING_VALUE(s))->ptr
#endif
#ifndef StringValueLen
#define StringValueLen(s) RSTRING(RB_STRING_VALUE(s))->len
#endif
#ifndef SafeStringValue
#define SafeStringValue(v) do {\
    StringValue(v);\
    rb_check_safe_str(v);\
} while (0)
#endif

#ifndef HAVE_RB_DEFINE_ALLOC_FUNC
#define rb_define_alloc_func(klass, func) rb_define_singleton_method((klass), "new", VALUEFUNC((func)), -1)
#define rb_undef_alloc_func(klass) rb_undef_method(CLASS_OF((klass)), "new")
#endif

/* Contract support */

#define SWIG_contract_assert(expr, msg) if (!(expr)) { rb_raise(rb_eRuntimeError, (char *) msg ); } else


/***********************************************************************
 * swigrun.swg
 *
 *     This file contains generic CAPI SWIG runtime support for pointer
 *     type checking.
 *
 ************************************************************************/

/* This should only be incremented when either the layout of swig_type_info changes,
   or for whatever reason, the runtime changes incompatibly */
#define SWIG_RUNTIME_VERSION "2"

/* define SWIG_TYPE_TABLE_NAME as "SWIG_TYPE_TABLE" */
#ifdef SWIG_TYPE_TABLE
# define SWIG_QUOTE_STRING(x) #x
# define SWIG_EXPAND_AND_QUOTE_STRING(x) SWIG_QUOTE_STRING(x)
# define SWIG_TYPE_TABLE_NAME SWIG_EXPAND_AND_QUOTE_STRING(SWIG_TYPE_TABLE)
#else
# define SWIG_TYPE_TABLE_NAME
#endif

/*
  You can use the SWIGRUNTIME and SWIGRUNTIMEINLINE macros for
  creating a static or dynamic library from the swig runtime code.
  In 99.9% of the cases, swig just needs to declare them as 'static'.
  
  But only do this if is strictly necessary, ie, if you have problems
  with your compiler or so.
*/

#ifndef SWIGRUNTIME
# define SWIGRUNTIME SWIGINTERN
#endif

#ifndef SWIGRUNTIMEINLINE
# define SWIGRUNTIMEINLINE SWIGRUNTIME SWIGINLINE
#endif

#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void *(*swig_converter_func)(void *);
typedef struct swig_type_info *(*swig_dycast_func)(void **);

/* Structure to store inforomation on one type */
typedef struct swig_type_info {
  const char             *name;			/* mangled name of this type */
  const char             *str;			/* human readable name of this type */
  swig_dycast_func        dcast;		/* dynamic cast function down a hierarchy */
  struct swig_cast_info  *cast;			/* linked list of types that can cast into this type */
  void                   *clientdata;		/* language specific type data */
} swig_type_info;

/* Structure to store a type and conversion function used for casting */
typedef struct swig_cast_info {
  swig_type_info         *type;			/* pointer to type that is equivalent to this type */
  swig_converter_func     converter;		/* function to cast the void pointers */
  struct swig_cast_info  *next;			/* pointer to next cast in linked list */
  struct swig_cast_info  *prev;			/* pointer to the previous cast */
} swig_cast_info;

/* Structure used to store module information
 * Each module generates one structure like this, and the runtime collects
 * all of these structures and stores them in a circularly linked list.*/
typedef struct swig_module_info {
  swig_type_info         **types;		/* Array of pointers to swig_type_info structures that are in this module */
  size_t                 size;		        /* Number of types in this module */
  struct swig_module_info *next;		/* Pointer to next element in circularly linked list */
  swig_type_info         **type_initial;	/* Array of initially generated type structures */
  swig_cast_info         **cast_initial;	/* Array of initially generated casting structures */
  void                    *clientdata;		/* Language specific module data */
} swig_module_info;


/* 
  Compare two type names skipping the space characters, therefore
  "char*" == "char *" and "Class<int>" == "Class<int >", etc.

  Return 0 when the two name types are equivalent, as in
  strncmp, but skipping ' '.
*/
SWIGRUNTIME int
SWIG_TypeNameComp(const char *f1, const char *l1,
		  const char *f2, const char *l2) {
  for (;(f1 != l1) && (f2 != l2); ++f1, ++f2) {
    while ((*f1 == ' ') && (f1 != l1)) ++f1;
    while ((*f2 == ' ') && (f2 != l2)) ++f2;
    if (*f1 != *f2) return (int)(*f1 - *f2);
  }
  return (l1 - f1) - (l2 - f2);
}

/*
  Check type equivalence in a name list like <name1>|<name2>|...
  Return 0 if not equal, 1 if equal
*/
SWIGRUNTIME int
SWIG_TypeEquiv(const char *nb, const char *tb) {
  int equiv = 0;
  const char* te = tb + strlen(tb);
  const char* ne = nb;
  while (!equiv && *ne) {
    for (nb = ne; *ne; ++ne) {
      if (*ne == '|') break;
    }
    equiv = (SWIG_TypeNameComp(nb, ne, tb, te) == 0) ? 1 : 0;
    if (*ne) ++ne;
  }
  return equiv;
}

/*
  Check type equivalence in a name list like <name1>|<name2>|...
  Return 0 if equal, -1 if nb < tb, 1 if nb > tb
*/
SWIGRUNTIME int
SWIG_TypeCompare(const char *nb, const char *tb) {
  int equiv = 0;
  const char* te = tb + strlen(tb);
  const char* ne = nb;
  while (!equiv && *ne) {
    for (nb = ne; *ne; ++ne) {
      if (*ne == '|') break;
    }
    equiv = (SWIG_TypeNameComp(nb, ne, tb, te) == 0) ? 1 : 0;
    if (*ne) ++ne;
  }
  return equiv;
}


/* think of this as a c++ template<> or a scheme macro */
#define SWIG_TypeCheck_Template(comparison, ty)         \
  if (ty) {                                             \
    swig_cast_info *iter = ty->cast;                    \
    while (iter) {                                      \
      if (comparison) {                                 \
        if (iter == ty->cast) return iter;              \
        /* Move iter to the top of the linked list */   \
        iter->prev->next = iter->next;                  \
        if (iter->next)                                 \
          iter->next->prev = iter->prev;                \
        iter->next = ty->cast;                          \
        iter->prev = 0;                                 \
        if (ty->cast) ty->cast->prev = iter;            \
        ty->cast = iter;                                \
        return iter;                                    \
      }                                                 \
      iter = iter->next;                                \
    }                                                   \
  }                                                     \
  return 0

/*
  Check the typename
*/
SWIGRUNTIME swig_cast_info *
SWIG_TypeCheck(const char *c, swig_type_info *ty) {
  SWIG_TypeCheck_Template(strcmp(iter->type->name, c) == 0, ty);
}

/* Same as previous function, except strcmp is replaced with a pointer comparison */
SWIGRUNTIME swig_cast_info *
SWIG_TypeCheckStruct(swig_type_info *from, swig_type_info *into) {
  SWIG_TypeCheck_Template(iter->type == from, into);
}

/*
  Cast a pointer up an inheritance hierarchy
*/
SWIGRUNTIMEINLINE void *
SWIG_TypeCast(swig_cast_info *ty, void *ptr) {
  return ((!ty) || (!ty->converter)) ? ptr : (*ty->converter)(ptr);
}

/* 
   Dynamic pointer casting. Down an inheritance hierarchy
*/
SWIGRUNTIME swig_type_info *
SWIG_TypeDynamicCast(swig_type_info *ty, void **ptr) {
  swig_type_info *lastty = ty;
  if (!ty || !ty->dcast) return ty;
  while (ty && (ty->dcast)) {
    ty = (*ty->dcast)(ptr);
    if (ty) lastty = ty;
  }
  return lastty;
}

/*
  Return the name associated with this type
*/
SWIGRUNTIMEINLINE const char *
SWIG_TypeName(const swig_type_info *ty) {
  return ty->name;
}

/*
  Return the pretty name associated with this type,
  that is an unmangled type name in a form presentable to the user.
*/
SWIGRUNTIME const char *
SWIG_TypePrettyName(const swig_type_info *type) {
  /* The "str" field contains the equivalent pretty names of the
     type, separated by vertical-bar characters.  We choose
     to print the last name, as it is often (?) the most
     specific. */
  if (type->str != NULL) {
    const char *last_name = type->str;
    const char *s;
    for (s = type->str; *s; s++)
      if (*s == '|') last_name = s+1;
    return last_name;
  }
  else
    return type->name;
}

/* 
   Set the clientdata field for a type
*/
SWIGRUNTIME void
SWIG_TypeClientData(swig_type_info *ti, void *clientdata) {
  swig_cast_info *cast = ti->cast;
  /* if (ti->clientdata == clientdata) return; */
  ti->clientdata = clientdata;
  
  while (cast) {
    if (!cast->converter) {
      swig_type_info *tc = cast->type;
      if (!tc->clientdata) {
	SWIG_TypeClientData(tc, clientdata);
      }
    }    
    cast = cast->next;
  }
}

/*
  Search for a swig_type_info structure only by mangled name
  Search is a O(log #types)
  
  We start searching at module start, and finish searching when start == end.  
  Note: if start == end at the beginning of the function, we go all the way around
  the circular list.
*/
SWIGRUNTIME swig_type_info *
SWIG_MangledTypeQueryModule(swig_module_info *start, 
                            swig_module_info *end, 
		            const char *name) {
  swig_module_info *iter = start;
  do {
    if (iter->size) {
      register size_t l = 0;
      register size_t r = iter->size - 1;
      do {
	/* since l+r >= 0, we can (>> 1) instead (/ 2) */
	register size_t i = (l + r) >> 1; 
	const char *iname = iter->types[i]->name;
	if (iname) {
	  register int compare = strcmp(name, iname);
	  if (compare == 0) {	    
	    return iter->types[i];
	  } else if (compare < 0) {
	    if (i) {
	      r = i - 1;
	    } else {
	      break;
	    }
	  } else if (compare > 0) {
	    l = i + 1;
	  }
	} else {
	  break; /* should never happen */
	}
      } while (l <= r);
    }
    iter = iter->next;
  } while (iter != end);
  return 0;
}

/*
  Search for a swig_type_info structure for either a mangled name or a human readable name.
  It first searches the mangled names of the types, which is a O(log #types)
  If a type is not found it then searches the human readable names, which is O(#types).
  
  We start searching at module start, and finish searching when start == end.  
  Note: if start == end at the beginning of the function, we go all the way around
  the circular list.
*/
SWIGRUNTIME swig_type_info *
SWIG_TypeQueryModule(swig_module_info *start, 
                     swig_module_info *end, 
		     const char *name) {
  /* STEP 1: Search the name field using binary search */
  swig_type_info *ret = SWIG_MangledTypeQueryModule(start, end, name);
  if (ret) {
    return ret;
  } else {
    /* STEP 2: If the type hasn't been found, do a complete search
       of the str field (the human readable name) */
    swig_module_info *iter = start;
    do {
      register size_t i = 0;
      for (; i < iter->size; ++i) {
	if (iter->types[i]->str && (SWIG_TypeEquiv(iter->types[i]->str, name)))
	  return iter->types[i];
      }
      iter = iter->next;
    } while (iter != end);
  }
  
  /* neither found a match */
  return 0;
}


/* 
   Pack binary data into a string
*/
SWIGRUNTIME char *
SWIG_PackData(char *c, void *ptr, size_t sz) {
  static const char hex[17] = "0123456789abcdef";
  register const unsigned char *u = (unsigned char *) ptr;
  register const unsigned char *eu =  u + sz;
  for (; u != eu; ++u) {
    register unsigned char uu = *u;
    *(c++) = hex[(uu & 0xf0) >> 4];
    *(c++) = hex[uu & 0xf];
  }
  return c;
}

/* 
   Unpack binary data from a string
*/
SWIGRUNTIME const char *
SWIG_UnpackData(const char *c, void *ptr, size_t sz) {
  register unsigned char *u = (unsigned char *) ptr;
  register const unsigned char *eu = u + sz;
  for (; u != eu; ++u) {
    register char d = *(c++);
    register unsigned char uu = 0;
    if ((d >= '0') && (d <= '9'))
      uu = ((d - '0') << 4);
    else if ((d >= 'a') && (d <= 'f'))
      uu = ((d - ('a'-10)) << 4);
    else 
      return (char *) 0;
    d = *(c++);
    if ((d >= '0') && (d <= '9'))
      uu |= (d - '0');
    else if ((d >= 'a') && (d <= 'f'))
      uu |= (d - ('a'-10));
    else 
      return (char *) 0;
    *u = uu;
  }
  return c;
}

/* 
   Pack 'void *' into a string buffer.
*/
SWIGRUNTIME char *
SWIG_PackVoidPtr(char *buff, void *ptr, const char *name, size_t bsz) {
  char *r = buff;
  if ((2*sizeof(void *) + 2) > bsz) return 0;
  *(r++) = '_';
  r = SWIG_PackData(r,&ptr,sizeof(void *));
  if (strlen(name) + 1 > (bsz - (r - buff))) return 0;
  strcpy(r,name);
  return buff;
}

SWIGRUNTIME const char *
SWIG_UnpackVoidPtr(const char *c, void **ptr, const char *name) {
  if (*c != '_') {
    if (strcmp(c,"NULL") == 0) {
      *ptr = (void *) 0;
      return name;
    } else {
      return 0;
    }
  }
  return SWIG_UnpackData(++c,ptr,sizeof(void *));
}

SWIGRUNTIME char *
SWIG_PackDataName(char *buff, void *ptr, size_t sz, const char *name, size_t bsz) {
  char *r = buff;
  size_t lname = (name ? strlen(name) : 0);
  if ((2*sz + 2 + lname) > bsz) return 0;
  *(r++) = '_';
  r = SWIG_PackData(r,ptr,sz);
  if (lname) {
    strncpy(r,name,lname+1);
  } else {
    *r = 0;
  }
  return buff;
}

SWIGRUNTIME const char *
SWIG_UnpackDataName(const char *c, void *ptr, size_t sz, const char *name) {
  if (*c != '_') {
    if (strcmp(c,"NULL") == 0) {
      memset(ptr,0,sz);
      return name;
    } else {
      return 0;
    }
  }
  return SWIG_UnpackData(++c,ptr,sz);
}

#ifdef __cplusplus
}
#endif

/***********************************************************************
 * rubytracking.swg
 *
 * This file contains support for tracking mappings from 
 * Ruby objects to C++ objects.  This functionality is needed
 * to implement mark functions for Ruby's mark and sweep
 * garbage collector.
 ************************************************************************/

/* Global Ruby hash table to store Trackings from C/C++
   structs to Ruby Objects. */
static VALUE swig_ruby_trackings;

/* Setup a Ruby hash table to store Trackings */
static void SWIG_RubyInitializeTrackings() {
	/* Create a ruby hash table to store Trackings from C++ 
	objects to Ruby objects.  Also make sure to tell
	the garabage collector about the hash table. */
	swig_ruby_trackings = rb_hash_new();
	rb_gc_register_address(&swig_ruby_trackings);
}

/* Get a Ruby number to reference a pointer */
static VALUE SWIG_RubyPtrToReference(void* ptr) {
	/* We cast the pointer to an unsigned long
	 and then store a reference to it using
	 a Ruby number object. */

	/* Convert the pointer to a Ruby number */
	unsigned long value = (unsigned long) ptr;
	return LONG2NUM(value);
}

/* Get a Ruby number to reference an object */
static VALUE SWIG_RubyObjectToReference(VALUE object) {
	/* We cast the object to an unsigned long
	 and then store a reference to it using
	 a Ruby number object. */

	/* Convert the Object to a Ruby number */
	unsigned long value = (unsigned long) object;
	return LONG2NUM(value);
}

/* Get a Ruby object from a previously stored reference */
static VALUE SWIG_RubyReferenceToObject(VALUE reference) {
	/* The provided Ruby number object is a reference
	to the Ruby object we want.*/

	/* First convert the Ruby number to a C number */
	unsigned long value = NUM2LONG(reference);
	return (VALUE) value;
}

/* Add a Tracking from a C/C++ struct to a Ruby object */
static void SWIG_RubyAddTracking(void* ptr, VALUE object) {
	/* In a Ruby hash table we store the pointer and
	the associated Ruby object.  The trick here is
	that we cannot store the Ruby object directly - if
	we do then it cannot be garbage collected.  So
	instead we typecast it as a unsigned long and
	convert it to a Ruby number object.*/

	/* Get a reference to the pointer as a Ruby number */
	VALUE key = SWIG_RubyPtrToReference(ptr);

	/* Get a reference to the Ruby object as a Ruby number */
	VALUE value = SWIG_RubyObjectToReference(object);

  /* Store the mapping to the global hash table. */
	rb_hash_aset(swig_ruby_trackings, key, value);
}

/* Get the Ruby object that owns the specified C/C++ struct */
static VALUE SWIG_RubyInstanceFor(void* ptr) {
	/* Get a reference to the pointer as a Ruby number */
	VALUE key = SWIG_RubyPtrToReference(ptr);

	/* Now lookup the value stored in the global hash table */
	VALUE value = rb_hash_aref(swig_ruby_trackings, key);
	
	if (value == Qnil) {
	  /* No object exists - return nil. */
		return Qnil;
	}
	else {
		/* Convert this value to Ruby object */
		return SWIG_RubyReferenceToObject(value);
	}
}

/* Remove a Tracking from a C/C++ struct to a Ruby object */
static void SWIG_RubyRemoveTracking(void* ptr) {
	/* Get a reference to the pointer as a Ruby number */
	VALUE key = SWIG_RubyPtrToReference(ptr);

	/* Define delete method - in C++ this could be marked as
	   static but unfortunately not in C. */
	VALUE delete_function = rb_intern("delete");

	/* Delete the object from the hash table by calling Ruby's
	   do this we need to call the Hash.delete method.*/
	rb_funcall(swig_ruby_trackings, delete_function, 1, key);
}

/* This is a helper method that unlinks a Ruby object from its
   underlying C++ object.  This is needed if the lifetime of the
   Ruby object is longer than the C++ object */
static void SWIG_RubyUnlinkObjects(void* ptr) {
	VALUE object = SWIG_RubyInstanceFor(ptr);

	if (object != Qnil) {
		DATA_PTR(object) = 0;
	}
}

/* Common SWIG API */
#define SWIG_ConvertPtr(obj, pp, type, flags) \
  SWIG_Ruby_ConvertPtr(obj, pp, type, flags)
#define SWIG_NewPointerObj(p, type, flags) \
  SWIG_Ruby_NewPointerObj(p, type, flags)
#define SWIG_MustGetPtr(p, type, argnum, flags) \
  SWIG_Ruby_MustGetPtr(p, type, argnum, flags)
#define SWIG_GetModule(clientdata) \
  SWIG_Ruby_GetModule()
#define SWIG_SetModule(clientdata, pointer) \
  SWIG_Ruby_SetModule(pointer)

/* Ruby-specific SWIG API */

#define SWIG_InitRuntime() \
  SWIG_Ruby_InitRuntime()
#define SWIG_define_class(ty) \
  SWIG_Ruby_define_class(ty)
#define SWIG_NewClassInstance(value, ty) \
  SWIG_Ruby_NewClassInstance(value, ty)
#define SWIG_MangleStr(value) \
  SWIG_Ruby_MangleStr(value)
#define SWIG_CheckConvert(value, ty) \
  SWIG_Ruby_CheckConvert(value, ty)
#define SWIG_NewPackedObj(ptr, sz, ty) \
  SWIG_Ruby_NewPackedObj(ptr, sz, ty)
#define SWIG_ConvertPacked(obj, ptr, sz, ty, flags) \
  SWIG_Ruby_ConvertPacked(obj, ptr, sz, ty, flags)

/* rubydef.swg */
#ifdef __cplusplus
extern "C" {
#endif

static VALUE _mSWIG = Qnil;
static VALUE _cSWIG_Pointer = Qnil;
static VALUE swig_runtime_data_type_pointer = Qnil;

/* Initialize Ruby runtime support */
static void
SWIG_Ruby_InitRuntime(void)
{
    if (_mSWIG == Qnil) {
        _mSWIG = rb_define_module("SWIG");
    }
}

/* Define Ruby class for C type */
static void
SWIG_Ruby_define_class(swig_type_info *type)
{
    VALUE klass;
    char *klass_name = (char *) malloc(4 + strlen(type->name) + 1);
    sprintf(klass_name, "TYPE%s", type->name);
    if (NIL_P(_cSWIG_Pointer)) {
	_cSWIG_Pointer = rb_define_class_under(_mSWIG, "Pointer", rb_cObject);
	rb_undef_method(CLASS_OF(_cSWIG_Pointer), "new");
    }
    klass = rb_define_class_under(_mSWIG, klass_name, _cSWIG_Pointer);
    free((void *) klass_name);
}

/* Create a new pointer object */
static VALUE
SWIG_Ruby_NewPointerObj(void *ptr, swig_type_info *type, int flags)
{
	 int own = flags & SWIG_POINTER_OWN;
	 int track = flags & SWIG_TRACK_OBJECTS;
	 
    char *klass_name;
    swig_class *sklass;
    VALUE klass;
    VALUE obj;
    
    if (!ptr)
      return Qnil;
    
    /* Have we already wrapped this pointer? */
    if (track) {
	    obj = SWIG_RubyInstanceFor(ptr);
   	 if (obj != Qnil) {
      	return obj;
    	}
    }
		
    if (type->clientdata) {
      sklass = (swig_class *) type->clientdata;
      obj = Data_Wrap_Struct(sklass->klass, VOIDFUNC(sklass->mark), (own ? VOIDFUNC(sklass->destroy) : 0), ptr);
    } else {
      klass_name = (char *) malloc(4 + strlen(type->name) + 1);
      sprintf(klass_name, "TYPE%s", type->name);
      klass = rb_const_get(_mSWIG, rb_intern(klass_name));
      free((void *) klass_name);
      obj = Data_Wrap_Struct(klass, 0, 0, ptr);
    }
    rb_iv_set(obj, "__swigtype__", rb_str_new2(type->name));
    
    /* Keep track of this object if necessary */
    if (track) {
	   SWIG_RubyAddTracking(ptr, obj);
    }
   
    return obj;
}

/* Create a new class instance (always owned) */
static VALUE
SWIG_Ruby_NewClassInstance(VALUE klass, swig_type_info *type)
{
    VALUE obj;
    swig_class *sklass = (swig_class *) type->clientdata;
    obj = Data_Wrap_Struct(klass, VOIDFUNC(sklass->mark), VOIDFUNC(sklass->destroy), 0);
    rb_iv_set(obj, "__swigtype__", rb_str_new2(type->name));
    return obj;
}

/* Get type mangle from class name */
static SWIGINLINE char *
SWIG_Ruby_MangleStr(VALUE obj)
{
  VALUE stype = rb_iv_get(obj, "__swigtype__");
  return StringValuePtr(stype);
}

/* Convert a pointer value */
static int
SWIG_Ruby_ConvertPtr(VALUE obj, void **ptr, swig_type_info *ty, int flags)
{
  char *c;
  swig_cast_info *tc;

  /* Grab the pointer */
  if (NIL_P(obj)) {
    *ptr = 0;
    return 0;
  } else {
    Data_Get_Struct(obj, void, *ptr);
  }
  
  /* Check to see if the input object is giving up ownership
     of the underlying C struct or C++ object.  If so then we
     need to reset the destructor since the Ruby object no 
     longer owns the underlying C++ object.*/ 
  if (flags & SWIG_POINTER_DISOWN) {
	 if (flags & SWIG_TRACK_OBJECTS) {
      /* We are tracking objects.  Thus we change the destructor
		 * to SWIG_RubyRemoveTracking.  This allows us to
		 * remove the mapping from the C++ to Ruby object
		 * when the Ruby object is garbage collected.  If we don't
		 * do this, then it is possible we will return a reference 
		 * to a Ruby object that no longer exists thereby crashing Ruby. */
		RDATA(obj)->dfree = SWIG_RubyRemoveTracking;
  	 } else {    
      RDATA(obj)->dfree = 0;
    }
  }

  /* Do type-checking if type info was provided */
  if (ty) {
    if (ty->clientdata) {
        if (rb_obj_is_kind_of(obj, ((swig_class *) (ty->clientdata))->klass)) {
          if (*ptr == 0)
            rb_raise(rb_eRuntimeError, "This %s already released", ty->str);
          return 0;
        }
    }
    if ((c = SWIG_MangleStr(obj)) == NULL) {
      if (flags & SWIG_POINTER_EXCEPTION)
        rb_raise(rb_eTypeError, "Expected %s", ty->str);
      else
        return -1;
    }
    tc = SWIG_TypeCheck(c, ty);
    if (!tc) {
      if (flags & SWIG_POINTER_EXCEPTION)
        rb_raise(rb_eTypeError, "Expected %s", ty->str);
      else
        return -1;
    }
    *ptr = SWIG_TypeCast(tc, *ptr);
  }
  return 0;
}

/* Convert a pointer value, signal an exception on a type mismatch */
static SWIGINLINE void *
SWIG_Ruby_MustGetPtr(VALUE obj, swig_type_info *ty, int argnum, int flags)
{
  void *result;
  SWIG_ConvertPtr(obj, &result, ty, flags | SWIG_POINTER_EXCEPTION);
  return result;
}

/* Check convert */
static SWIGINLINE int
SWIG_Ruby_CheckConvert(VALUE obj, swig_type_info *ty)
{
  char *c = SWIG_MangleStr(obj);
  if (!c)
    return 0;
  return SWIG_TypeCheck(c,ty) != 0;
}

static VALUE
SWIG_Ruby_NewPackedObj(void *ptr, int sz, swig_type_info *type) {
  char result[1024];
  char *r = result;
  if ((2*sz + 1 + strlen(type->name)) > 1000) return 0;
  *(r++) = '_';
  r = SWIG_PackData(r, ptr, sz);
  strcpy(r, type->name);
  return rb_str_new2(result);
}

/* Convert a packed value value */
static void
SWIG_Ruby_ConvertPacked(VALUE obj, void *ptr, int sz, swig_type_info *ty, int flags) {
  swig_cast_info *tc;
  const char  *c;

  if (TYPE(obj) != T_STRING) goto type_error;
  c = StringValuePtr(obj);
  /* Pointer values must start with leading underscore */
  if (*c != '_') goto type_error;
  c++;
  c = SWIG_UnpackData(c, ptr, sz);
  if (ty) {
    tc = SWIG_TypeCheck(c, ty);
    if (!tc) goto type_error;
  }
  return;

type_error:

  if (flags) {
    if (ty) {
      rb_raise(rb_eTypeError, "Type error. Expected %s", ty->name);
    } else {
      rb_raise(rb_eTypeError, "Expected a pointer");
    }
  }
}

static swig_module_info *SWIG_Ruby_GetModule() {
    VALUE pointer;
    swig_module_info *ret = 0;

   /* first check if pointer already created */
    pointer = rb_gv_get("$swig_runtime_data_type_pointer" SWIG_RUNTIME_VERSION SWIG_TYPE_TABLE_NAME);
    if (pointer != Qnil) {
      Data_Get_Struct(pointer, swig_module_info, ret);
    }
    return ret;
}

static void SWIG_Ruby_SetModule(swig_module_info *pointer) {
      /* register a new class */
      VALUE cl = rb_define_class("swig_runtime_data", rb_cObject);
      /* create and store the structure pointer to a global variable */
      swig_runtime_data_type_pointer = Data_Wrap_Struct(cl, 0, 0, pointer);
      rb_define_readonly_variable("$swig_runtime_data_type_pointer" SWIG_RUNTIME_VERSION SWIG_TYPE_TABLE_NAME, &swig_runtime_data_type_pointer);
}

#ifdef __cplusplus
}
#endif



/* -------- TYPES TABLE (BEGIN) -------- */

#define SWIGTYPE_p_CallbackData swig_types[0]
#define SWIGTYPE_p_f_p_sqlite3_context__void swig_types[1]
#define SWIGTYPE_p_f_p_sqlite3_context_int_p_p_sqlite3_value__void swig_types[2]
#define SWIGTYPE_p_f_p_void__void swig_types[3]
#define SWIGTYPE_p_f_p_void_int__int swig_types[4]
#define SWIGTYPE_p_f_p_void_int_p_q_const__char_p_q_const__char_p_q_const__char_p_q_const__char__int swig_types[5]
#define SWIGTYPE_p_f_p_void_p_q_const__char__void swig_types[6]
#define SWIGTYPE_p_p_char swig_types[7]
#define SWIGTYPE_p_p_sqlite3 swig_types[8]
#define SWIGTYPE_p_p_sqlite3_stmt swig_types[9]
#define SWIGTYPE_p_p_void swig_types[10]
#define SWIGTYPE_p_sqlite3 swig_types[11]
#define SWIGTYPE_p_sqlite3_context swig_types[12]
#define SWIGTYPE_p_sqlite3_stmt swig_types[13]
#define SWIGTYPE_p_sqlite3_value swig_types[14]
#define SWIGTYPE_p_sqlite_int64 swig_types[15]
#define SWIGTYPE_p_unsigned_long swig_types[16]
#define SWIGTYPE_p_void swig_types[17]
static swig_type_info *swig_types[19];
static swig_module_info swig_module = {swig_types, 18, 0, 0, 0, 0};
#define SWIG_TypeQuery(name) SWIG_TypeQueryModule(&swig_module, &swig_module, name)
#define SWIG_MangledTypeQuery(name) SWIG_MangledTypeQueryModule(&swig_module, &swig_module, name)

/* -------- TYPES TABLE (END) -------- */

#define SWIG_init    Init_API
#define SWIG_name    "SQLite3::Driver::Native::API"

static VALUE mAPI;

static void SWIG_AsVal(VALUE obj, int *val)
{
    *val = (int) NUM2INT(obj);
}


#ifdef __cplusplus
extern "C" {
#endif
#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
struct timeval rb_time_timeval(VALUE);
#endif
#ifdef __cplusplus
}
#endif


#ifdef __cplusplus
extern "C" {
#endif
//trv #include "rubyio.h"
#ifdef __cplusplus
}
#endif


#include "sqlite3.h"
#include "ruby.h"

#ifndef RSTRING_PTR
#define RSTRING_PTR(s) (RSTRING(s)->ptr)
#endif
 
#ifndef RSTRING_LEN
#define RSTRING_LEN(s) (RSTRING(s)->len)
#endif

#define Init_API Init_sqlite3_api

struct CallbackData {
  VALUE proc;
  VALUE proc2;
  VALUE data;
};

typedef struct CallbackData CallbackData;
typedef void RUBY_BLOB;
typedef void RUBY_VALBLOB;

int Sqlite3_ruby_busy_handler(void* data,int value) {
  VALUE result;
  CallbackData *cb = (CallbackData*)data;
  result = rb_funcall(
    cb->proc, rb_intern("call"), 2, cb->data, INT2FIX(value) );
  return FIX2INT(result);
}

static void mark_CallbackData(void* ptr) {
    CallbackData* cb = (CallbackData*)ptr;
    if (cb->proc != Qnil)
        rb_gc_mark(cb->proc);
    if (cb->proc2 != Qnil)
        rb_gc_mark(cb->proc2);
    if (cb->data != Qnil)
        rb_gc_mark(cb->data);
}

int Sqlite3_ruby_authorizer(void* data,int type,
  const char* a,const char* b,const char* c,const char* d)
{
  VALUE result;
  CallbackData *cb = (CallbackData*)data;
  result = rb_funcall(
    cb->proc, rb_intern("call"), 6, cb->data, INT2FIX(type),
    ( a ? rb_str_new2(a) : Qnil ), ( b ? rb_str_new2(b) : Qnil ),
    ( c ? rb_str_new2(c) : Qnil ), ( d ? rb_str_new2(d) : Qnil ) );
  return FIX2INT(result);
}

void Sqlite3_ruby_trace(void* data, const char *sql) {
  CallbackData *cb = (CallbackData*)data;
  rb_funcall( cb->proc, rb_intern("call"), 2, cb->data,
    sql ? rb_str_new2(sql) : Qnil );
}

void Sqlite3_ruby_function_step(sqlite3_context* ctx,int n,
  sqlite3_value** args)
{
  CallbackData *data;
  VALUE rb_args;
  VALUE *rb_context;
  int idx;
  
  data = (CallbackData*)sqlite3_user_data(ctx);

  if( data->proc2 != Qnil ) {
    rb_context = (VALUE*)sqlite3_aggregate_context(ctx,sizeof(VALUE));
    if( *rb_context == 0 ) {
      *rb_context = rb_hash_new();
      rb_gc_register_address( rb_context );
    }
  }

  rb_args = rb_ary_new2(n+1);
  rb_ary_push( rb_args, SWIG_NewPointerObj(ctx,SWIGTYPE_p_sqlite3_context,0) );
  for( idx = 0; idx < n; idx++ ) {
    rb_ary_push( rb_args, SWIG_NewPointerObj(args[idx],
      SWIGTYPE_p_sqlite3_value,0) );
  }

  rb_apply( data->proc, rb_intern("call"), rb_args );
}

void Sqlite3_ruby_function_final(sqlite3_context *ctx) {
  VALUE *rb_context;
  CallbackData *data;
  
  rb_context = (VALUE*)sqlite3_aggregate_context(ctx,sizeof(VALUE));
  if( *rb_context == 0 ) {
    *rb_context = rb_hash_new();
    rb_gc_register_address( rb_context );
  }

  data = (CallbackData*)sqlite3_user_data(ctx);

  rb_funcall( data->proc2, rb_intern("call"), 1,
    SWIG_NewPointerObj(ctx,SWIGTYPE_p_sqlite3_context,0) );

  rb_gc_unregister_address( rb_context );
}

swig_class cCallbackData;

static VALUE
_wrap_CallbackData_proc_set(int argc, VALUE *argv, VALUE self) {
    struct CallbackData *arg1 = (struct CallbackData *) 0 ;
    VALUE arg2 ;
    
    if ((argc < 1) || (argc > 1))
    rb_raise(rb_eArgError, "wrong # of arguments(%d for 1)",argc);
    SWIG_ConvertPtr(self, (void **) &arg1, SWIGTYPE_p_CallbackData, 0);
    arg2 = argv[0];
    if (arg1) (arg1)->proc = arg2;
    
    return Qnil;
}


static VALUE
_wrap_CallbackData_proc_get(int argc, VALUE *argv, VALUE self) {
    struct CallbackData *arg1 = (struct CallbackData *) 0 ;
    VALUE result;
    VALUE vresult = Qnil;
    
    if ((argc < 0) || (argc > 0))
    rb_raise(rb_eArgError, "wrong # of arguments(%d for 0)",argc);
    SWIG_ConvertPtr(self, (void **) &arg1, SWIGTYPE_p_CallbackData, 0);
    result = (VALUE) ((arg1)->proc);
    
    vresult = result;
    return vresult;
}


static VALUE
_wrap_CallbackData_proc2_set(int argc, VALUE *argv, VALUE self) {
    struct CallbackData *arg1 = (struct CallbackData *) 0 ;
    VALUE arg2 ;
    
    if ((argc < 1) || (argc > 1))
    rb_raise(rb_eArgError, "wrong # of arguments(%d for 1)",argc);
    SWIG_ConvertPtr(self, (void **) &arg1, SWIGTYPE_p_CallbackData, 0);
    arg2 = argv[0];
    if (arg1) (arg1)->proc2 = arg2;
    
    return Qnil;
}


static VALUE
_wrap_CallbackData_proc2_get(int argc, VALUE *argv, VALUE self) {
    struct CallbackData *arg1 = (struct CallbackData *) 0 ;
    VALUE result;
    VALUE vresult = Qnil;
    
    if ((argc < 0) || (argc > 0))
    rb_raise(rb_eArgError, "wrong # of arguments(%d for 0)",argc);
    SWIG_ConvertPtr(self, (void **) &arg1, SWIGTYPE_p_CallbackData, 0);
    result = (VALUE) ((arg1)->proc2);
    
    vresult = result;
    return vresult;
}


static VALUE
_wrap_CallbackData_data_set(int argc, VALUE *argv, VALUE self) {
    struct CallbackData *arg1 = (struct CallbackData *) 0 ;
    VALUE arg2 ;
    
    if ((argc < 1) || (argc > 1))
    rb_raise(rb_eArgError, "wrong # of arguments(%d for 1)",argc);
    SWIG_ConvertPtr(self, (void **) &arg1, SWIGTYPE_p_CallbackData, 0);
    arg2 = argv[0];
    if (arg1) (arg1)->data = arg2;
    
    return Qnil;
}


static VALUE
_wrap_CallbackData_data_get(int argc, VALUE *argv, VALUE self) {
    struct CallbackData *arg1 = (struct CallbackData *) 0 ;
    VALUE result;
    VALUE vresult = Qnil;
    
    if ((argc < 0) || (argc > 0))
    rb_raise(rb_eArgError, "wrong # of arguments(%d for 0)",argc);
    SWIG_ConvertPtr(self, (void **) &arg1, SWIGTYPE_p_CallbackData, 0);
    result = (VALUE) ((arg1)->data);
    
    vresult = result;
    return vresult;
}


#ifdef HAVE_RB_DEFINE_ALLOC_FUNC
static VALUE
_wrap_CallbackData_allocate(VALUE self) {
#else
    static VALUE
    _wrap_CallbackData_allocate(int argc, VALUE *argv, VALUE self) {
#endif
        
        
        VALUE vresult = SWIG_NewClassInstance(self, SWIGTYPE_p_CallbackData);
#ifndef HAVE_RB_DEFINE_ALLOC_FUNC
        rb_obj_call_init(vresult, argc, argv);
#endif
        return vresult;
    }
    

static VALUE
_wrap_new_CallbackData(int argc, VALUE *argv, VALUE self) {
    struct CallbackData *result;
    
    if ((argc < 0) || (argc > 0))
    rb_raise(rb_eArgError, "wrong # of arguments(%d for 0)",argc);
    result = (struct CallbackData *)(struct CallbackData *) calloc(1, sizeof(struct CallbackData));
    DATA_PTR(self) = result;
    
    return self;
}


static void
free_CallbackData(struct CallbackData *arg1) {
    free((char *) arg1);
}

static VALUE
_wrap_sqlite3_libversion(int argc, VALUE *argv, VALUE self) {
    char *result;
    VALUE vresult = Qnil;
    
    if ((argc < 0) || (argc > 0))
    rb_raise(rb_eArgError, "wrong # of arguments(%d for 0)",argc);
    result = (char *)sqlite3_libversion();
    
    {
        vresult = result ? rb_str_new2(result) : Qnil;
    }
    return vresult;
}


static VALUE
_wrap_sqlite3_close(int argc, VALUE *argv, VALUE self) {
    sqlite3 *arg1 = (sqlite3 *) 0 ;
    int result;
    VALUE vresult = Qnil;
    
    if ((argc < 1) || (argc > 1))
    rb_raise(rb_eArgError, "wrong # of arguments(%d for 1)",argc);
    SWIG_ConvertPtr(argv[0], (void **) &arg1, SWIGTYPE_p_sqlite3, 0);
    result = (int)sqlite3_close(arg1);
    
    vresult = INT2NUM(result);
    return vresult;
}


static VALUE
_wrap_sqlite3_last_insert_rowid(int argc, VALUE *argv, VALUE self) {
    sqlite3 *arg1 = (sqlite3 *) 0 ;
    sqlite_int64 result;
    VALUE vresult = Qnil;
    
    if ((argc < 1) || (argc > 1))
    rb_raise(rb_eArgError, "wrong # of arguments(%d for 1)",argc);
    SWIG_ConvertPtr(argv[0], (void **) &arg1, SWIGTYPE_p_sqlite3, 0);
    result = sqlite3_last_insert_rowid(arg1);
    
    {
        vresult = rb_ll2inum( result );
    }
    return vresult;
}


static VALUE
_wrap_sqlite3_changes(int argc, VALUE *argv, VALUE self) {
    sqlite3 *arg1 = (sqlite3 *) 0 ;
    int result;
    VALUE vresult = Qnil;
    
    if ((argc < 1) || (argc > 1))
    rb_raise(rb_eArgError, "wrong # of arguments(%d for 1)",argc);
    SWIG_ConvertPtr(argv[0], (void **) &arg1, SWIGTYPE_p_sqlite3, 0);
    result = (int)sqlite3_changes(arg1);
    
    vresult = INT2NUM(result);
    return vresult;
}


static VALUE
_wrap_sqlite3_total_changes(int argc, VALUE *argv, VALUE self) {
    sqlite3 *arg1 = (sqlite3 *) 0 ;
    int result;
    VALUE vresult = Qnil;
    
    if ((argc < 1) || (argc > 1))
    rb_raise(rb_eArgError, "wrong # of arguments(%d for 1)",argc);
    SWIG_ConvertPtr(argv[0], (void **) &arg1, SWIGTYPE_p_sqlite3, 0);
    result = (int)sqlite3_total_changes(arg1);
    
    vresult = INT2NUM(result);
    return vresult;
}


static VALUE
_wrap_sqlite3_interrupt(int argc, VALUE *argv, VALUE self) {
    sqlite3 *arg1 = (sqlite3 *) 0 ;
    
    if ((argc < 1) || (argc > 1))
    rb_raise(rb_eArgError, "wrong # of arguments(%d for 1)",argc);
    SWIG_ConvertPtr(argv[0], (void **) &arg1, SWIGTYPE_p_sqlite3, 0);
    sqlite3_interrupt(arg1);
    
    return Qnil;
}


static VALUE
_wrap_sqlite3_complete(int argc, VALUE *argv, VALUE self) {
    char *arg1 = (char *) 0 ;
    int result;
    VALUE vresult = Qnil;
    
    if ((argc < 1) || (argc > 1))
    rb_raise(rb_eArgError, "wrong # of arguments(%d for 1)",argc);
    arg1 = StringValuePtr(argv[0]);
    result = (int)sqlite3_complete((char const *)arg1);
    
    vresult = INT2NUM(result);
    return vresult;
}


static VALUE
_wrap_sqlite3_complete16(int argc, VALUE *argv, VALUE self) {
    void *arg1 = (void *) 0 ;
    int result;
    VALUE vresult = Qnil;
    
    if ((argc < 1) || (argc > 1))
    rb_raise(rb_eArgError, "wrong # of arguments(%d for 1)",argc);
    {
        arg1 = (void*)RSTRING_PTR(argv[0]);
    }
    result = (int)sqlite3_complete16((void const *)arg1);
    
    vresult = INT2NUM(result);
    return vresult;
}


static VALUE
_wrap_sqlite3_busy_handler(int argc, VALUE *argv, VALUE self) {
    sqlite3 *arg1 = (sqlite3 *) 0 ;
    int (*arg2)(void *,int) = (int (*)(void *,int)) 0 ;
    void *arg3 = (void *) 0 ;
    int result;
    VALUE vresult = Qnil;
    
    if ((argc < 3) || (argc > 3))
    rb_raise(rb_eArgError, "wrong # of arguments(%d for 3)",argc);
    SWIG_ConvertPtr(argv[0], (void **) &arg1, SWIGTYPE_p_sqlite3, 0);
    SWIG_ConvertPtr(argv[1], (void **) &arg2, SWIGTYPE_p_f_p_void_int__int, 0);
    SWIG_ConvertPtr(argv[2], (void **) &arg3, 0, SWIG_POINTER_EXCEPTION|0);
    result = (int)sqlite3_busy_handler(arg1,arg2,arg3);
    
    vresult = INT2NUM(result);
    return vresult;
}


static VALUE
_wrap_sqlite3_busy_timeout(int argc, VALUE *argv, VALUE self) {
    sqlite3 *arg1 = (sqlite3 *) 0 ;
    int arg2 ;
    int result;
    VALUE vresult = Qnil;
    
    if ((argc < 2) || (argc > 2))
    rb_raise(rb_eArgError, "wrong # of arguments(%d for 2)",argc);
    SWIG_ConvertPtr(argv[0], (void **) &arg1, SWIGTYPE_p_sqlite3, 0);
    arg2 = NUM2INT(argv[1]);
    result = (int)sqlite3_busy_timeout(arg1,arg2);
    
    vresult = INT2NUM(result);
    return vresult;
}


static VALUE
_wrap_sqlite3_set_authorizer(int argc, VALUE *argv, VALUE self) {
    sqlite3 *arg1 = (sqlite3 *) 0 ;
    int (*arg2)(void *,int,char const *,char const *,char const *,char const *) = (int (*)(void *,int,char const *,char const *,char const *,char const *)) 0 ;
    void *arg3 = (void *) 0 ;
    int result;
    VALUE vresult = Qnil;
    
    if ((argc < 3) || (argc > 3))
    rb_raise(rb_eArgError, "wrong # of arguments(%d for 3)",argc);
    SWIG_ConvertPtr(argv[0], (void **) &arg1, SWIGTYPE_p_sqlite3, 0);
    SWIG_ConvertPtr(argv[1], (void **) &arg2, SWIGTYPE_p_f_p_void_int_p_q_const__char_p_q_const__char_p_q_const__char_p_q_const__char__int, 0);
    SWIG_ConvertPtr(argv[2], (void **) &arg3, 0, SWIG_POINTER_EXCEPTION|0);
    result = (int)sqlite3_set_authorizer(arg1,arg2,arg3);
    
    vresult = INT2NUM(result);
    return vresult;
}


static VALUE
_wrap_sqlite3_trace(int argc, VALUE *argv, VALUE self) {
    sqlite3 *arg1 = (sqlite3 *) 0 ;
    void (*arg2)(void *,char const *) = (void (*)(void *,char const *)) 0 ;
    void *arg3 = (void *) 0 ;
    int result;
    VALUE vresult = Qnil;
    
    if ((argc < 3) || (argc > 3))
    rb_raise(rb_eArgError, "wrong # of arguments(%d for 3)",argc);
    SWIG_ConvertPtr(argv[0], (void **) &arg1, SWIGTYPE_p_sqlite3, 0);
    SWIG_ConvertPtr(argv[1], (void **) &arg2, SWIGTYPE_p_f_p_void_p_q_const__char__void, 0);
    SWIG_ConvertPtr(argv[2], (void **) &arg3, 0, SWIG_POINTER_EXCEPTION|0);
    result = (int)sqlite3_trace(arg1,arg2,arg3);
    
    vresult = INT2NUM(result);
    return vresult;
}

void setup_delete_db_callback(sqlite3 * db);
static VALUE
_wrap_sqlite3_open(int argc, VALUE *argv, VALUE self) {
    char *arg1 = (char *) 0 ;
    sqlite3 **arg2 = (sqlite3 **) 0 ;
    int result;
    VALUE vresult = Qnil;
    
    if ((argc < 1) || (argc > 1))
    rb_raise(rb_eArgError, "wrong # of arguments(%d for 1)",argc);
    {
        arg1 = STR2CSTR(argv[0]);
        arg2 = (sqlite3**)malloc( sizeof( sqlite3* ) );
    }
    result = (int)sqlite3_open((char const *)arg1,arg2);
    setup_delete_db_callback(*arg2);

    vresult = INT2NUM(result);
    {
        VALUE ary;
        ary = rb_ary_new2(2);
        rb_ary_push( ary, vresult );
        rb_ary_push( ary, SWIG_NewPointerObj( *arg2, SWIGTYPE_p_sqlite3, 0 ) );
        free( arg2 );
        vresult = ary;
    }
    return vresult;
}


static VALUE
_wrap_sqlite3_open16(int argc, VALUE *argv, VALUE self) {
    void *arg1 = (void *) 0 ;
    sqlite3 **arg2 = (sqlite3 **) 0 ;
    int result;
    VALUE vresult = Qnil;
    
    if ((argc < 1) || (argc > 1))
    rb_raise(rb_eArgError, "wrong # of arguments(%d for 1)",argc);
    {
        arg1 = (void*)RSTRING_PTR(argv[0]);
        arg2 = (sqlite3**)malloc( sizeof( sqlite3* ) );
    }
    result = (int)sqlite3_open16((void const *)arg1,arg2);
    
    vresult = INT2NUM(result);
    {
        VALUE ary;
        ary = rb_ary_new2(2);
        rb_ary_push( ary, vresult );
        rb_ary_push( ary, SWIG_NewPointerObj( *arg2, SWIGTYPE_p_sqlite3, 0 ) );
        free( arg2 );
        vresult = ary;
    }
    return vresult;
}


static VALUE
_wrap_sqlite3_errcode(int argc, VALUE *argv, VALUE self) {
    sqlite3 *arg1 = (sqlite3 *) 0 ;
    int result;
    VALUE vresult = Qnil;
    
    if ((argc < 1) || (argc > 1))
    rb_raise(rb_eArgError, "wrong # of arguments(%d for 1)",argc);
    SWIG_ConvertPtr(argv[0], (void **) &arg1, SWIGTYPE_p_sqlite3, 0);
    result = (int)sqlite3_errcode(arg1);
    
    vresult = INT2NUM(result);
    return vresult;
}


static VALUE
_wrap_sqlite3_errmsg(int argc, VALUE *argv, VALUE self) {
    sqlite3 *arg1 = (sqlite3 *) 0 ;
    char *result;
    VALUE vresult = Qnil;
    
    if ((argc < 1) || (argc > 1))
    rb_raise(rb_eArgError, "wrong # of arguments(%d for 1)",argc);
    SWIG_ConvertPtr(argv[0], (void **) &arg1, SWIGTYPE_p_sqlite3, 0);
    result = (char *)sqlite3_errmsg(arg1);
    
    {
        vresult = result ? rb_str_new2(result) : Qnil;
    }
    return vresult;
}


static VALUE
_wrap_sqlite3_errmsg16(int argc, VALUE *argv, VALUE self) {
    sqlite3 *arg1 = (sqlite3 *) 0 ;
    void *result;
    VALUE vresult = Qnil;
    
    if ((argc < 1) || (argc > 1))
    rb_raise(rb_eArgError, "wrong # of arguments(%d for 1)",argc);
    SWIG_ConvertPtr(argv[0], (void **) &arg1, SWIGTYPE_p_sqlite3, 0);
    result = (void *)sqlite3_errmsg16(arg1);
    
    {
        int i;
        if( result ) {
            for( i = 0; ((char*)result)[i]; i += 2 );
            vresult = rb_str_new( (char*)result, i );
        } else vresult = Qnil;
    }
    return vresult;
}


static VALUE
_wrap_sqlite3_prepare(int argc, VALUE *argv, VALUE self) {
    sqlite3 *arg1 = (sqlite3 *) 0 ;
    char *arg2 = (char *) 0 ;
    int arg3 ;
    sqlite3_stmt **arg4 = (sqlite3_stmt **) 0 ;
    char **arg5 = (char **) 0 ;
    int result;
    sqlite3_stmt *stmt2 ;
    char *errmsg2 ;
    VALUE vresult = Qnil;
    
    if ((argc < 2) || (argc > 2))
    rb_raise(rb_eArgError, "wrong # of arguments(%d for 2)",argc);
    SWIG_ConvertPtr(argv[0], (void **) &arg1, SWIGTYPE_p_sqlite3, 0);
    {
        arg2 = RSTRING_PTR(argv[1]);
        arg3 = RSTRING_LEN(argv[1]);
        arg4 = &stmt2;
        arg5 = &errmsg2;
    }
    result = (int)sqlite3_prepare(arg1,(char const *)arg2,arg3,arg4,(char const **)arg5);
    
    vresult = INT2NUM(result);
    {
        VALUE ary;
        ary = rb_ary_new2(3);
        rb_ary_push( ary, vresult );
        rb_ary_push( ary, SWIG_NewPointerObj( stmt2, SWIGTYPE_p_sqlite3_stmt, 0 ) );
        rb_ary_push( ary, errmsg2 ? rb_str_new2( errmsg2 ) : Qnil );
        vresult = ary;
    }
    return vresult;
}


static VALUE
_wrap_sqlite3_prepare16(int argc, VALUE *argv, VALUE self) {
    sqlite3 *arg1 = (sqlite3 *) 0 ;
    void *arg2 = (void *) 0 ;
    int arg3 ;
    sqlite3_stmt **arg4 = (sqlite3_stmt **) 0 ;
    void **arg5 = (void **) 0 ;
    int result;
    sqlite3_stmt *stmt2 ;
    void *errmsg2 ;
    VALUE vresult = Qnil;
    
    if ((argc < 2) || (argc > 2))
    rb_raise(rb_eArgError, "wrong # of arguments(%d for 2)",argc);
    SWIG_ConvertPtr(argv[0], (void **) &arg1, SWIGTYPE_p_sqlite3, 0);
    {
        arg2 = RSTRING_PTR(argv[1]);
        arg3 = RSTRING_LEN(argv[1]);
        arg4 = &stmt2;
        arg5 = &errmsg2;
    }
    result = (int)sqlite3_prepare16(arg1,(void const *)arg2,arg3,arg4,(void const **)arg5);
    
    vresult = INT2NUM(result);
    {
        VALUE ary;
        int i;
        
        for( i = 0; ((char*)errmsg2)[i]; i += 2 );
        
        ary = rb_ary_new2(3);
        rb_ary_push( ary, vresult );
        rb_ary_push( ary, SWIG_NewPointerObj( stmt2, SWIGTYPE_p_sqlite3_stmt, 0 ) );
        rb_ary_push( ary, errmsg2 ? rb_str_new( (char*)errmsg2, i ) : Qnil );
        vresult = ary;
    }
    return vresult;
}


static VALUE
_wrap_sqlite3_bind_blob(int argc, VALUE *argv, VALUE self) {
    sqlite3_stmt *arg1 = (sqlite3_stmt *) 0 ;
    int arg2 ;
    void *arg3 = (void *) 0 ;
    int arg4 ;
    void (*arg5)(void *) = (void (*)(void *)) 0 ;
    int result;
    VALUE vresult = Qnil;
    
    if ((argc < 3) || (argc > 3))
    rb_raise(rb_eArgError, "wrong # of arguments(%d for 3)",argc);
    SWIG_ConvertPtr(argv[0], (void **) &arg1, SWIGTYPE_p_sqlite3_stmt, 0);
    arg2 = NUM2INT(argv[1]);
    {
        arg3 = (void*)RSTRING_PTR(argv[2]);
        arg4 = RSTRING_LEN(argv[2]);
        arg5 = SQLITE_TRANSIENT;
    }
    result = (int)sqlite3_bind_blob(arg1,arg2,(void const *)arg3,arg4,arg5);
    
    vresult = INT2NUM(result);
    return vresult;
}


static VALUE
_wrap_sqlite3_bind_double(int argc, VALUE *argv, VALUE self) {
    sqlite3_stmt *arg1 = (sqlite3_stmt *) 0 ;
    int arg2 ;
    double arg3 ;
    int result;
    VALUE vresult = Qnil;
    
    if ((argc < 3) || (argc > 3))
    rb_raise(rb_eArgError, "wrong # of arguments(%d for 3)",argc);
    SWIG_ConvertPtr(argv[0], (void **) &arg1, SWIGTYPE_p_sqlite3_stmt, 0);
    arg2 = NUM2INT(argv[1]);
    arg3 = (double) NUM2DBL(argv[2]);
    result = (int)sqlite3_bind_double(arg1,arg2,arg3);
    
    vresult = INT2NUM(result);
    return vresult;
}


static VALUE
_wrap_sqlite3_bind_int(int argc, VALUE *argv, VALUE self) {
    sqlite3_stmt *arg1 = (sqlite3_stmt *) 0 ;
    int arg2 ;
    int arg3 ;
    int result;
    VALUE vresult = Qnil;
    
    if ((argc < 3) || (argc > 3))
    rb_raise(rb_eArgError, "wrong # of arguments(%d for 3)",argc);
    SWIG_ConvertPtr(argv[0], (void **) &arg1, SWIGTYPE_p_sqlite3_stmt, 0);
    arg2 = NUM2INT(argv[1]);
    arg3 = NUM2INT(argv[2]);
    result = (int)sqlite3_bind_int(arg1,arg2,arg3);
    
    vresult = INT2NUM(result);
    return vresult;
}


static VALUE
_wrap_sqlite3_bind_int64(int argc, VALUE *argv, VALUE self) {
    sqlite3_stmt *arg1 = (sqlite3_stmt *) 0 ;
    int arg2 ;
    sqlite_int64 arg3 ;
    int result;
    VALUE vresult = Qnil;
    
    if ((argc < 3) || (argc > 3))
    rb_raise(rb_eArgError, "wrong # of arguments(%d for 3)",argc);
    SWIG_ConvertPtr(argv[0], (void **) &arg1, SWIGTYPE_p_sqlite3_stmt, 0);
    arg2 = NUM2INT(argv[1]);
    {
        arg3 = rb_num2ll( argv[2] );
    }
    result = (int)sqlite3_bind_int64(arg1,arg2,arg3);
    
    vresult = INT2NUM(result);
    return vresult;
}


static VALUE
_wrap_sqlite3_bind_null(int argc, VALUE *argv, VALUE self) {
    sqlite3_stmt *arg1 = (sqlite3_stmt *) 0 ;
    int arg2 ;
    int result;
    VALUE vresult = Qnil;
    
    if ((argc < 2) || (argc > 2))
    rb_raise(rb_eArgError, "wrong # of arguments(%d for 2)",argc);
    SWIG_ConvertPtr(argv[0], (void **) &arg1, SWIGTYPE_p_sqlite3_stmt, 0);
    arg2 = NUM2INT(argv[1]);
    result = (int)sqlite3_bind_null(arg1,arg2);
    
    vresult = INT2NUM(result);
    return vresult;
}


static VALUE
_wrap_sqlite3_bind_text(int argc, VALUE *argv, VALUE self) {
    sqlite3_stmt *arg1 = (sqlite3_stmt *) 0 ;
    int arg2 ;
    char *arg3 = (char *) 0 ;
    int arg4 ;
    void (*arg5)(void *) = (void (*)(void *)) 0 ;
    int result;
    VALUE vresult = Qnil;
    
    if ((argc < 3) || (argc > 3))
    rb_raise(rb_eArgError, "wrong # of arguments(%d for 3)",argc);
    SWIG_ConvertPtr(argv[0], (void **) &arg1, SWIGTYPE_p_sqlite3_stmt, 0);
    arg2 = NUM2INT(argv[1]);
    {
        arg3 = RSTRING_PTR(argv[2]);
        arg4 = RSTRING_LEN(argv[2]);
        arg5 = SQLITE_TRANSIENT;
    }
    result = (int)sqlite3_bind_text(arg1,arg2,(char const *)arg3,arg4,arg5);
    
    vresult = INT2NUM(result);
    return vresult;
}


static VALUE
_wrap_sqlite3_bind_text16(int argc, VALUE *argv, VALUE self) {
    sqlite3_stmt *arg1 = (sqlite3_stmt *) 0 ;
    int arg2 ;
    void *arg3 = (void *) 0 ;
    int arg4 ;
    void (*arg5)(void *) = (void (*)(void *)) 0 ;
    int result;
    VALUE vresult = Qnil;
    
    if ((argc < 3) || (argc > 3))
    rb_raise(rb_eArgError, "wrong # of arguments(%d for 3)",argc);
    SWIG_ConvertPtr(argv[0], (void **) &arg1, SWIGTYPE_p_sqlite3_stmt, 0);
    arg2 = NUM2INT(argv[1]);
    {
        arg3 = (void*)RSTRING_PTR(argv[2]);
        arg4 = RSTRING_LEN(argv[2]);
        arg5 = SQLITE_TRANSIENT;
    }
    result = (int)sqlite3_bind_text16(arg1,arg2,(void const *)arg3,arg4,arg5);
    
    vresult = INT2NUM(result);
    return vresult;
}


static VALUE
_wrap_sqlite3_bind_parameter_count(int argc, VALUE *argv, VALUE self) {
    sqlite3_stmt *arg1 = (sqlite3_stmt *) 0 ;
    int result;
    VALUE vresult = Qnil;
    
    if ((argc < 1) || (argc > 1))
    rb_raise(rb_eArgError, "wrong # of arguments(%d for 1)",argc);
    SWIG_ConvertPtr(argv[0], (void **) &arg1, SWIGTYPE_p_sqlite3_stmt, 0);
    result = (int)sqlite3_bind_parameter_count(arg1);
    
    vresult = INT2NUM(result);
    return vresult;
}


static VALUE
_wrap_sqlite3_bind_parameter_name(int argc, VALUE *argv, VALUE self) {
    sqlite3_stmt *arg1 = (sqlite3_stmt *) 0 ;
    int arg2 ;
    char *result;
    VALUE vresult = Qnil;
    
    if ((argc < 2) || (argc > 2))
    rb_raise(rb_eArgError, "wrong # of arguments(%d for 2)",argc);
    SWIG_ConvertPtr(argv[0], (void **) &arg1, SWIGTYPE_p_sqlite3_stmt, 0);
    arg2 = NUM2INT(argv[1]);
    result = (char *)sqlite3_bind_parameter_name(arg1,arg2);
    
    {
        vresult = result ? rb_str_new2(result) : Qnil;
    }
    return vresult;
}


static VALUE
_wrap_sqlite3_bind_parameter_index(int argc, VALUE *argv, VALUE self) {
    sqlite3_stmt *arg1 = (sqlite3_stmt *) 0 ;
    char *arg2 = (char *) 0 ;
    int result;
    VALUE vresult = Qnil;
    
    if ((argc < 2) || (argc > 2))
    rb_raise(rb_eArgError, "wrong # of arguments(%d for 2)",argc);
    SWIG_ConvertPtr(argv[0], (void **) &arg1, SWIGTYPE_p_sqlite3_stmt, 0);
    arg2 = StringValuePtr(argv[1]);
    result = (int)sqlite3_bind_parameter_index(arg1,(char const *)arg2);
    
    vresult = INT2NUM(result);
    return vresult;
}


static VALUE
_wrap_sqlite3_column_count(int argc, VALUE *argv, VALUE self) {
    sqlite3_stmt *arg1 = (sqlite3_stmt *) 0 ;
    int result;
    VALUE vresult = Qnil;
    
    if ((argc < 1) || (argc > 1))
    rb_raise(rb_eArgError, "wrong # of arguments(%d for 1)",argc);
    SWIG_ConvertPtr(argv[0], (void **) &arg1, SWIGTYPE_p_sqlite3_stmt, 0);
    result = (int)sqlite3_column_count(arg1);
    
    vresult = INT2NUM(result);
    return vresult;
}


static VALUE
_wrap_sqlite3_column_name(int argc, VALUE *argv, VALUE self) {
    sqlite3_stmt *arg1 = (sqlite3_stmt *) 0 ;
    int arg2 ;
    char *result;
    VALUE vresult = Qnil;
    
    if ((argc < 2) || (argc > 2))
    rb_raise(rb_eArgError, "wrong # of arguments(%d for 2)",argc);
    SWIG_ConvertPtr(argv[0], (void **) &arg1, SWIGTYPE_p_sqlite3_stmt, 0);
    arg2 = NUM2INT(argv[1]);
    result = (char *)sqlite3_column_name(arg1,arg2);
    
    {
        vresult = result ? rb_str_new2(result) : Qnil;
    }
    return vresult;
}


static VALUE
_wrap_sqlite3_column_name16(int argc, VALUE *argv, VALUE self) {
    sqlite3_stmt *arg1 = (sqlite3_stmt *) 0 ;
    int arg2 ;
    void *result;
    VALUE vresult = Qnil;
    
    if ((argc < 2) || (argc > 2))
    rb_raise(rb_eArgError, "wrong # of arguments(%d for 2)",argc);
    SWIG_ConvertPtr(argv[0], (void **) &arg1, SWIGTYPE_p_sqlite3_stmt, 0);
    arg2 = NUM2INT(argv[1]);
    result = (void *)sqlite3_column_name16(arg1,arg2);
    
    {
        int i;
        if( result ) {
            for( i = 0; ((char*)result)[i]; i += 2 );
            vresult = rb_str_new( (char*)result, i );
        } else vresult = Qnil;
    }
    return vresult;
}


static VALUE
_wrap_sqlite3_column_decltype(int argc, VALUE *argv, VALUE self) {
    sqlite3_stmt *arg1 = (sqlite3_stmt *) 0 ;
    int arg2 ;
    char *result;
    VALUE vresult = Qnil;
    
    if ((argc < 2) || (argc > 2))
    rb_raise(rb_eArgError, "wrong # of arguments(%d for 2)",argc);
    SWIG_ConvertPtr(argv[0], (void **) &arg1, SWIGTYPE_p_sqlite3_stmt, 0);
    arg2 = NUM2INT(argv[1]);
    result = (char *)sqlite3_column_decltype(arg1,arg2);
    
    {
        vresult = result ? rb_str_new2(result) : Qnil;
    }
    return vresult;
}


static VALUE
_wrap_sqlite3_column_decltype16(int argc, VALUE *argv, VALUE self) {
    sqlite3_stmt *arg1 = (sqlite3_stmt *) 0 ;
    int arg2 ;
    void *result;
    VALUE vresult = Qnil;
    
    if ((argc < 2) || (argc > 2))
    rb_raise(rb_eArgError, "wrong # of arguments(%d for 2)",argc);
    SWIG_ConvertPtr(argv[0], (void **) &arg1, SWIGTYPE_p_sqlite3_stmt, 0);
    arg2 = NUM2INT(argv[1]);
    result = (void *)sqlite3_column_decltype16(arg1,arg2);
    
    {
        int i;
        if( result ) {
            for( i = 0; ((char*)result)[i]; i += 2 );
            vresult = rb_str_new( (char*)result, i );
        } else vresult = Qnil;
    }
    return vresult;
}


static VALUE
_wrap_sqlite3_step(int argc, VALUE *argv, VALUE self) {
    sqlite3_stmt *arg1 = (sqlite3_stmt *) 0 ;
    int result;
    VALUE vresult = Qnil;
    
    if ((argc < 1) || (argc > 1))
    rb_raise(rb_eArgError, "wrong # of arguments(%d for 1)",argc);
    SWIG_ConvertPtr(argv[0], (void **) &arg1, SWIGTYPE_p_sqlite3_stmt, 0);
    result = (int)sqlite3_step(arg1);
    
    vresult = INT2NUM(result);
    return vresult;
}


static VALUE
_wrap_sqlite3_data_count(int argc, VALUE *argv, VALUE self) {
    sqlite3_stmt *arg1 = (sqlite3_stmt *) 0 ;
    int result;
    VALUE vresult = Qnil;
    
    if ((argc < 1) || (argc > 1))
    rb_raise(rb_eArgError, "wrong # of arguments(%d for 1)",argc);
    SWIG_ConvertPtr(argv[0], (void **) &arg1, SWIGTYPE_p_sqlite3_stmt, 0);
    result = (int)sqlite3_data_count(arg1);
    
    vresult = INT2NUM(result);
    return vresult;
}


static VALUE
_wrap_sqlite3_column_blob(int argc, VALUE *argv, VALUE self) {
    sqlite3_stmt *arg1 = (sqlite3_stmt *) 0 ;
    int arg2 ;
    RUBY_BLOB *result;
    VALUE vresult = Qnil;
    
    if ((argc < 2) || (argc > 2))
    rb_raise(rb_eArgError, "wrong # of arguments(%d for 2)",argc);
    SWIG_ConvertPtr(argv[0], (void **) &arg1, SWIGTYPE_p_sqlite3_stmt, 0);
    arg2 = NUM2INT(argv[1]);
    result = (RUBY_BLOB *)sqlite3_column_blob(arg1,arg2);
    
    {
        vresult = result ?
        rb_str_new( (char*)result, sqlite3_column_bytes( arg1, arg2 ) ) : Qnil;
    }
    return vresult;
}


static VALUE
_wrap_sqlite3_column_bytes(int argc, VALUE *argv, VALUE self) {
    sqlite3_stmt *arg1 = (sqlite3_stmt *) 0 ;
    int arg2 ;
    int result;
    VALUE vresult = Qnil;
    
    if ((argc < 2) || (argc > 2))
    rb_raise(rb_eArgError, "wrong # of arguments(%d for 2)",argc);
    SWIG_ConvertPtr(argv[0], (void **) &arg1, SWIGTYPE_p_sqlite3_stmt, 0);
    arg2 = NUM2INT(argv[1]);
    result = (int)sqlite3_column_bytes(arg1,arg2);
    
    vresult = INT2NUM(result);
    return vresult;
}


static VALUE
_wrap_sqlite3_column_bytes16(int argc, VALUE *argv, VALUE self) {
    sqlite3_stmt *arg1 = (sqlite3_stmt *) 0 ;
    int arg2 ;
    int result;
    VALUE vresult = Qnil;
    
    if ((argc < 2) || (argc > 2))
    rb_raise(rb_eArgError, "wrong # of arguments(%d for 2)",argc);
    SWIG_ConvertPtr(argv[0], (void **) &arg1, SWIGTYPE_p_sqlite3_stmt, 0);
    arg2 = NUM2INT(argv[1]);
    result = (int)sqlite3_column_bytes16(arg1,arg2);
    
    vresult = INT2NUM(result);
    return vresult;
}


static VALUE
_wrap_sqlite3_column_double(int argc, VALUE *argv, VALUE self) {
    sqlite3_stmt *arg1 = (sqlite3_stmt *) 0 ;
    int arg2 ;
    double result;
    VALUE vresult = Qnil;
    
    if ((argc < 2) || (argc > 2))
    rb_raise(rb_eArgError, "wrong # of arguments(%d for 2)",argc);
    SWIG_ConvertPtr(argv[0], (void **) &arg1, SWIGTYPE_p_sqlite3_stmt, 0);
    arg2 = NUM2INT(argv[1]);
    result = (double)sqlite3_column_double(arg1,arg2);
    
    vresult = rb_float_new(result);
    return vresult;
}


static VALUE
_wrap_sqlite3_column_int(int argc, VALUE *argv, VALUE self) {
    sqlite3_stmt *arg1 = (sqlite3_stmt *) 0 ;
    int arg2 ;
    double result;
    VALUE vresult = Qnil;
    
    if ((argc < 2) || (argc > 2))
    rb_raise(rb_eArgError, "wrong # of arguments(%d for 2)",argc);
    SWIG_ConvertPtr(argv[0], (void **) &arg1, SWIGTYPE_p_sqlite3_stmt, 0);
    arg2 = NUM2INT(argv[1]);
    result = (double)sqlite3_column_int(arg1,arg2);
    
    vresult = rb_float_new(result);
    return vresult;
}


static VALUE
_wrap_sqlite3_column_int64(int argc, VALUE *argv, VALUE self) {
    sqlite3_stmt *arg1 = (sqlite3_stmt *) 0 ;
    int arg2 ;
    sqlite_int64 result;
    VALUE vresult = Qnil;
    
    if ((argc < 2) || (argc > 2))
    rb_raise(rb_eArgError, "wrong # of arguments(%d for 2)",argc);
    SWIG_ConvertPtr(argv[0], (void **) &arg1, SWIGTYPE_p_sqlite3_stmt, 0);
    arg2 = NUM2INT(argv[1]);
    result = sqlite3_column_int64(arg1,arg2);
    
    {
        vresult = rb_ll2inum( result );
    }
    return vresult;
}


static VALUE
_wrap_sqlite3_column_text(int argc, VALUE *argv, VALUE self) {
    sqlite3_stmt *arg1 = (sqlite3_stmt *) 0 ;
    int arg2 ;
    char *result;
    VALUE vresult = Qnil;
    
    if ((argc < 2) || (argc > 2))
    rb_raise(rb_eArgError, "wrong # of arguments(%d for 2)",argc);
    SWIG_ConvertPtr(argv[0], (void **) &arg1, SWIGTYPE_p_sqlite3_stmt, 0);
    arg2 = NUM2INT(argv[1]);
    result = (char *)sqlite3_column_text(arg1,arg2);
    
    {
        vresult = result ? rb_str_new2(result) : Qnil;
    }
    return vresult;
}


static VALUE
_wrap_sqlite3_column_text16(int argc, VALUE *argv, VALUE self) {
    sqlite3_stmt *arg1 = (sqlite3_stmt *) 0 ;
    int arg2 ;
    void *result;
    VALUE vresult = Qnil;
    
    if ((argc < 2) || (argc > 2))
    rb_raise(rb_eArgError, "wrong # of arguments(%d for 2)",argc);
    SWIG_ConvertPtr(argv[0], (void **) &arg1, SWIGTYPE_p_sqlite3_stmt, 0);
    arg2 = NUM2INT(argv[1]);
    result = (void *)sqlite3_column_text16(arg1,arg2);
    
    {
        int i;
        if( result ) {
            for( i = 0; ((char*)result)[i]; i += 2 );
            vresult = rb_str_new( (char*)result, i );
        } else vresult = Qnil;
    }
    return vresult;
}


static VALUE
_wrap_sqlite3_column_type(int argc, VALUE *argv, VALUE self) {
    sqlite3_stmt *arg1 = (sqlite3_stmt *) 0 ;
    int arg2 ;
    int result;
    VALUE vresult = Qnil;
    
    if ((argc < 2) || (argc > 2))
    rb_raise(rb_eArgError, "wrong # of arguments(%d for 2)",argc);
    SWIG_ConvertPtr(argv[0], (void **) &arg1, SWIGTYPE_p_sqlite3_stmt, 0);
    arg2 = NUM2INT(argv[1]);
    result = (int)sqlite3_column_type(arg1,arg2);
    
    vresult = INT2NUM(result);
    return vresult;
}


static VALUE
_wrap_sqlite3_finalize(int argc, VALUE *argv, VALUE self) {
    sqlite3_stmt *arg1 = (sqlite3_stmt *) 0 ;
    int result;
    VALUE vresult = Qnil;
    
    if ((argc < 1) || (argc > 1))
    rb_raise(rb_eArgError, "wrong # of arguments(%d for 1)",argc);
    SWIG_ConvertPtr(argv[0], (void **) &arg1, SWIGTYPE_p_sqlite3_stmt, 0);
    result = (int)sqlite3_finalize(arg1);
    
    vresult = INT2NUM(result);
    return vresult;
}


static VALUE
_wrap_sqlite3_reset(int argc, VALUE *argv, VALUE self) {
    sqlite3_stmt *arg1 = (sqlite3_stmt *) 0 ;
    int result;
    VALUE vresult = Qnil;
    
    if ((argc < 1) || (argc > 1))
    rb_raise(rb_eArgError, "wrong # of arguments(%d for 1)",argc);
    SWIG_ConvertPtr(argv[0], (void **) &arg1, SWIGTYPE_p_sqlite3_stmt, 0);
    result = (int)sqlite3_reset(arg1);
    
    vresult = INT2NUM(result);
    return vresult;
}


static VALUE
_wrap_sqlite3_create_function(int argc, VALUE *argv, VALUE self) {
    sqlite3 *arg1 = (sqlite3 *) 0 ;
    char *arg2 = (char *) 0 ;
    int arg3 ;
    int arg4 ;
    void *arg5 = (void *) 0 ;
    void (*arg6)(sqlite3_context *,int,sqlite3_value **) = (void (*)(sqlite3_context *,int,sqlite3_value **)) 0 ;
    void (*arg7)(sqlite3_context *,int,sqlite3_value **) = (void (*)(sqlite3_context *,int,sqlite3_value **)) 0 ;
    void (*arg8)(sqlite3_context *) = (void (*)(sqlite3_context *)) 0 ;
    int result;
    VALUE vresult = Qnil;
    
    if ((argc < 8) || (argc > 8))
    rb_raise(rb_eArgError, "wrong # of arguments(%d for 8)",argc);
    SWIG_ConvertPtr(argv[0], (void **) &arg1, SWIGTYPE_p_sqlite3, 0);
    arg2 = StringValuePtr(argv[1]);
    arg3 = NUM2INT(argv[2]);
    arg4 = NUM2INT(argv[3]);
    SWIG_ConvertPtr(argv[4], (void **) &arg5, 0, SWIG_POINTER_EXCEPTION|0);
    SWIG_ConvertPtr(argv[5], (void **) &arg6, SWIGTYPE_p_f_p_sqlite3_context_int_p_p_sqlite3_value__void, 0);
    SWIG_ConvertPtr(argv[6], (void **) &arg7, SWIGTYPE_p_f_p_sqlite3_context_int_p_p_sqlite3_value__void, 0);
    SWIG_ConvertPtr(argv[7], (void **) &arg8, SWIGTYPE_p_f_p_sqlite3_context__void, 0);
    result = (int)sqlite3_create_function(arg1,(char const *)arg2,arg3,arg4,arg5,arg6,arg7,arg8);
    
    vresult = INT2NUM(result);
    return vresult;
}


static VALUE
_wrap_sqlite3_create_function16(int argc, VALUE *argv, VALUE self) {
    sqlite3 *arg1 = (sqlite3 *) 0 ;
    void *arg2 = (void *) 0 ;
    int arg3 ;
    int arg4 ;
    void *arg5 = (void *) 0 ;
    void (*arg6)(sqlite3_context *,int,sqlite3_value **) = (void (*)(sqlite3_context *,int,sqlite3_value **)) 0 ;
    void (*arg7)(sqlite3_context *,int,sqlite3_value **) = (void (*)(sqlite3_context *,int,sqlite3_value **)) 0 ;
    void (*arg8)(sqlite3_context *) = (void (*)(sqlite3_context *)) 0 ;
    int result;
    VALUE vresult = Qnil;
    
    if ((argc < 8) || (argc > 8))
    rb_raise(rb_eArgError, "wrong # of arguments(%d for 8)",argc);
    SWIG_ConvertPtr(argv[0], (void **) &arg1, SWIGTYPE_p_sqlite3, 0);
    {
        arg2 = (void*)RSTRING_PTR(argv[1]);
    }
    arg3 = NUM2INT(argv[2]);
    arg4 = NUM2INT(argv[3]);
    SWIG_ConvertPtr(argv[4], (void **) &arg5, 0, SWIG_POINTER_EXCEPTION|0);
    SWIG_ConvertPtr(argv[5], (void **) &arg6, SWIGTYPE_p_f_p_sqlite3_context_int_p_p_sqlite3_value__void, 0);
    SWIG_ConvertPtr(argv[6], (void **) &arg7, SWIGTYPE_p_f_p_sqlite3_context_int_p_p_sqlite3_value__void, 0);
    SWIG_ConvertPtr(argv[7], (void **) &arg8, SWIGTYPE_p_f_p_sqlite3_context__void, 0);
    result = (int)sqlite3_create_function16(arg1,(void const *)arg2,arg3,arg4,arg5,arg6,arg7,arg8);
    
    vresult = INT2NUM(result);
    return vresult;
}


static VALUE
_wrap_sqlite3_aggregate_count(int argc, VALUE *argv, VALUE self) {
    sqlite3_context *arg1 = (sqlite3_context *) 0 ;
    int result;
    VALUE vresult = Qnil;
    
    if ((argc < 1) || (argc > 1))
    rb_raise(rb_eArgError, "wrong # of arguments(%d for 1)",argc);
    SWIG_ConvertPtr(argv[0], (void **) &arg1, SWIGTYPE_p_sqlite3_context, 0);
    result = (int)sqlite3_aggregate_count(arg1);
    
    vresult = INT2NUM(result);
    return vresult;
}


static VALUE
_wrap_sqlite3_value_blob(int argc, VALUE *argv, VALUE self) {
    sqlite3_value *arg1 = (sqlite3_value *) 0 ;
    RUBY_VALBLOB *result;
    VALUE vresult = Qnil;
    
    if ((argc < 1) || (argc > 1))
    rb_raise(rb_eArgError, "wrong # of arguments(%d for 1)",argc);
    SWIG_ConvertPtr(argv[0], (void **) &arg1, SWIGTYPE_p_sqlite3_value, 0);
    result = (RUBY_VALBLOB *)sqlite3_value_blob(arg1);
    
    {
        vresult = result ? rb_str_new( (char*)result, sqlite3_value_bytes( arg1 ) ) : Qnil;
    }
    return vresult;
}


static VALUE
_wrap_sqlite3_value_bytes(int argc, VALUE *argv, VALUE self) {
    sqlite3_value *arg1 = (sqlite3_value *) 0 ;
    int result;
    VALUE vresult = Qnil;
    
    if ((argc < 1) || (argc > 1))
    rb_raise(rb_eArgError, "wrong # of arguments(%d for 1)",argc);
    SWIG_ConvertPtr(argv[0], (void **) &arg1, SWIGTYPE_p_sqlite3_value, 0);
    result = (int)sqlite3_value_bytes(arg1);
    
    vresult = INT2NUM(result);
    return vresult;
}


static VALUE
_wrap_sqlite3_value_bytes16(int argc, VALUE *argv, VALUE self) {
    sqlite3_value *arg1 = (sqlite3_value *) 0 ;
    int result;
    VALUE vresult = Qnil;
    
    if ((argc < 1) || (argc > 1))
    rb_raise(rb_eArgError, "wrong # of arguments(%d for 1)",argc);
    SWIG_ConvertPtr(argv[0], (void **) &arg1, SWIGTYPE_p_sqlite3_value, 0);
    result = (int)sqlite3_value_bytes16(arg1);
    
    vresult = INT2NUM(result);
    return vresult;
}


static VALUE
_wrap_sqlite3_value_double(int argc, VALUE *argv, VALUE self) {
    sqlite3_value *arg1 = (sqlite3_value *) 0 ;
    double result;
    VALUE vresult = Qnil;
    
    if ((argc < 1) || (argc > 1))
    rb_raise(rb_eArgError, "wrong # of arguments(%d for 1)",argc);
    SWIG_ConvertPtr(argv[0], (void **) &arg1, SWIGTYPE_p_sqlite3_value, 0);
    result = (double)sqlite3_value_double(arg1);
    
    vresult = rb_float_new(result);
    return vresult;
}


static VALUE
_wrap_sqlite3_value_int(int argc, VALUE *argv, VALUE self) {
    sqlite3_value *arg1 = (sqlite3_value *) 0 ;
    int result;
    VALUE vresult = Qnil;
    
    if ((argc < 1) || (argc > 1))
    rb_raise(rb_eArgError, "wrong # of arguments(%d for 1)",argc);
    SWIG_ConvertPtr(argv[0], (void **) &arg1, SWIGTYPE_p_sqlite3_value, 0);
    result = (int)sqlite3_value_int(arg1);
    
    vresult = INT2NUM(result);
    return vresult;
}


static VALUE
_wrap_sqlite3_value_int64(int argc, VALUE *argv, VALUE self) {
    sqlite3_value *arg1 = (sqlite3_value *) 0 ;
    sqlite_int64 result;
    VALUE vresult = Qnil;
    
    if ((argc < 1) || (argc > 1))
    rb_raise(rb_eArgError, "wrong # of arguments(%d for 1)",argc);
    SWIG_ConvertPtr(argv[0], (void **) &arg1, SWIGTYPE_p_sqlite3_value, 0);
    result = sqlite3_value_int64(arg1);
    
    {
        vresult = rb_ll2inum( result );
    }
    return vresult;
}


static VALUE
_wrap_sqlite3_value_text(int argc, VALUE *argv, VALUE self) {
    sqlite3_value *arg1 = (sqlite3_value *) 0 ;
    char *result;
    VALUE vresult = Qnil;
    
    if ((argc < 1) || (argc > 1))
    rb_raise(rb_eArgError, "wrong # of arguments(%d for 1)",argc);
    SWIG_ConvertPtr(argv[0], (void **) &arg1, SWIGTYPE_p_sqlite3_value, 0);
    result = (char *)sqlite3_value_text(arg1);
    
    {
        vresult = result ? rb_str_new2(result) : Qnil;
    }
    return vresult;
}


static VALUE
_wrap_sqlite3_value_text16(int argc, VALUE *argv, VALUE self) {
    sqlite3_value *arg1 = (sqlite3_value *) 0 ;
    void *result;
    VALUE vresult = Qnil;
    
    if ((argc < 1) || (argc > 1))
    rb_raise(rb_eArgError, "wrong # of arguments(%d for 1)",argc);
    SWIG_ConvertPtr(argv[0], (void **) &arg1, SWIGTYPE_p_sqlite3_value, 0);
    result = (void *)sqlite3_value_text16(arg1);
    
    {
        int i;
        if( result ) {
            for( i = 0; ((char*)result)[i]; i += 2 );
            vresult = rb_str_new( (char*)result, i );
        } else vresult = Qnil;
    }
    return vresult;
}


static VALUE
_wrap_sqlite3_value_text16le(int argc, VALUE *argv, VALUE self) {
    sqlite3_value *arg1 = (sqlite3_value *) 0 ;
    void *result;
    VALUE vresult = Qnil;
    
    if ((argc < 1) || (argc > 1))
    rb_raise(rb_eArgError, "wrong # of arguments(%d for 1)",argc);
    SWIG_ConvertPtr(argv[0], (void **) &arg1, SWIGTYPE_p_sqlite3_value, 0);
    result = (void *)sqlite3_value_text16le(arg1);
    
    {
        int i;
        if( result ) {
            for( i = 0; ((char*)result)[i]; i += 2 );
            vresult = rb_str_new( (char*)result, i );
        } else vresult = Qnil;
    }
    return vresult;
}


static VALUE
_wrap_sqlite3_value_text16be(int argc, VALUE *argv, VALUE self) {
    sqlite3_value *arg1 = (sqlite3_value *) 0 ;
    void *result;
    VALUE vresult = Qnil;
    
    if ((argc < 1) || (argc > 1))
    rb_raise(rb_eArgError, "wrong # of arguments(%d for 1)",argc);
    SWIG_ConvertPtr(argv[0], (void **) &arg1, SWIGTYPE_p_sqlite3_value, 0);
    result = (void *)sqlite3_value_text16be(arg1);
    
    {
        int i;
        if( result ) {
            for( i = 0; ((char*)result)[i]; i += 2 );
            vresult = rb_str_new( (char*)result, i );
        } else vresult = Qnil;
    }
    return vresult;
}


static VALUE
_wrap_sqlite3_value_type(int argc, VALUE *argv, VALUE self) {
    sqlite3_value *arg1 = (sqlite3_value *) 0 ;
    int result;
    VALUE vresult = Qnil;
    
    if ((argc < 1) || (argc > 1))
    rb_raise(rb_eArgError, "wrong # of arguments(%d for 1)",argc);
    SWIG_ConvertPtr(argv[0], (void **) &arg1, SWIGTYPE_p_sqlite3_value, 0);
    result = (int)sqlite3_value_type(arg1);
    
    vresult = INT2NUM(result);
    return vresult;
}


static VALUE
_wrap_sqlite3_result_blob(int argc, VALUE *argv, VALUE self) {
    sqlite3_context *arg1 = (sqlite3_context *) 0 ;
    void *arg2 = (void *) 0 ;
    int arg3 ;
    void (*arg4)(void *) = (void (*)(void *)) 0 ;
    
    if ((argc < 2) || (argc > 2))
    rb_raise(rb_eArgError, "wrong # of arguments(%d for 2)",argc);
    SWIG_ConvertPtr(argv[0], (void **) &arg1, SWIGTYPE_p_sqlite3_context, 0);
    {
        arg2 = (void*)RSTRING_PTR(argv[1]);
        arg3 = RSTRING_LEN(argv[1]);
        arg4 = SQLITE_TRANSIENT;
    }
    sqlite3_result_blob(arg1,(void const *)arg2,arg3,arg4);
    
    return Qnil;
}


static VALUE
_wrap_sqlite3_result_double(int argc, VALUE *argv, VALUE self) {
    sqlite3_context *arg1 = (sqlite3_context *) 0 ;
    double arg2 ;
    
    if ((argc < 2) || (argc > 2))
    rb_raise(rb_eArgError, "wrong # of arguments(%d for 2)",argc);
    SWIG_ConvertPtr(argv[0], (void **) &arg1, SWIGTYPE_p_sqlite3_context, 0);
    arg2 = (double) NUM2DBL(argv[1]);
    sqlite3_result_double(arg1,arg2);
    
    return Qnil;
}


static VALUE
_wrap_sqlite3_result_error(int argc, VALUE *argv, VALUE self) {
    sqlite3_context *arg1 = (sqlite3_context *) 0 ;
    char *arg2 = (char *) 0 ;
    int arg3 ;
    
    if ((argc < 2) || (argc > 2))
    rb_raise(rb_eArgError, "wrong # of arguments(%d for 2)",argc);
    SWIG_ConvertPtr(argv[0], (void **) &arg1, SWIGTYPE_p_sqlite3_context, 0);
    {
        arg2 = RSTRING_PTR(argv[1]);
        arg3 = RSTRING_LEN(argv[1]);
    }
    sqlite3_result_error(arg1,(char const *)arg2,arg3);
    
    return Qnil;
}


static VALUE
_wrap_sqlite3_result_error16(int argc, VALUE *argv, VALUE self) {
    sqlite3_context *arg1 = (sqlite3_context *) 0 ;
    void *arg2 = (void *) 0 ;
    int arg3 ;
    
    if ((argc < 2) || (argc > 2))
    rb_raise(rb_eArgError, "wrong # of arguments(%d for 2)",argc);
    SWIG_ConvertPtr(argv[0], (void **) &arg1, SWIGTYPE_p_sqlite3_context, 0);
    {
        arg2 = (void*)RSTRING_PTR(argv[1]);
        arg3 = RSTRING_LEN(argv[1]);
    }
    sqlite3_result_error16(arg1,(void const *)arg2,arg3);
    
    return Qnil;
}


static VALUE
_wrap_sqlite3_result_int(int argc, VALUE *argv, VALUE self) {
    sqlite3_context *arg1 = (sqlite3_context *) 0 ;
    int arg2 ;
    
    if ((argc < 2) || (argc > 2))
    rb_raise(rb_eArgError, "wrong # of arguments(%d for 2)",argc);
    SWIG_ConvertPtr(argv[0], (void **) &arg1, SWIGTYPE_p_sqlite3_context, 0);
    arg2 = NUM2INT(argv[1]);
    sqlite3_result_int(arg1,arg2);
    
    return Qnil;
}


static VALUE
_wrap_sqlite3_result_int64(int argc, VALUE *argv, VALUE self) {
    sqlite3_context *arg1 = (sqlite3_context *) 0 ;
    sqlite_int64 arg2 ;
    
    if ((argc < 2) || (argc > 2))
    rb_raise(rb_eArgError, "wrong # of arguments(%d for 2)",argc);
    SWIG_ConvertPtr(argv[0], (void **) &arg1, SWIGTYPE_p_sqlite3_context, 0);
    {
        arg2 = rb_num2ll( argv[1] );
    }
    sqlite3_result_int64(arg1,arg2);
    
    return Qnil;
}


static VALUE
_wrap_sqlite3_result_text(int argc, VALUE *argv, VALUE self) {
    sqlite3_context *arg1 = (sqlite3_context *) 0 ;
    char *arg2 = (char *) 0 ;
    int arg3 ;
    void (*arg4)(void *) = (void (*)(void *)) 0 ;
    
    if ((argc < 2) || (argc > 2))
    rb_raise(rb_eArgError, "wrong # of arguments(%d for 2)",argc);
    SWIG_ConvertPtr(argv[0], (void **) &arg1, SWIGTYPE_p_sqlite3_context, 0);
    {
        arg2 = RSTRING_PTR(argv[1]);
        arg3 = RSTRING_LEN(argv[1]);
        arg4 = SQLITE_TRANSIENT;
    }
    sqlite3_result_text(arg1,(char const *)arg2,arg3,arg4);
    
    return Qnil;
}


static VALUE
_wrap_sqlite3_result_text16(int argc, VALUE *argv, VALUE self) {
    sqlite3_context *arg1 = (sqlite3_context *) 0 ;
    void *arg2 = (void *) 0 ;
    int arg3 ;
    void (*arg4)(void *) = (void (*)(void *)) 0 ;
    
    if ((argc < 2) || (argc > 2))
    rb_raise(rb_eArgError, "wrong # of arguments(%d for 2)",argc);
    SWIG_ConvertPtr(argv[0], (void **) &arg1, SWIGTYPE_p_sqlite3_context, 0);
    {
        arg2 = (void*)RSTRING_PTR(argv[1]);
        arg3 = RSTRING_LEN(argv[1]);
        arg4 = SQLITE_TRANSIENT;
    }
    sqlite3_result_text16(arg1,(void const *)arg2,arg3,arg4);
    
    return Qnil;
}


static VALUE
_wrap_sqlite3_result_text16le(int argc, VALUE *argv, VALUE self) {
    sqlite3_context *arg1 = (sqlite3_context *) 0 ;
    void *arg2 = (void *) 0 ;
    int arg3 ;
    void (*arg4)(void *) = (void (*)(void *)) 0 ;
    
    if ((argc < 2) || (argc > 2))
    rb_raise(rb_eArgError, "wrong # of arguments(%d for 2)",argc);
    SWIG_ConvertPtr(argv[0], (void **) &arg1, SWIGTYPE_p_sqlite3_context, 0);
    {
        arg2 = (void*)RSTRING_PTR(argv[1]);
        arg3 = RSTRING_LEN(argv[1]);
        arg4 = SQLITE_TRANSIENT;
    }
    sqlite3_result_text16le(arg1,(void const *)arg2,arg3,arg4);
    
    return Qnil;
}


static VALUE
_wrap_sqlite3_result_text16be(int argc, VALUE *argv, VALUE self) {
    sqlite3_context *arg1 = (sqlite3_context *) 0 ;
    void *arg2 = (void *) 0 ;
    int arg3 ;
    void (*arg4)(void *) = (void (*)(void *)) 0 ;
    
    if ((argc < 2) || (argc > 2))
    rb_raise(rb_eArgError, "wrong # of arguments(%d for 2)",argc);
    SWIG_ConvertPtr(argv[0], (void **) &arg1, SWIGTYPE_p_sqlite3_context, 0);
    {
        arg2 = (void*)RSTRING_PTR(argv[1]);
        arg3 = RSTRING_LEN(argv[1]);
        arg4 = SQLITE_TRANSIENT;
    }
    sqlite3_result_text16be(arg1,(void const *)arg2,arg3,arg4);
    
    return Qnil;
}


static VALUE
_wrap_sqlite3_result_value(int argc, VALUE *argv, VALUE self) {
    sqlite3_context *arg1 = (sqlite3_context *) 0 ;
    sqlite3_value *arg2 = (sqlite3_value *) 0 ;
    
    if ((argc < 2) || (argc > 2))
    rb_raise(rb_eArgError, "wrong # of arguments(%d for 2)",argc);
    SWIG_ConvertPtr(argv[0], (void **) &arg1, SWIGTYPE_p_sqlite3_context, 0);
    SWIG_ConvertPtr(argv[1], (void **) &arg2, SWIGTYPE_p_sqlite3_value, 0);
    sqlite3_result_value(arg1,arg2);
    
    return Qnil;
}


static VALUE
_wrap_sqlite3_aggregate_context(int argc, VALUE *argv, VALUE self) {
    sqlite3_context *arg1 = (sqlite3_context *) 0 ;
    int arg2 ;
    VALUE *result;
    VALUE vresult = Qnil;
    
    if ((argc < 1) || (argc > 1))
    rb_raise(rb_eArgError, "wrong # of arguments(%d for 1)",argc);
    {
        SWIG_ConvertPtr(argv[0],(void**)&arg1, SWIGTYPE_p_sqlite3_context, 1);
        arg2 = 4;
    }
    result = (VALUE *)sqlite3_aggregate_context(arg1,arg2);
    
    {
        vresult = *(VALUE*)result;
    }
    return vresult;
}



/* -------- TYPE CONVERSION AND EQUIVALENCE RULES (BEGIN) -------- */

static swig_type_info _swigt__p_CallbackData = {"_p_CallbackData", "struct CallbackData *", 0, 0, 0};
static swig_type_info _swigt__p_f_p_sqlite3_context__void = {"_p_f_p_sqlite3_context__void", "void (*)(sqlite3_context *)", 0, 0, 0};
static swig_type_info _swigt__p_f_p_sqlite3_context_int_p_p_sqlite3_value__void = {"_p_f_p_sqlite3_context_int_p_p_sqlite3_value__void", "void (*)(sqlite3_context *,int,sqlite3_value **)", 0, 0, 0};
static swig_type_info _swigt__p_f_p_void__void = {"_p_f_p_void__void", "void (*)(void *)", 0, 0, 0};
static swig_type_info _swigt__p_f_p_void_int__int = {"_p_f_p_void_int__int", "int (*)(void *,int)", 0, 0, 0};
static swig_type_info _swigt__p_f_p_void_int_p_q_const__char_p_q_const__char_p_q_const__char_p_q_const__char__int = {"_p_f_p_void_int_p_q_const__char_p_q_const__char_p_q_const__char_p_q_const__char__int", "int (*)(void *,int,char const *,char const *,char const *,char const *)", 0, 0, 0};
static swig_type_info _swigt__p_f_p_void_p_q_const__char__void = {"_p_f_p_void_p_q_const__char__void", "void (*)(void *,char const *)", 0, 0, 0};
static swig_type_info _swigt__p_p_char = {"_p_p_char", "char **", 0, 0, 0};
static swig_type_info _swigt__p_p_sqlite3 = {"_p_p_sqlite3", "sqlite3 **", 0, 0, 0};
static swig_type_info _swigt__p_p_sqlite3_stmt = {"_p_p_sqlite3_stmt", "sqlite3_stmt **", 0, 0, 0};
static swig_type_info _swigt__p_p_void = {"_p_p_void", "void **", 0, 0, 0};
static swig_type_info _swigt__p_sqlite3 = {"_p_sqlite3", "sqlite3 *", 0, 0, 0};
static swig_type_info _swigt__p_sqlite3_context = {"_p_sqlite3_context", "sqlite3_context *", 0, 0, 0};
static swig_type_info _swigt__p_sqlite3_stmt = {"_p_sqlite3_stmt", "sqlite3_stmt *", 0, 0, 0};
static swig_type_info _swigt__p_sqlite3_value = {"_p_sqlite3_value", "sqlite3_value *", 0, 0, 0};
static swig_type_info _swigt__p_sqlite_int64 = {"_p_sqlite_int64", "sqlite_int64 *", 0, 0, 0};
static swig_type_info _swigt__p_unsigned_long = {"_p_unsigned_long", "unsigned long *|VALUE *", 0, 0, 0};
static swig_type_info _swigt__p_void = {"_p_void", "void *|RUBY_VALBLOB *", 0, 0, 0};

static swig_type_info *swig_type_initial[] = {
  &_swigt__p_CallbackData,
  &_swigt__p_f_p_sqlite3_context__void,
  &_swigt__p_f_p_sqlite3_context_int_p_p_sqlite3_value__void,
  &_swigt__p_f_p_void__void,
  &_swigt__p_f_p_void_int__int,
  &_swigt__p_f_p_void_int_p_q_const__char_p_q_const__char_p_q_const__char_p_q_const__char__int,
  &_swigt__p_f_p_void_p_q_const__char__void,
  &_swigt__p_p_char,
  &_swigt__p_p_sqlite3,
  &_swigt__p_p_sqlite3_stmt,
  &_swigt__p_p_void,
  &_swigt__p_sqlite3,
  &_swigt__p_sqlite3_context,
  &_swigt__p_sqlite3_stmt,
  &_swigt__p_sqlite3_value,
  &_swigt__p_sqlite_int64,
  &_swigt__p_unsigned_long,
  &_swigt__p_void,
};

static swig_cast_info _swigc__p_CallbackData[] = {  {&_swigt__p_CallbackData, 0, 0, 0},{0, 0, 0, 0}};
static swig_cast_info _swigc__p_f_p_sqlite3_context__void[] = {  {&_swigt__p_f_p_sqlite3_context__void, 0, 0, 0},{0, 0, 0, 0}};
static swig_cast_info _swigc__p_f_p_sqlite3_context_int_p_p_sqlite3_value__void[] = {  {&_swigt__p_f_p_sqlite3_context_int_p_p_sqlite3_value__void, 0, 0, 0},{0, 0, 0, 0}};
static swig_cast_info _swigc__p_f_p_void__void[] = {  {&_swigt__p_f_p_void__void, 0, 0, 0},{0, 0, 0, 0}};
static swig_cast_info _swigc__p_f_p_void_int__int[] = {  {&_swigt__p_f_p_void_int__int, 0, 0, 0},{0, 0, 0, 0}};
static swig_cast_info _swigc__p_f_p_void_int_p_q_const__char_p_q_const__char_p_q_const__char_p_q_const__char__int[] = {  {&_swigt__p_f_p_void_int_p_q_const__char_p_q_const__char_p_q_const__char_p_q_const__char__int, 0, 0, 0},{0, 0, 0, 0}};
static swig_cast_info _swigc__p_f_p_void_p_q_const__char__void[] = {  {&_swigt__p_f_p_void_p_q_const__char__void, 0, 0, 0},{0, 0, 0, 0}};
static swig_cast_info _swigc__p_p_char[] = {  {&_swigt__p_p_char, 0, 0, 0},{0, 0, 0, 0}};
static swig_cast_info _swigc__p_p_sqlite3[] = {  {&_swigt__p_p_sqlite3, 0, 0, 0},{0, 0, 0, 0}};
static swig_cast_info _swigc__p_p_sqlite3_stmt[] = {  {&_swigt__p_p_sqlite3_stmt, 0, 0, 0},{0, 0, 0, 0}};
static swig_cast_info _swigc__p_p_void[] = {  {&_swigt__p_p_void, 0, 0, 0},{0, 0, 0, 0}};
static swig_cast_info _swigc__p_sqlite3[] = {  {&_swigt__p_sqlite3, 0, 0, 0},{0, 0, 0, 0}};
static swig_cast_info _swigc__p_sqlite3_context[] = {  {&_swigt__p_sqlite3_context, 0, 0, 0},{0, 0, 0, 0}};
static swig_cast_info _swigc__p_sqlite3_stmt[] = {  {&_swigt__p_sqlite3_stmt, 0, 0, 0},{0, 0, 0, 0}};
static swig_cast_info _swigc__p_sqlite3_value[] = {  {&_swigt__p_sqlite3_value, 0, 0, 0},{0, 0, 0, 0}};
static swig_cast_info _swigc__p_sqlite_int64[] = {  {&_swigt__p_sqlite_int64, 0, 0, 0},{0, 0, 0, 0}};
static swig_cast_info _swigc__p_unsigned_long[] = {  {&_swigt__p_unsigned_long, 0, 0, 0},{0, 0, 0, 0}};
static swig_cast_info _swigc__p_void[] = {  {&_swigt__p_void, 0, 0, 0},{0, 0, 0, 0}};

static swig_cast_info *swig_cast_initial[] = {
  _swigc__p_CallbackData,
  _swigc__p_f_p_sqlite3_context__void,
  _swigc__p_f_p_sqlite3_context_int_p_p_sqlite3_value__void,
  _swigc__p_f_p_void__void,
  _swigc__p_f_p_void_int__int,
  _swigc__p_f_p_void_int_p_q_const__char_p_q_const__char_p_q_const__char_p_q_const__char__int,
  _swigc__p_f_p_void_p_q_const__char__void,
  _swigc__p_p_char,
  _swigc__p_p_sqlite3,
  _swigc__p_p_sqlite3_stmt,
  _swigc__p_p_void,
  _swigc__p_sqlite3,
  _swigc__p_sqlite3_context,
  _swigc__p_sqlite3_stmt,
  _swigc__p_sqlite3_value,
  _swigc__p_sqlite_int64,
  _swigc__p_unsigned_long,
  _swigc__p_void,
};


/* -------- TYPE CONVERSION AND EQUIVALENCE RULES (END) -------- */

/*************************************************************************
 * Type initialization:
 * This problem is tough by the requirement that no dynamic 
 * memory is used. Also, since swig_type_info structures store pointers to 
 * swig_cast_info structures and swig_cast_info structures store pointers back
 * to swig_type_info structures, we need some lookup code at initialization. 
 * The idea is that swig generates all the structures that are needed. 
 * The runtime then collects these partially filled structures. 
 * The SWIG_InitializeModule function takes these initial arrays out of 
 * swig_module, and does all the lookup, filling in the swig_module.types
 * array with the correct data and linking the correct swig_cast_info
 * structures together.

 * The generated swig_type_info structures are assigned staticly to an initial 
 * array. We just loop though that array, and handle each type individually.
 * First we lookup if this type has been already loaded, and if so, use the
 * loaded structure instead of the generated one. Then we have to fill in the
 * cast linked list. The cast data is initially stored in something like a
 * two-dimensional array. Each row corresponds to a type (there are the same
 * number of rows as there are in the swig_type_initial array). Each entry in
 * a column is one of the swig_cast_info structures for that type.
 * The cast_initial array is actually an array of arrays, because each row has
 * a variable number of columns. So to actually build the cast linked list,
 * we find the array of casts associated with the type, and loop through it 
 * adding the casts to the list. The one last trick we need to do is making
 * sure the type pointer in the swig_cast_info struct is correct.

 * First off, we lookup the cast->type name to see if it is already loaded. 
 * There are three cases to handle:
 *  1) If the cast->type has already been loaded AND the type we are adding
 *     casting info to has not been loaded (it is in this module), THEN we
 *     replace the cast->type pointer with the type pointer that has already
 *     been loaded.
 *  2) If BOTH types (the one we are adding casting info to, and the 
 *     cast->type) are loaded, THEN the cast info has already been loaded by
 *     the previous module so we just ignore it.
 *  3) Finally, if cast->type has not already been loaded, then we add that
 *     swig_cast_info to the linked list (because the cast->type) pointer will
 *     be correct.
**/

#ifdef __cplusplus
extern "C" {
#endif

SWIGRUNTIME void
SWIG_InitializeModule(void *clientdata) {
  swig_type_info *type, *ret;
  swig_cast_info *cast;
  size_t i;
  swig_module_info *module_head;
  static int init_run = 0;

  clientdata = clientdata;

  if (init_run) return;
  init_run = 1;

  /* Initialize the swig_module */
  swig_module.type_initial = swig_type_initial;
  swig_module.cast_initial = swig_cast_initial;

  /* Try and load any already created modules */
  module_head = SWIG_GetModule(clientdata);
  if (module_head) {
    swig_module.next = module_head->next;
    module_head->next = &swig_module;
  } else {
    /* This is the first module loaded */
    swig_module.next = &swig_module;
    SWIG_SetModule(clientdata, &swig_module);
  }
		 
  /* Now work on filling in swig_module.types */
  for (i = 0; i < swig_module.size; ++i) {
    type = 0;

    /* if there is another module already loaded */
    if (swig_module.next != &swig_module) {
      type = SWIG_MangledTypeQueryModule(swig_module.next, &swig_module, swig_module.type_initial[i]->name);
    }
    if (type) {
      /* Overwrite clientdata field */
      if (swig_module.type_initial[i]->clientdata) type->clientdata = swig_module.type_initial[i]->clientdata;
    } else {
      type = swig_module.type_initial[i];
    }

    /* Insert casting types */
    cast = swig_module.cast_initial[i];
    while (cast->type) {
    
      /* Don't need to add information already in the list */
      ret = 0;
      if (swig_module.next != &swig_module) {
        ret = SWIG_MangledTypeQueryModule(swig_module.next, &swig_module, cast->type->name);
      }
      if (ret && type == swig_module.type_initial[i]) {
        cast->type = ret;
        ret = 0;
      }
      
      if (!ret) {
        if (type->cast) {
          type->cast->prev = cast;
          cast->next = type->cast;
        }
        type->cast = cast;
      }

      cast++;
    }

    /* Set entry in modules->types array equal to the type */
    swig_module.types[i] = type;
  }
  swig_module.types[i] = 0;
}

/* This function will propagate the clientdata field of type to
* any new swig_type_info structures that have been added into the list
* of equivalent types.  It is like calling
* SWIG_TypeClientData(type, clientdata) a second time.
*/
SWIGRUNTIME void
SWIG_PropagateClientData(void) {
  size_t i;
  swig_cast_info *equiv;
  static int init_run = 0;

  if (init_run) return;
  init_run = 1;

  for (i = 0; i < swig_module.size; i++) {
    if (swig_module.types[i]->clientdata) {
      equiv = swig_module.types[i]->cast;
      while (equiv) {
        if (!equiv->converter) {
          if (equiv->type && !equiv->type->clientdata)
            SWIG_TypeClientData(equiv->type, swig_module.types[i]->clientdata);
        }
        equiv = equiv->next;
      }
    }
  }
}

#ifdef __cplusplus
}
#endif


#ifdef __cplusplus
extern "C"
#endif
	
SWIGEXPORT void Init_API(void) {
    size_t i;
    
    SWIG_InitRuntime();
    mAPI = rb_define_module("SQLite3");
    mAPI = rb_define_module_under(mAPI, "Driver");
    mAPI = rb_define_module_under(mAPI, "Native");
    mAPI = rb_define_module_under(mAPI, "API");
    
    SWIG_InitializeModule(0);
    for (i = 0; i < swig_module.size; i++) {
        SWIG_define_class(swig_module.types[i]);
    }
    
    SWIG_RubyInitializeTrackings();
    
    cCallbackData.klass = rb_define_class_under(mAPI, "CallbackData", rb_cObject);
    SWIG_TypeClientData(SWIGTYPE_p_CallbackData, (void *) &cCallbackData);
    rb_define_alloc_func(cCallbackData.klass, _wrap_CallbackData_allocate);
    rb_define_method(cCallbackData.klass, "initialize", _wrap_new_CallbackData, -1);
    rb_define_method(cCallbackData.klass, "proc=", _wrap_CallbackData_proc_set, -1);
    rb_define_method(cCallbackData.klass, "proc", _wrap_CallbackData_proc_get, -1);
    rb_define_method(cCallbackData.klass, "proc2=", _wrap_CallbackData_proc2_set, -1);
    rb_define_method(cCallbackData.klass, "proc2", _wrap_CallbackData_proc2_get, -1);
    rb_define_method(cCallbackData.klass, "data=", _wrap_CallbackData_data_set, -1);
    rb_define_method(cCallbackData.klass, "data", _wrap_CallbackData_data_get, -1);
    cCallbackData.mark = (void (*)(void *)) mark_CallbackData;
    cCallbackData.destroy = (void (*)(void *)) free_CallbackData;
    rb_define_const(mAPI,"Sqlite3_ruby_busy_handler", SWIG_NewPointerObj((void *) Sqlite3_ruby_busy_handler, SWIGTYPE_p_f_p_void_int__int,0));
    rb_define_const(mAPI,"Sqlite3_ruby_authorizer", SWIG_NewPointerObj((void *) Sqlite3_ruby_authorizer, SWIGTYPE_p_f_p_void_int_p_q_const__char_p_q_const__char_p_q_const__char_p_q_const__char__int,0));
    rb_define_const(mAPI,"Sqlite3_ruby_trace", SWIG_NewPointerObj((void *) Sqlite3_ruby_trace, SWIGTYPE_p_f_p_void_p_q_const__char__void,0));
    rb_define_const(mAPI,"Sqlite3_ruby_function_step", SWIG_NewPointerObj((void *) Sqlite3_ruby_function_step, SWIGTYPE_p_f_p_sqlite3_context_int_p_p_sqlite3_value__void,0));
    rb_define_const(mAPI,"Sqlite3_ruby_function_final", SWIG_NewPointerObj((void *) Sqlite3_ruby_function_final, SWIGTYPE_p_f_p_sqlite3_context__void,0));
    rb_define_module_function(mAPI, "sqlite3_libversion", _wrap_sqlite3_libversion, -1);
    rb_define_module_function(mAPI, "sqlite3_close", _wrap_sqlite3_close, -1);
    rb_define_module_function(mAPI, "sqlite3_last_insert_rowid", _wrap_sqlite3_last_insert_rowid, -1);
    rb_define_module_function(mAPI, "sqlite3_changes", _wrap_sqlite3_changes, -1);
    rb_define_module_function(mAPI, "sqlite3_total_changes", _wrap_sqlite3_total_changes, -1);
    rb_define_module_function(mAPI, "sqlite3_interrupt", _wrap_sqlite3_interrupt, -1);
    rb_define_module_function(mAPI, "sqlite3_complete", _wrap_sqlite3_complete, -1);
    rb_define_module_function(mAPI, "sqlite3_complete16", _wrap_sqlite3_complete16, -1);
    rb_define_module_function(mAPI, "sqlite3_busy_handler", _wrap_sqlite3_busy_handler, -1);
    rb_define_module_function(mAPI, "sqlite3_busy_timeout", _wrap_sqlite3_busy_timeout, -1);
    rb_define_module_function(mAPI, "sqlite3_set_authorizer", _wrap_sqlite3_set_authorizer, -1);
    rb_define_module_function(mAPI, "sqlite3_trace", _wrap_sqlite3_trace, -1);
    rb_define_module_function(mAPI, "sqlite3_open", _wrap_sqlite3_open, -1);
    rb_define_module_function(mAPI, "sqlite3_open16", _wrap_sqlite3_open16, -1);
    rb_define_module_function(mAPI, "sqlite3_errcode", _wrap_sqlite3_errcode, -1);
    rb_define_module_function(mAPI, "sqlite3_errmsg", _wrap_sqlite3_errmsg, -1);
    rb_define_module_function(mAPI, "sqlite3_errmsg16", _wrap_sqlite3_errmsg16, -1);
    rb_define_module_function(mAPI, "sqlite3_prepare", _wrap_sqlite3_prepare, -1);
    rb_define_module_function(mAPI, "sqlite3_prepare16", _wrap_sqlite3_prepare16, -1);
    rb_define_module_function(mAPI, "sqlite3_bind_blob", _wrap_sqlite3_bind_blob, -1);
    rb_define_module_function(mAPI, "sqlite3_bind_double", _wrap_sqlite3_bind_double, -1);
    rb_define_module_function(mAPI, "sqlite3_bind_int", _wrap_sqlite3_bind_int, -1);
    rb_define_module_function(mAPI, "sqlite3_bind_int64", _wrap_sqlite3_bind_int64, -1);
    rb_define_module_function(mAPI, "sqlite3_bind_null", _wrap_sqlite3_bind_null, -1);
    rb_define_module_function(mAPI, "sqlite3_bind_text", _wrap_sqlite3_bind_text, -1);
    rb_define_module_function(mAPI, "sqlite3_bind_text16", _wrap_sqlite3_bind_text16, -1);
    rb_define_module_function(mAPI, "sqlite3_bind_parameter_count", _wrap_sqlite3_bind_parameter_count, -1);
    rb_define_module_function(mAPI, "sqlite3_bind_parameter_name", _wrap_sqlite3_bind_parameter_name, -1);
    rb_define_module_function(mAPI, "sqlite3_bind_parameter_index", _wrap_sqlite3_bind_parameter_index, -1);
    rb_define_module_function(mAPI, "sqlite3_column_count", _wrap_sqlite3_column_count, -1);
    rb_define_module_function(mAPI, "sqlite3_column_name", _wrap_sqlite3_column_name, -1);
    rb_define_module_function(mAPI, "sqlite3_column_name16", _wrap_sqlite3_column_name16, -1);
    rb_define_module_function(mAPI, "sqlite3_column_decltype", _wrap_sqlite3_column_decltype, -1);
    rb_define_module_function(mAPI, "sqlite3_column_decltype16", _wrap_sqlite3_column_decltype16, -1);
    rb_define_module_function(mAPI, "sqlite3_step", _wrap_sqlite3_step, -1);
    rb_define_module_function(mAPI, "sqlite3_data_count", _wrap_sqlite3_data_count, -1);
    rb_define_module_function(mAPI, "sqlite3_column_blob", _wrap_sqlite3_column_blob, -1);
    rb_define_module_function(mAPI, "sqlite3_column_bytes", _wrap_sqlite3_column_bytes, -1);
    rb_define_module_function(mAPI, "sqlite3_column_bytes16", _wrap_sqlite3_column_bytes16, -1);
    rb_define_module_function(mAPI, "sqlite3_column_double", _wrap_sqlite3_column_double, -1);
    rb_define_module_function(mAPI, "sqlite3_column_int", _wrap_sqlite3_column_int, -1);
    rb_define_module_function(mAPI, "sqlite3_column_int64", _wrap_sqlite3_column_int64, -1);
    rb_define_module_function(mAPI, "sqlite3_column_text", _wrap_sqlite3_column_text, -1);
    rb_define_module_function(mAPI, "sqlite3_column_text16", _wrap_sqlite3_column_text16, -1);
    rb_define_module_function(mAPI, "sqlite3_column_type", _wrap_sqlite3_column_type, -1);
    rb_define_module_function(mAPI, "sqlite3_finalize", _wrap_sqlite3_finalize, -1);
    rb_define_module_function(mAPI, "sqlite3_reset", _wrap_sqlite3_reset, -1);
    rb_define_module_function(mAPI, "sqlite3_create_function", _wrap_sqlite3_create_function, -1);
    rb_define_module_function(mAPI, "sqlite3_create_function16", _wrap_sqlite3_create_function16, -1);
    rb_define_module_function(mAPI, "sqlite3_aggregate_count", _wrap_sqlite3_aggregate_count, -1);
    rb_define_module_function(mAPI, "sqlite3_value_blob", _wrap_sqlite3_value_blob, -1);
    rb_define_module_function(mAPI, "sqlite3_value_bytes", _wrap_sqlite3_value_bytes, -1);
    rb_define_module_function(mAPI, "sqlite3_value_bytes16", _wrap_sqlite3_value_bytes16, -1);
    rb_define_module_function(mAPI, "sqlite3_value_double", _wrap_sqlite3_value_double, -1);
    rb_define_module_function(mAPI, "sqlite3_value_int", _wrap_sqlite3_value_int, -1);
    rb_define_module_function(mAPI, "sqlite3_value_int64", _wrap_sqlite3_value_int64, -1);
    rb_define_module_function(mAPI, "sqlite3_value_text", _wrap_sqlite3_value_text, -1);
    rb_define_module_function(mAPI, "sqlite3_value_text16", _wrap_sqlite3_value_text16, -1);
    rb_define_module_function(mAPI, "sqlite3_value_text16le", _wrap_sqlite3_value_text16le, -1);
    rb_define_module_function(mAPI, "sqlite3_value_text16be", _wrap_sqlite3_value_text16be, -1);
    rb_define_module_function(mAPI, "sqlite3_value_type", _wrap_sqlite3_value_type, -1);
    rb_define_module_function(mAPI, "sqlite3_result_blob", _wrap_sqlite3_result_blob, -1);
    rb_define_module_function(mAPI, "sqlite3_result_double", _wrap_sqlite3_result_double, -1);
    rb_define_module_function(mAPI, "sqlite3_result_error", _wrap_sqlite3_result_error, -1);
    rb_define_module_function(mAPI, "sqlite3_result_error16", _wrap_sqlite3_result_error16, -1);
    rb_define_module_function(mAPI, "sqlite3_result_int", _wrap_sqlite3_result_int, -1);
    rb_define_module_function(mAPI, "sqlite3_result_int64", _wrap_sqlite3_result_int64, -1);
    rb_define_module_function(mAPI, "sqlite3_result_text", _wrap_sqlite3_result_text, -1);
    rb_define_module_function(mAPI, "sqlite3_result_text16", _wrap_sqlite3_result_text16, -1);
    rb_define_module_function(mAPI, "sqlite3_result_text16le", _wrap_sqlite3_result_text16le, -1);
    rb_define_module_function(mAPI, "sqlite3_result_text16be", _wrap_sqlite3_result_text16be, -1);
    rb_define_module_function(mAPI, "sqlite3_result_value", _wrap_sqlite3_result_value, -1);
    rb_define_module_function(mAPI, "sqlite3_aggregate_context", _wrap_sqlite3_aggregate_context, -1);
}

#endif //0