#ifndef __rai_raimd__app_a_h__
#define __rai_raimd__app_a_h__

#include <raimd/md_types.h>

namespace rai {
namespace md {

/* col
 * 1     2         3    4           5        6       7         8
 * ACRO  DDE_ACRO  FID  RIPPLES_TO  MF_type  MF_len  RWF_type  RWF_len */

enum MF_type {
  MF_NONE         = 0,
  MF_ALPHANUMERIC = 1,
  MF_TIME         = 2,
  MF_DATE         = 3,
  MF_ENUMERATED   = 4,
  MF_INTEGER      = 5,
  MF_PRICE        = 6,
  MF_TIME_SECONDS = 7,
  MF_BINARY       = 8  /* binary + uint = mf encoded integer */
};

enum RWF_type {
  /* primitives */
  RWF_NONE         = 0,
  RWF_RSVD_1       = 1,
  RWF_RSVD_2       = 2,
  RWF_INT          = 3, /* int64, int32 (rare) */
  RWF_UINT         = 4, /* uint64 */
  RWF_FLOAT        = 5, /* float */
  RWF_DOUBLE       = 6, /* double */
  RWF_REAL         = 8, /* real64 */
  RWF_DATE         = 9, /* y/m/d */
  RWF_TIME         = 10, /* h:m:s */
  RWF_DATETIME     = 11, /* date + time */
  RWF_QOS          = 12, /* qos bits */
  RWF_ENUM         = 14, /* uint16 enum */
  RWF_ARRAY        = 15, /* primitive type array */
  RWF_BUFFER       = 16, /* len + opaque */
  RWF_ASCII_STRING = 17,
  RWF_UTF8_STRING  = 18,
  RWF_RMTES_STRING = 19, /* need rsslRMTESToUTF8 */

  /* sets */
  RWF_INT_1        = 64,
  RWF_UINT_1       = 65,
  RWF_INT_2        = 66,
  RWF_UINT_2       = 67,
  RWF_INT_4        = 68,
  RWF_UINT_4       = 69,
  RWF_INT_8        = 70,
  RWF_UINT_8       = 71,
  RWF_FLOAT_4      = 72,
  RWF_DOUBLE_8     = 73,
  RWF_REAL_4RB     = 74,
  RWF_REAL_8RB     = 75,
  RWF_DATE_4       = 76,
  RWF_TIME_3       = 77,
  RWF_TIME_5       = 78,
  RWF_DATETIME_7   = 79,
  RWF_DATETIME_9   = 80,
  RWF_DATETIME_11  = 81,
  RWF_DATETIME_12  = 82,
  RWF_TIME_7       = 83,
  RWF_TIME_8       = 84,

  /* containers */
  RWF_NO_DATA      = 128,
  RWF_MSG_KEY      = 129,
  RWF_OPAQUE       = 130,
  RWF_XML          = 131,
  RWF_FIELD_LIST   = 132,
  RWF_ELEMENT_LIST = 133,
  RWF_ANSI_PAGE    = 134,
  RWF_FILTER_LIST  = 135,
  RWF_VECTOR       = 136,
  RWF_MAP          = 137,
  RWF_SERIES       = 138,
  RWF_RSVD_139     = 139,
  RWF_RSVD_140     = 140,
  RWF_MSG          = 141,
  RWF_JSON         = 142
};

static inline int is_rwf_primitive( RWF_type t ) {
  return t >= RWF_INT && t <= RWF_RMTES_STRING;
}
static inline int is_rwf_set_primitive( RWF_type t ) {
  return t >= RWF_INT_1 && t <= RWF_TIME_8;
}
static inline int is_rwf_container( RWF_type t ) {
  return t >= RWF_NO_DATA  && t <= RWF_JSON &&
         t != RWF_RSVD_139 && t != RWF_RSVD_140;
}

enum AppATok {
  ATK_ERROR        = -2,
  ATK_EOF          = -1,
  ATK_IDENT        = 0,
  ATK_INT          = 1,
  ATK_INTEGER      = 2,
  ATK_ALPHANUMERIC = 3,
  ATK_ENUMERATED   = 4,
  ATK_TIME         = 5,
  ATK_DATE         = 6,
  ATK_PRICE        = 7,
  ATK_OPEN_PAREN   = 8,
  ATK_CLOSE_PAREN  = 9,
  ATK_REAL32       = 10,
  ATK_REAL64       = 11,
  ATK_RMTES_STRING = 12,
  ATK_UINT32       = 13,
  ATK_UINT64       = 14,
  ATK_ASCII_STRING = 15,
  ATK_BINARY       = 16,
  ATK_BUFFER       = 17,
  ATK_NULL         = 18,
  ATK_ENUM         = 19,
  ATK_TIME_SECONDS = 20,
  ATK_INT32        = 21,
  ATK_INT64        = 22,
  ATK_NONE         = 23,
  ATK_ARRAY        = 24,
  ATK_SERIES       = 25,
  ATK_ELEMENT_LIST = 26,
  ATK_VECTOR       = 27,
  ATK_MAP          = 28
};

struct AppAKeyword {
  const char * str;
  size_t       len;
  AppATok      tok;
};

struct MDDictBuild;
struct AppA : public DictParser {
  int     fid,
          length,
          enum_length,
          rwf_len;
  AppATok field_type,
          rwf_type;
  char    acro[ 256 ],
          dde_acro[ 256 ],
          ripples_to[ 256 ];

  void * operator new( size_t, void *ptr ) { return ptr; } 
  void operator delete( void *ptr ) { ::free( ptr ); } 

  AppA( const char *p,  int debug_flags )
      : DictParser( p, ATK_INT, ATK_IDENT, ATK_ERROR, debug_flags,
                    "RDM Field Dictionary" ) {
    this->clear_line();
  }
  ~AppA() {
    this->clear_line();
    this->close();
  }
  void clear_line( void ) noexcept;
  void set_ident( char *id_buf ) noexcept;
  AppATok consume( AppATok k,  size_t sz ) {
    return (AppATok) this->DictParser::consume_tok( k, sz );
  }
  AppATok consume( AppAKeyword &kw ) {
    return this->consume( kw.tok, kw.len );
  }
  AppATok consume_int( void ) {
    return (AppATok) this->DictParser::consume_int_tok();
  }
  AppATok consume_ident( void ) {
    return (AppATok) this->DictParser::consume_ident_tok();
  }
  AppATok consume_string( void ) {
    return (AppATok) this->DictParser::consume_string_tok();
  }
  AppATok get_token( void ) noexcept;
  bool match( AppAKeyword &kw ) {
    return this->DictParser::match( kw.str, kw.len );
  }
  void get_type_size( MDType &type,  uint32_t &size ) noexcept;

  static AppA * open_path( const char *path,  const char *filename,
                           int debug_flags ) noexcept;
  static int parse_path( MDDictBuild &dict_build,  const char *path,
                         const char *fn ) noexcept;
};

}
}
#endif

