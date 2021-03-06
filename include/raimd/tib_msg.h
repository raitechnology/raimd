#ifndef __rai_raimd__tib_msg_h__
#define __rai_raimd__tib_msg_h__

#include <raimd/md_msg.h>

namespace rai {
namespace md {

static const uint32_t RAIMSG_TYPE_ID = 0x07344064,
                      TIBMSG_TYPE_ID = RAIMSG_TYPE_ID;

struct TibMsg : public MDMsg {
  /* used by unpack() to alloc in MDMsgMem */
  void * operator new( size_t, void *ptr ) { return ptr; }

  TibMsg( void *bb,  size_t off,  size_t len,  MDDict *d,  MDMsgMem *m )
    : MDMsg( bb, off, len, d, m ) {}

  virtual const char *get_proto_string( void ) noexcept final;
  virtual uint32_t get_type_id( void ) noexcept final;
  virtual int get_sub_msg( MDReference &mref, MDMsg *&msg ) noexcept final;
  virtual int get_field_iter( MDFieldIter *&iter ) noexcept final;
  /* convert tib decimal to md decimal */
  static bool set_decimal( MDDecimal &dec,  double val,
                           uint8_t tib_hint ) noexcept;
  static bool is_tibmsg( void *bb,  size_t off,  size_t len,
                         uint32_t h ) noexcept;
  static TibMsg *unpack( void *bb,  size_t off,  size_t len,  uint32_t h,
                         MDDict *d,  MDMsgMem *m ) noexcept;
  static void init_auto_unpack( void ) noexcept;
};

struct TibFieldIter : public MDFieldIter {
  uint32_t  size,         /* size of data */
            hint_size,    /* hint size */
            data_off;     /* offset of data */
  uint8_t   name_len,     /* length of field name */
            type,         /* tib field type */
            hint_type,    /* tib field hint type */
            decimal_hint; /* tib decimal field hint */
  MDDecimal dec;          /* temp storage for decimal */
  MDDate    date;         /* temp storage for date */
  MDTime    time;         /* temp storage for time */

  /* used by GetFieldIterator() to alloc in MDMsgMem */
  void * operator new( size_t, void *ptr ) { return ptr; }

  TibFieldIter( MDMsg &m ) : MDFieldIter( m ), size( 0 ), hint_size( 0 ),
    name_len( 0 ), type( 0 ), hint_type( 0 ) {}

  virtual int get_name( MDName &name ) noexcept final;
  virtual int get_reference( MDReference &mref ) noexcept final;
  virtual int get_hint_reference( MDReference &mref ) noexcept final;
  virtual int find( const char *name, size_t name_len,
                    MDReference &mref ) noexcept final;
  virtual int first( void ) noexcept final;
  virtual int next( void ) noexcept final;
  int unpack( void ) noexcept;
};

struct TibMsgWriter {
  uint8_t * buf;    /* output buffer */
  size_t    off,    /* index to end of data (+9 for hdr) */
            buflen; /* max length of a buf */

  TibMsgWriter( void *bb,  size_t len ) : buf( (uint8_t *) bb ), off( 0 ),
                                          buflen( len ) {}
  void reset( void ) {
    this->off = 0;
  }
  int append_ref( const char *fname,  size_t fname_len,
                  MDReference &mref ) noexcept;
  bool has_space( size_t len ) const {
    return this->off + 9 + len <= this->buflen;
  }

  size_t update_hdr( void ) {
    this->buf[ 0 ] = 0xce;
    this->buf[ 1 ] = 0x13;
    this->buf[ 2 ] = 0xaa;
    this->buf[ 3 ] = 0x1f;
    this->buf[ 4 ] = 1;
    this->buf[ 5 ] = ( this->off >> 24 ) & 0xffU;
    this->buf[ 6 ] = ( this->off >> 16 ) & 0xffU;
    this->buf[ 7 ] = ( this->off >> 8 ) & 0xffU;
    this->buf[ 8 ] = this->off & 0xffU;
    return this->off + 9; /* returns length of data in buffer */
  }

  template< class T >
  int append_type( const char *fname,  size_t fname_len,  T val,  MDType t ) {
    MDReference mref;
    mref.fptr    = (uint8_t *) (void *) &val;
    mref.fsize   = sizeof( val );
    mref.ftype   = t;
    mref.fendian = md_endian;
    return this->append_ref( fname, fname_len, mref );
  }

  template< class T >
  int append_int( const char *fname,  size_t fname_len,  T ival ) {
    return this->append_type( fname, fname_len, ival, MD_INT );
  }
  template< class T >
  int append_uint( const char *fname,  size_t fname_len,  T uval ) {
    return this->append_type( fname, fname_len, uval, MD_UINT );
  }
  template< class T >
  int append_real( const char *fname,  size_t fname_len,  T rval ) {
    return this->append_type( fname, fname_len, rval, MD_REAL );
  }

  int append_string( const char *fname,  size_t fname_len,
                     const char *str,  size_t len ) {
    MDReference mref;
    mref.fptr    = (uint8_t *) (void *) str;
    mref.fsize   = len;
    mref.ftype   = MD_STRING;
    mref.fendian = md_endian;
    return this->append_ref( fname, fname_len, mref );
  }

  int append_decimal( const char *fname,  size_t fname_len,
                      MDDecimal &dec ) noexcept;
  int append_time( const char *fname,  size_t fname_len,
                   MDTime &time ) noexcept;
  int append_date( const char *fname,  size_t fname_len,
                   MDDate &date ) noexcept;
};

}
} // namespace rai

#endif
