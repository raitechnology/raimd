#ifndef __rai_raimd__md_field_iter_h__
#define __rai_raimd__md_field_iter_h__

#include <raimd/md_types.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

struct tm;
void md_localtime( time_t t, struct tm *tmbuf );
void md_gmtime( time_t t, struct tm *tmbuf );
time_t md_mktime( struct tm *tmbuf );
time_t md_timegm( struct tm *tmbuf );

struct MDMsg_s;
typedef struct MDFieldIter_s { /* generic field iterator */
  struct MDMsg_s * field_iter_msg;
  size_t           field_start,   /* geom of a field, if msg is byte blob */
                   field_end,
                   field_index;
} MDFieldIter_t;

MDFieldIter_t *md_field_iter_copy( MDFieldIter_t *iter );
int md_field_iter_get_name( MDFieldIter_t *iter,  MDName_t *name );
int md_field_iter_copy_name( MDFieldIter_t *iter,  char *name,  size_t *name_len,  MDFid *fid );
int md_field_iter_get_reference( MDFieldIter_t *iter,  MDReference_t *mref );
int md_field_iter_get_hint_reference( MDFieldIter_t *iter,  MDReference_t *mref );
int md_field_iter_get_enum( MDFieldIter_t *iter,  MDReference_t *mref, MDEnum_t *enu );
int md_field_iter_find( MDFieldIter_t *iter,  const char *name,  size_t name_len, MDReference_t *mref );
int md_field_iter_first( MDFieldIter_t *iter );
int md_field_iter_next( MDFieldIter_t *iter );
int md_field_iter_update( MDFieldIter_t *iter,  MDReference_t *mref );
size_t md_field_iter_fname_string( MDFieldIter_t *iter, char *fname_buf,  size_t *fname_len );
int md_field_iter_print( MDFieldIter_t *iter, MDOutput_t *out );
int md_field_iter_print_fmt( MDFieldIter_t *iter, MDOutput_t *out, int indent_newline, const char *fname_fmt, const char *type_fmt );

typedef struct MDFieldReader_s MDFieldReader_t;
typedef struct MDMsg_s MDMsg_t;
MDFieldReader_t *md_msg_get_field_reader( MDMsg_t *m );
bool md_field_reader_find( MDFieldReader_t *rd, const char *fn,  size_t flen );
bool md_field_reader_first( MDFieldReader_t *rd, MDName_t *n );
bool md_field_reader_next( MDFieldReader_t *rd, MDName_t *n );
bool md_field_reader_get_uint( MDFieldReader_t *rd, void *u, size_t ulen );
bool md_field_reader_get_int( MDFieldReader_t *rd, void *u, size_t ulen );
bool md_field_reader_get_real( MDFieldReader_t *rd, void *r, size_t rlen );
bool md_field_reader_get_string( MDFieldReader_t *rd, char **buf,  size_t *len );
bool md_field_reader_get_opaque( MDFieldReader_t *rd, void **buf,  size_t *len );
bool md_field_reader_get_string_buf( MDFieldReader_t *rd, char *buf, size_t buflen, size_t *blen );
bool md_field_reader_get_time( MDFieldReader_t *rd,  MDTime_t *time );
bool md_field_reader_get_date( MDFieldReader_t *rd,  MDDate_t *date );
bool md_field_reader_get_decimal( MDFieldReader_t *rd,  MDDecimal_t *dec );
bool md_field_reader_get_sub_msg( MDFieldReader_t *rd,  MDMsg_t **msg );

#ifdef __cplusplus
}

namespace rai {
namespace md {

struct MDFieldIter : public MDFieldIter_s { /* generic field iterator */
  MDMsg & iter_msg( void ) const noexcept;
  MDFieldIter( MDMsg &m ) noexcept;
  void dup_iter( MDFieldIter &i ) noexcept;
  virtual MDFieldIter *copy( void ) noexcept;
  virtual int get_name( MDName &name ) noexcept;
  virtual int copy_name( char *name,  size_t &name_len,  MDFid &fid ) noexcept;
  virtual int get_reference( MDReference &mref ) noexcept;
  virtual int get_hint_reference( MDReference &mref ) noexcept;
  virtual int get_enum( MDReference &mref, MDEnum &enu ) noexcept;
  virtual int find( const char *name,  size_t name_len,
                    MDReference &mref ) noexcept;
  int find( const char *name,  MDReference &mref ) {
    return this->find( name, ::strlen( name ), mref );
  }
  virtual int first( void ) noexcept;
  virtual int next( void ) noexcept;
  virtual int update( MDReference &mref ) noexcept;

  /* escaped strings is used for json escaping */
  size_t fname_string( char *fname_buf,  size_t &fname_len ) noexcept;
  int print( MDOutput *out, int indent_newline, const char *fname_fmt,
             const char *type_fmt ) noexcept;
  int print( MDOutput *out ) {
    return this->print( out, 1, "%-18s : " /* fname fmt */,
                                "%-10s %3d : " /* type fmt */);
  }
};

struct MDFieldReader { /* iterator but w bool return with err field */
  MDFieldIter * iter;
  MDReference   mref;
  int           err;

  void * operator new( size_t, void *ptr ) { return ptr; }
  MDFieldReader( MDMsg &m ) noexcept;
  bool find( const char *fname,  size_t fnamelen ) noexcept;
  bool find( const char *fname ) {
    return this->find( fname, ::strlen( fname ) );
  }
  bool name( MDName &n ) noexcept;
  bool first( void ) noexcept;
  bool next( void ) noexcept;
  bool first( MDName &n ) {
    return this->first() && this->name( n );
  }
  bool next( MDName &n ) {
    return this->next() && this->name( n );
  }
  MDType type( void ) noexcept;
  bool get_value( void *val,  size_t len,  MDType t ) noexcept;
  bool get_array_count( size_t &cnt ) noexcept;
  bool get_array_value( void *val,  size_t cnt,  size_t elsz,
                        MDType t ) noexcept;
  template <class I>
  bool get_int( I &ival ) {
    return this->get_value( &ival, sizeof( I ), MD_INT );
  }
  template <class I>
  bool get_uint( I &ival ) {
    return this->get_value( &ival, sizeof( I ), MD_UINT );
  }
  template <class F>
  bool get_real( F &fval ) {
    return this->get_value( &fval, sizeof( F ), MD_REAL );
  }
  bool get_string( char *&buf,  size_t &len ) noexcept;
  bool get_opaque( void *&buf,  size_t &len ) noexcept;
  bool get_string( char *buf, size_t buflen, size_t &blen ) noexcept;
  bool get_time( MDTime &time ) {
    return this->get_value( &time, sizeof( MDTime ), MD_TIME );
  }
  bool get_date( MDDate &date ) {
    return this->get_value( &date, sizeof( MDDate ), MD_DATE );
  }
  bool get_decimal( MDDecimal &dec ) {
    return this->get_value( &dec, sizeof( MDDecimal ), MD_DECIMAL );
  }
  bool get_sub_msg( MDMsg *&msg ) noexcept;
  bool string_eq( const char *str,  size_t len ) {
    char * buf;
    size_t buflen;
    if ( ! this->get_string( buf, buflen ) )
      return false;
    if ( buflen > 0 && buf[ buflen - 1 ] == '\0' )
      buflen--;
    return buflen == len && ::memcmp( str, buf, buflen ) == 0;
  }
  template <class I>
  bool int_eq( I i ) {
    I ival;
    return this->get_value( &ival, sizeof( I ), MD_INT ) && ival == i;
  }
  template <class I>
  bool uint_eq( I u ) {
    I uval;
    return this->get_value( &uval, sizeof( I ), MD_UINT ) && uval == u;
  }
};

struct MDIterMap {
  const char * fname;
  size_t       fname_len;
  void       * fptr;
  size_t       fsize, elem_fsize;
  MDType       ftype, elem_ftype;
  uint32_t   * elem_count;

  MDIterMap() : fname( 0 ), fptr( 0 ), fsize( 0 ), elem_fsize( 0 ),
    ftype( MD_NODATA ), elem_ftype( MD_NODATA ), elem_count( 0 ) {}

  void elem( const char *fn,  void *fp,  size_t sz,  MDType ft ) {
    this->ftype     = ft; 
    this->fname     = fn;
    this->fname_len = ::strlen( fn );
    this->fptr      = fp;
    this->fsize     = sz;
  }
  void string( const char *fn,  void *fp,  size_t sz ) {
    this->elem( fn, fp, sz, MD_STRING );
  }
  void opaque( const char *fn,  void *fp,  size_t sz ) {
    this->elem( fn, fp, sz, MD_OPAQUE );
  }
  void uint( const char *fn,  void *fp,  size_t sz ) {
    this->elem( fn, fp, sz, MD_UINT );
  }
  template <class I>
  void uint( const char *fn,  I &ival ) {
    this->elem( fn, &ival, sizeof( I ), MD_UINT );
  }
  void sint( const char *fn,  void *fp,  size_t sz ) {
    this->elem( fn, fp, sz, MD_INT );
  }
  void boolean( const char *fn,  bool &bval ) {
    this->elem( fn, &bval, sizeof( bool ), MD_BOOLEAN );
  }
  template <class I>
  void sint( const char *fn,  I &ival ) {
    this->elem( fn, &ival, sizeof( I ), MD_INT );
  }
  void decimal( const char *fn,  MDDecimal &dec ) {
    this->elem( fn, &dec, sizeof( MDDecimal ), MD_DECIMAL );
  }
  void array( const char *fn,  void *fp,  uint32_t *elcnt,
              size_t arsz,  size_t elsz,  MDType eltp ) {
    this->elem( fn, fp, arsz, MD_ARRAY );
    this->elem_fsize = elsz;
    this->elem_ftype = eltp;
    this->elem_count = elcnt;
  }
  void uint_array( const char *fn,  void *fp,  uint32_t *elcnt,
                   size_t arsz,  size_t elsz ) {
    this->array( fn, fp, elcnt, arsz, elsz, MD_UINT );
  }
  void sint_array( const char *fn,  void *fp,  uint32_t *elcnt,
                   size_t arsz,  size_t elsz ) {
    this->array( fn, fp, elcnt, arsz, elsz, MD_INT );
  }
  void string_array( const char *fn,  void *fp,  uint32_t *elcnt,
                     size_t arsz,  size_t elsz ) {
    this->array( fn, fp, elcnt, arsz, elsz, MD_STRING );
  }
  void opaque_array( const char *fn,  void *fp,  uint32_t *elcnt,
                     size_t arsz,  size_t elsz ) {
    this->array( fn, fp, elcnt, arsz, elsz, MD_OPAQUE );
  }
  bool index_array( size_t i,  void *&ptr,  size_t &sz ) noexcept;
  bool copy_string( size_t i,  MDReference &mref ) noexcept;
  bool copy_uint( size_t i,  MDReference &mref ) noexcept;
  bool copy_sint( size_t i,  MDReference &mref ) noexcept;
  bool copy_decimal( size_t i,  MDReference &mref ) noexcept;
  bool copy_array( MDMsg &msg,  MDReference &mref ) noexcept;

  static size_t get_map( MDMsg &msg,  MDIterMap *map,  size_t n,
                         MDFieldIter *iter = NULL ) noexcept;
};

}
} // namespace rai

#endif
#endif
