#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <math.h>
#include <complex.h>
#include "gwion_util.h"
#include "gwion_ast.h"
#include "gwion_env.h"
#include "vm.h"
#include "instr.h"
#include "object.h"
#include "gwion.h"
#include "plug.h"
#include "operator.h"
#include "import.h"
#include "array.h"
#include "gwi.h"

m_int o_fileio_file;

static DTOR(fileio_dtor) {
  if(IO_FILE(o))
    fclose(IO_FILE(o));
}

static DTOR(static_fileio_dtor) {
  IO_FILE(o) = NULL;
}

#define describe_2file(name, type, offset, pop, format, ...)  \
static INSTR(name##_to_file) {\
  POP_REG(shred, SZ_INT)                                      \
  const type lhs = *(type*)REG(-offset);                      \
  const M_Object o = *(M_Object*)REG(0);                      \
  if(!o) Except(shred, "NullPtrException")                    \
  _release(o, shred);                                         \
  if(!IO_FILE(o)) Except(shred, "EmptyFileException")         \
  fprintf(IO_FILE(o), format, __VA_ARGS__);                   \
  pop;                                                        \
  *(M_Object*)REG(-SZ_INT) = o;                               \
}
describe_2file(int, m_int, SZ_INT,, "%" INT_F "", lhs)
describe_2file(float, m_float, SZ_FLOAT,
 PUSH_REG(shred, SZ_INT - SZ_FLOAT), "%f", lhs)
describe_2file(string, M_Object, SZ_INT,
  release(lhs, shred), "%s", lhs ? STRING(lhs) : NULL)
describe_2file(object, M_Object, SZ_INT,
  release(lhs, shred), "%p", (void*)lhs)
describe_2file(complex, m_complex, SZ_COMPLEX,
  PUSH_REG(shred, SZ_INT - SZ_COMPLEX), "#(%f, %f)", creal(lhs), cimag(lhs))
describe_2file(polar, m_complex, SZ_COMPLEX,
  PUSH_REG(shred, SZ_INT - SZ_COMPLEX), "%%(%f, %f)", creal(lhs), cimag(lhs)/M_PI)
describe_2file(vec3, m_vec3, SZ_VEC3,
  PUSH_REG(shred, SZ_INT - SZ_VEC3), "@(%f, %f, %f)", lhs.x, lhs.y, lhs.z)
describe_2file(vec4, m_vec4, SZ_VEC4,
  POP_REG(shred, SZ_INT - SZ_VEC4), "@(%f, %f, %f, %f)", lhs.x, lhs.y, lhs.z, lhs.w)

static INSTR(file_to_int) {
  POP_REG(shred, SZ_INT)
  m_int* ret = *(m_int**)REG(0);
  const M_Object o = *(M_Object*)REG(-SZ_INT);
  if(!o)
    Except(shred, "EmptyFileException");
  _release(o, shred);
  if(IO_FILE(o)) {
    if(fscanf(IO_FILE(o), "%"INT_F, ret) < 0)
      Except(shred, "FileReadException");
    *(m_uint*)REG(-SZ_INT) = *ret;
  } else
    Except(shred, "EmptyFileException");
}

static INSTR(file_to_float) {
  POP_REG(shred, SZ_INT)
  float ret;
  const M_Object o = *(M_Object*)REG(-SZ_INT);
  if(!o)
    Except(shred, "EmptyFileException");
  _release(o, shred);
  if(IO_FILE(o)) {
    if(fscanf(IO_FILE(o), "%f", &ret) < 0)
      Except(shred, "FileReadException");                                     // LCOV_EXCL_LINE
    *(m_float*)REG(- SZ_FLOAT) = (**(m_float**)REG(0) = ret);
  } else
    Except(shred, "EmptyFileException");
  POP_REG(shred, SZ_FLOAT)
}

static INSTR(file_to_string) {
  POP_REG(shred, SZ_INT)
  const M_Object o    = *(M_Object*)REG(- SZ_INT);
  const M_Object s    = *(M_Object*)REG(0);
  if(!o)
    Except(shred, "EmptyFileException");
  if(!s)
    Except(shred, "EmptyStringException");
  if(IO_FILE(o)) {
    char c[1025];
    if(fscanf(IO_FILE(o), "%1024s", c) < 0)
      Except(shred, "FileReadException");                                     // LCOV_EXCL_LINE
    STRING(s) = s_name(insert_symbol(shred->info->vm->gwion->st, c));
    *(M_Object*)REG(- SZ_INT) = s;
  }
  _release(o, shred);
  _release(s, shred);
}

static MFUN(file_nl) {
  *(m_uint*)RETURN = fwrite("\n",  strlen("\n"), 1, IO_FILE(o));
}

static MFUN(file_open) {
  const M_Object lhs = *(M_Object*)MEM(SZ_INT * 2);
  const M_Object rhs = *(M_Object*)MEM(SZ_INT);
  if(!lhs || !rhs)
    Except(shred, "invalid arguments to FileIO.open()");
  const m_str filename = STRING(rhs);
  const m_str mode = STRING(lhs);
  release(rhs, shred);
  release(lhs, shred);
  if(IO_FILE(o)) {
    fclose(IO_FILE(o));
    IO_FILE(o) = NULL;
  }
  IO_FILE(o) = fopen(filename, mode);
  *(m_uint*)RETURN = IO_FILE(o) ? 1 : 0;
}

static MFUN(file_close) {
  if(IO_FILE(o)) {
    fclose(IO_FILE(o));
    IO_FILE(o) = NULL;
    *(m_uint*)RETURN = 1;
  } else
    *(m_uint*)RETURN = !IO_FILE(o) ? 1 : 0;
}

static MFUN(file_fileno) {
  FILE* file = IO_FILE(o);
  *(m_int*)RETURN = file ? fileno(file) : -1;
}

static SFUN(file_remove) {
  const M_Object obj = *(M_Object*)MEM(0);
  if(!obj)
    return;
  _release(obj, shred);
  *(m_uint*)RETURN = remove(STRING(*(M_Object*)MEM(0)));
}

static SFUN(file_list) {
  struct dirent **namelist;
  const M_Object obj = *(M_Object*)MEM(0);
  if(!obj)
    return;
  const m_str str = STRING(obj);
  _release(obj, shred);
  if(!str)
    return;
  const m_int n = scandir(str, &namelist, NULL, alphasort);
  if(n < 0) {
    *(m_uint*)RETURN = 0;
    return;
  }
  const Type t = array_type(shred->info->vm->gwion->env, shred->info->vm->gwion->type[et_string], 1);
  const M_Object ret = new_array(shred->info->vm->gwion->mp, t, n);
  vector_add(&shred->gc, (vtype)ret);
  for(m_uint i = 0; i < (m_uint)n; i++) {
    const M_Object string = new_string(shred->info->vm->gwion->mp, NULL, namelist[i]->d_name);
    m_vector_set(ARRAY(ret), i, &string);
    free(namelist[i]);
  }
  free(namelist);
  *(m_uint*)RETURN = (m_uint)ret;
}

GWION_IMPORT(fileio) {
  GWI_BB(gwi_class_ini(gwi, "FileIO", "Event"))
  gwi_class_xtor(gwi, NULL, fileio_dtor);

  // import vars
  gwi_item_ini(gwi, "@internal", "@file");
  o_fileio_file = gwi_item_end(gwi, ae_flag_none, NULL);
  GWI_BB(o_fileio_file)

  // import funcs
  gwi_func_ini(gwi, "int", "nl");
  GWI_BB(gwi_func_end(gwi, file_nl, ae_flag_none))
  gwi_func_ini(gwi, "int", "open");
  gwi_func_arg(gwi, "string", "filename");
  gwi_func_arg(gwi, "string", "mode");
  GWI_BB(gwi_func_end(gwi, file_open, ae_flag_none))
  gwi_func_ini(gwi, "int", "close");
  GWI_BB(gwi_func_end(gwi, file_close, ae_flag_none))
  gwi_func_ini(gwi, "int", "fileno");
  GWI_BB(gwi_func_end(gwi, file_fileno, ae_flag_none))
  gwi_func_ini(gwi, "int", "remove");
  gwi_func_arg(gwi, "string", "filename");
  GWI_BB(gwi_func_end(gwi, file_remove, ae_flag_static))
  gwi_func_ini(gwi, "string[]", "list");
  gwi_func_arg(gwi, "string", "filename");
  GWI_BB(gwi_func_end(gwi, file_list, ae_flag_static))

  GWI_BB(gwi_class_end(gwi))

  GWI_BB(gwi_oper_ini(gwi, "int",    "FileIO", "FileIO"))
  GWI_BB(gwi_oper_end(gwi, "=>", int_to_file))
  GWI_BB(gwi_oper_ini(gwi, "float",  "FileIO", "FileIO"))
  GWI_BB(gwi_oper_end(gwi, "=>", float_to_file))
  GWI_BB(gwi_oper_ini(gwi, "complex",  "FileIO", "FileIO"))
  GWI_BB(gwi_oper_end(gwi, "=>", complex_to_file))
  GWI_BB(gwi_oper_ini(gwi, "polar",  "FileIO", "FileIO"))
  GWI_BB(gwi_oper_end(gwi, "=>", polar_to_file))
  GWI_BB(gwi_oper_ini(gwi, "Vec3",  "FileIO", "FileIO"))
  GWI_BB(gwi_oper_end(gwi, "=>", vec3_to_file))
  GWI_BB(gwi_oper_ini(gwi, "Vec4",  "FileIO", "FileIO"))
  GWI_BB(gwi_oper_end(gwi, "=>", vec4_to_file))
  GWI_BB(gwi_oper_ini(gwi,"string", "FileIO", "FileIO"))
  GWI_BB(gwi_oper_end(gwi, "=>", string_to_file))
  GWI_BB(gwi_oper_ini(gwi,"Object", "FileIO", "FileIO"))
  GWI_BB(gwi_oper_end(gwi, "=>", object_to_file))
  GWI_BB(gwi_oper_ini(gwi,"@null",  "FileIO", "FileIO"))
  GWI_BB(gwi_oper_end(gwi, "=>", object_to_file))
  GWI_BB(gwi_oper_ini(gwi, "FileIO", "string", "string"))
  GWI_BB(gwi_oper_add(gwi, opck_const_rhs))
  GWI_BB(gwi_oper_end(gwi, "=>", file_to_string))
  GWI_BB(gwi_oper_ini(gwi, "FileIO", "int",    "int"))
  GWI_BB(gwi_oper_add(gwi, opck_rassign))
  GWI_BB(gwi_oper_end(gwi, "=>", file_to_int))
  GWI_BB(gwi_oper_ini(gwi, "FileIO", "float",  "float"))
  GWI_BB(gwi_oper_add(gwi, opck_rassign))
  GWI_BB(gwi_oper_end(gwi, "=>", file_to_float))

  const Type t_cout = gwi_class_ini(gwi, "@Cout", "FileIO");
  gwi_class_xtor(gwi,  NULL, static_fileio_dtor);
  GWI_BB(gwi_class_end(gwi))

  const Type t_cerr = gwi_class_ini(gwi, "@Cerr", "FileIO");
  gwi_class_xtor(gwi,  NULL, static_fileio_dtor);
  GWI_BB(gwi_class_end(gwi))

  const Type t_cin = gwi_class_ini(gwi, "@Cin", "FileIO");
  gwi_class_xtor(gwi, NULL, static_fileio_dtor);
  GWI_BB(gwi_class_end(gwi))

  const M_Object gw_cin = new_object(gwi->gwion->mp, NULL, t_cin);
  IO_FILE(gw_cin) = stdin;
  EV_SHREDS(gw_cin) = new_vector(gwi->gwion->mp);
  const M_Object gw_cout = new_object(gwi->gwion->mp, NULL, t_cout);
  IO_FILE(gw_cout) = stdout;
  EV_SHREDS(gw_cout) = new_vector(gwi->gwion->mp);
  const M_Object gw_cerr = new_object(gwi->gwion->mp, NULL, t_cerr);
  IO_FILE(gw_cerr) = stderr;
  EV_SHREDS(gw_cerr) = new_vector(gwi->gwion->mp);
  gwi_item_ini(gwi, "FileIO", "cin");
  gwi_item_end(gwi, ae_flag_const, gw_cin);
  gwi_item_ini(gwi, "FileIO", "cout");
  gwi_item_end(gwi, ae_flag_const, gw_cout);
  gwi_item_ini(gwi, "FileIO", "cerr");
  gwi_item_end(gwi, ae_flag_const, gw_cerr);
  return GW_OK;
}

#ifdef JIT
#include "ctrl/fileio.h"
#include "code/fileio.h"
#endif

