#ifndef __rai_raimd__md_msg_h__
#define __rai_raimd__md_msg_h__

#include <raimd/md_field_iter.h>

extern "C" {
const char *md_get_version( void );
void md_init_auto_unpack( void );
}

namespace rai {
namespace md {

struct MDMsgMem {
  static const size_t   MEM_CNT = 256 - 4;
  uint32_t mem_off; /* offset in mem[] where next is allocated */
#ifdef MD_REF_COUNT
  static const uint32_t NO_REF_COUNT = 0x7fffffffU;
  uint32_t ref_cnt; /* incremented when attached to a MDMsg */
  void incr_ref( void ) {
    if ( this->ref_cnt != NO_REF_COUNT )
      this->ref_cnt++;
  }
#else
  void incr_ref( void ) {}
#endif
  struct MemBlock {
    MemBlock * next;
    size_t     size;
    void     * mem[ MEM_CNT ];
  };
  MemBlock blk, * blk_ptr;

#ifdef MD_REF_COUNT
  void * operator new( size_t, void *ptr ) { return ptr; }
  void operator delete( void *ptr ) { ::free( ptr ); }
#endif
  /* ref count should be set when created dynamically,
   * this default is set to not release memory based on refs */
  MDMsgMem() {
    this->blk_ptr  = &this->blk;
    this->blk.next = &this->blk;
    this->blk.size = MEM_CNT;
    this->mem_off  = 0;
  }
  ~MDMsgMem() {
    this->reuse();
  }
  void error( void ) noexcept;

  void reuse( void ) {
    if ( this->blk_ptr != &this->blk )
      this->release();
    this->mem_off = 0;
  }
  void reset( MemBlock *blk,  uint32_t off ) noexcept;

  void *reuse_make( size_t size ) {
    size = this->align_size( size );
    if ( this->blk_ptr == &this->blk ||
         this->blk_ptr->next == &this->blk ) {
      if ( size <= this->blk_ptr->size ) {
        this->mem_off = size;
        return this->blk_ptr->mem;
      }
    }
    this->reuse();
    return this->alloc_slow( size );
  }
  static size_t align_size( size_t sz ) {
    return ( sz + sizeof( void * ) - 1 ) / sizeof( void * );
  }
  void alloc( size_t size,  void *ptr ) {
    void * next = this->mem_ptr();
    size = this->align_size( size );
    if ( size + (size_t) this->mem_off <= MEM_CNT )
      this->mem_off += (uint32_t) size;
    else
      next = this->alloc_slow( size );
    *(void **) ptr = next;
  }
  void *mem_ptr( void ) const {
    return &this->blk_ptr->mem[ this->mem_off ];
  }
  void *make( size_t size ) {
    void * next = this->mem_ptr();
    size = this->align_size( size );
    if ( size + (size_t) this->mem_off <= MEM_CNT ) {
      this->mem_off += (uint32_t) size;
      return next;
    }
    return this->alloc_slow( size );
  }
  char *str_make( size_t size ) {
    return (char *) this->make( size );
  }
  /* when out of static space, add mem block using malloc() */
  void *alloc_slow( size_t size ) noexcept;
  /* extend last alloc for more space */
  void extend( size_t old_size,  size_t new_size,  void *ptr ) noexcept;
  /* alloc for strings */
  char *stralloc( size_t len,  const char *str ) {
    char * s = NULL;
    if ( str != NULL ) {
      this->alloc( len + 1, &s );
      ::memcpy( s, str, len );
      s[ len ] = '\0';
    }
    return s;
  }
  void *memalloc( size_t len,  const void *mem ) {
    void * m = NULL;
    if ( len != 0 ) {
      this->alloc( len, &m );
      ::memcpy( m, mem, len );
    }
    return m;
  }
  void release( void ) noexcept; /* release alloced memory ) */
};

struct MDMsgMemSwap { /* push new local allocation for the current stack */
  MDMsgMem *& sav, * old;
  MDMsgMem    tmp;

  MDMsgMemSwap( MDMsgMem *&swap ) : sav( swap ), old( swap ) {
    swap = &this->tmp;
  }
  ~MDMsgMemSwap() {
    this->sav = this->old;
  }
};

struct MDDict;
struct MDMsg;

typedef bool (*md_is_msg_type_f)( void *bb,  size_t off,  size_t end,
                                  uint32_t h );
typedef MDMsg *(*md_msg_unpack_f)( void *bb,  size_t off,  size_t end,
                                   uint32_t h,  MDDict *d,  MDMsgMem &m );

struct MDMatch { /* match msg features in the header */
  uint8_t  off,       /* offset of feature */
           len,       /* length of feature match */
           hint_size, /* an external hint[] words */
           ftype;
  uint8_t  buf[ 4 ];  /* the values to match against the offset */
  uint32_t hint[ 2 ]; /* external hints */

  md_is_msg_type_f is_msg_type; /* test whether msg matches */
  md_msg_unpack_f  unpack;      /* wrap the msg in a decoder */
  const char     * name;
};

struct MDMatchGroup {
  MDMatch ** matches;     /* matches at this offset */
  uint8_t    off, haslen; /* the offset */
  uint16_t   count;       /* the count of matches[] */
  uint8_t    xoff[ 256 ]; /* the values at off which are valid */
    /* uint8_t x = xoff[ msgdata[ off ] ]
       MDMatch *match = x > 0 ? matches[ x - 1 ] : NULL;
       if ( match != NULL && match->is_msg_type( msgdata ) )
         match->unpack( msgdata ) */
  void * operator new( size_t, void *ptr ) { return ptr; }
  void operator delete( void *ptr ) { ::free( ptr ); }

  MDMatchGroup() : matches( 0 ), off( 0 ), haslen( 0 ), count( 0 ) {
    ::memset( this->xoff, 0, sizeof( this->xoff ) );
  }
  /* add a msg matcher to the match group */
  void add_match( MDMatch &ma ) noexcept;
  /* check that if msg has a magic, that the char at offset matches the magic */
  MDMsg * match( void *bb,  size_t off,  size_t end,  uint32_t h,
                 MDDict *d,  MDMsgMem &m ) {
    uint16_t i;
    /* if char matches, i > 0 */
    if ( this->haslen ) {
      if ( off + this->off >= end )
        return NULL;
      /* if i == 0, there is no match, otherwise start matching at i */
      i = this->xoff[ ((uint8_t *) bb)[ off + this->off ] ];
      if ( i == 0 )
        return NULL;
    }
    else {
      i = 1; /* start at head, there is no len for match group */
    }
    return this->match2( bb, off, end, h, d, m, i );
  }
  MDMsg * match2( void *bb,  size_t off,  size_t end,  uint32_t h,
                  MDDict *d,  MDMsgMem &m,  uint16_t i ) noexcept;
  MDMatch * is_msg_type( void *bb,  size_t off,  size_t end,
                         uint32_t h ) {
    uint16_t i;
    /* if char matches, i > 0 */
    if ( this->haslen ) {
      if ( off + this->off >= end )
        return NULL;
      /* if i == 0, there is no match, otherwise start matching at i */
      i = this->xoff[ ((uint8_t *) bb)[ off + this->off ] ];
      if ( i == 0 )
        return NULL;
    }
    else {
      i = 1; /* start at head, there is no len for match group */
    }
    return this->is_msg_type2( bb, off, end, h, i );
  }
  MDMatch * is_msg_type2( void *bb,  size_t off,  size_t end,  uint32_t h,
                          uint16_t i ) noexcept;
};

struct MDMsg {
  void     * msg_buf;
  size_t     msg_off,    /* offset where message starts */
             msg_end;    /* end offset of msg_buf */
  MDDict   * dict;
  MDMsgMem * mem; /* ref count increment in unpack() and get_sub_msg()
                     ref count decrement in release() */
  void * operator new( size_t, void *ptr ) { return ptr; }
  void operator delete( void * ) { /*::free( ptr );*/ } /* do nothing */

  MDMsg( void *bb,  size_t off,  size_t end,  MDDict *d,  MDMsgMem &m )
    : msg_buf( bb ), msg_off( off ), msg_end( end ), dict( d ), mem( &m ) {}
  ~MDMsg() { this->release(); }

  static void add_match( MDMatch &ma ) noexcept;
  /* Unpack message and make available for extracting field values */
  static MDMsg *unpack( void *bb,  size_t off,  size_t end,  uint32_t h,
                        MDDict *d,  MDMsgMem &m ) noexcept;
  static uint32_t is_msg_type( void *b,  size_t off,  size_t end,
                               uint32_t h ) noexcept;
  /* dereference msg mem */
  void release( void ) {
#ifdef MD_REF_COUNT
    if ( this->mem != NULL && this->mem->ref_cnt != MDMsgMem::NO_REF_COUNT ) {
      MDMsgMem *m = this->mem;
      this->mem = NULL; /* prevent multiple derefs */
      if ( --m->ref_cnt == 0 )
        delete m;
    }
#endif
  }
  /* Print message in 'TIB' format to output stream */
  void print( MDOutput *out ) {
    this->print( out, 1, "%-18s : " /* fname fmt */,
                         "%-10s %3d : " /* type fmt */);
  }
  /* Print message with printf style fmts */
  virtual void print( MDOutput *out,  int indent_newline,  const char *fname_fmt,
                      const char *type_fmt ) noexcept;
  /* Print buffer hex values */
  static void print_hex( MDOutput *out,  const void *msgBuf,
                         size_t offset,  size_t length ) noexcept;
  static MDMatch *first_match( uint32_t &i ) noexcept;
  static MDMatch *next_match( uint32_t &i ) noexcept;
  /* Used by field iterators to creae a sub message */
  virtual const char *get_proto_string( void ) noexcept;
  virtual uint32_t get_type_id( void ) noexcept;
  virtual int get_sub_msg( MDReference &mref, MDMsg *&msg,
                           MDFieldIter *iter ) noexcept;
  virtual int get_reference( MDReference &mref ) noexcept;
  virtual int get_field_iter( MDFieldIter *&iter ) noexcept;
  virtual int get_array_ref( MDReference &mref, size_t i,
                             MDReference &aref ) noexcept;
  virtual int msg_to_string( MDReference &mref,  char *&buf,  size_t &len ) noexcept;
  virtual int hash_to_string( MDReference &mref,  char *&buf,  size_t &len ) noexcept;
  virtual int set_to_string( MDReference &mref,  char *&buf,  size_t &len ) noexcept;
  virtual int zset_to_string( MDReference &mref,  char *&buf,  size_t &len ) noexcept;
  virtual int geo_to_string( MDReference &mref,  char *&buf,  size_t &len ) noexcept;
  virtual int hll_to_string( MDReference &mref,  char *&buf,  size_t &len ) noexcept;
  virtual int stream_to_string( MDReference &mref,  char *&buf,  size_t &len ) noexcept;
  virtual int xml_to_string( MDReference &mref,  char *&buf,  size_t &len ) noexcept;
  virtual int get_subject_string( MDReference &mref,  char *&buf, size_t &len ) noexcept;
  virtual int time_to_string( MDReference &mref,  char *&buf, size_t &len ) noexcept;
  virtual int array_to_string( MDReference &mref,  char *&buf, size_t &len ) noexcept;
  virtual int list_to_string( MDReference &mref,  char *&buf, size_t &len ) noexcept;

  int get_quoted_string( MDReference &mref,  char *&buf,  size_t &len ) noexcept;
  static size_t get_escaped_string_len( MDReference &mref,  const char *quotes ) noexcept;
  static size_t get_escaped_string_output( MDReference &mref,  const char *quotes,
                                           char *str ) noexcept;
  int get_escaped_string( MDReference &mref,  const char *quotes,
                          char *&buf,  size_t &len ) noexcept;
  int get_hex_string( MDReference &mref,  char *&buf,  size_t &len ) noexcept;
  int get_b64_string( MDReference &mref,  char *&buf,  size_t &len ) noexcept;
  int get_string( MDReference &mref,  char *&buf,  size_t &len ) noexcept;
  int concat_array_to_string( char **str,  size_t *k,  size_t num_entries,
                              size_t tot_len,  char *&buf,
                              size_t &len ) noexcept;
};

}
} // namespace rai

#endif
