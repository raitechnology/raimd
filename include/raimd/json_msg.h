#ifndef __rai_raimd__json_msg_h__
#define __rai_raimd__json_msg_h__

#include <raimd/md_msg.h>
#include <raimd/json.h>

namespace rai {
namespace md {

struct JsonParser;
struct JsonObject;

struct JsonMsg : public MDMsg {
  /* used by unpack() to alloc in MDMsgMem */
  void * operator new( size_t sz, void *ptr ) { return ptr; }

  JsonValue * js; /* any json atom:  object, string, array, number */

  JsonMsg( void *bb,  size_t off,  size_t end,  MDDict *d,  MDMsgMem *m )
    : MDMsg( bb, off, end, d, m ), js( 0 ) {}

  virtual const char *get_proto_string( void ) final;
  virtual int get_sub_msg( MDReference &mref, MDMsg *&msg ) final;
  virtual int get_reference( MDReference &mref ) final;
  virtual int get_field_iter( MDFieldIter *&iter ) final;
  virtual int get_array_ref( MDReference &mref, size_t i,
                             MDReference &aref ) final;
  static int value_to_ref( MDReference &mref,  JsonValue &x );
  static bool is_jsonmsg( void *bb,  size_t off,  size_t end,  uint32_t h );
  /* unpack only objects delimited by '{' '}' */
  static JsonMsg *unpack( void *bb,  size_t off,  size_t end,  uint32_t h,
                          MDDict *d,  MDMsgMem *m );
  /* try to parse any json:  array, string, number */
  static JsonMsg *unpack_any( void *bb,  size_t off,  size_t end,  uint32_t h,
                              MDDict *d,  MDMsgMem *m );
  static void init_auto_unpack( void );
};

struct JsonFieldIter : public MDFieldIter {
  JsonMsg    & me;
  JsonObject & obj;

  void * operator new( size_t sz, void *ptr ) { return ptr; }

  JsonFieldIter( JsonMsg &m,  JsonObject &o ) : MDFieldIter( m ),
    me( m ), obj( o ) {}

  virtual int get_name( MDName &name ) final;
  virtual int get_reference( MDReference &mref ) final;
  virtual int find( const char *name ) final;
  virtual int first( void ) final;
  virtual int next( void ) final;
};

}
}

#endif
