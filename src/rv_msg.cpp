#include <stdio.h>
#include <time.h>
#include <zlib.h>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <raimd/rv_msg.h>
#include <raimd/tib_msg.h>
#include <raimd/tib_sass_msg.h>
#include <raimd/rwf_msg.h>
#include <raimd/md_dict.h>
#include <raimd/sass.h>

using namespace rai;
using namespace md;

extern "C" {
MDMsg_t * rv_msg_unpack( void *bb,  size_t off,  size_t end,  uint32_t h, 
                        MDDict_t *d,  MDMsgMem_t *m ) 
{   
  return RvMsg::unpack( bb, off, end, h, (MDDict *)d,  *(MDMsgMem *) m );
}
MDMsgWriter_t *
rv_msg_writer_create( MDMsgMem_t *mem,  MDDict_t *,
                      void *buf_ptr, size_t buf_sz )
{
  void * p = ((MDMsgMem *) mem)->make( sizeof( RvMsgWriter ) );
  return new ( p ) RvMsgWriter( *(MDMsgMem *) mem, buf_ptr, buf_sz );
}
}

static const char RvMsg_proto_string[] = "RVMSG";
const char *
RvMsg::get_proto_string( void ) noexcept
{
  return RvMsg_proto_string;
}

uint32_t
RvMsg::get_type_id( void ) noexcept
{
  return RVMSG_TYPE_ID;
}

static MDMatch rvmsg_match = {
  .name        = RvMsg_proto_string,
  .off         = 4,
  .len         = 4, /* cnt of buf[] */
  .hint_size   = 1, /* cnt of hint[] */
  .ftype       = (uint8_t) RVMSG_TYPE_ID,
  .buf         = { 0x99, 0x55, 0xee, 0xaa },
  .hint        = { RVMSG_TYPE_ID, 0 },
  .is_msg_type = RvMsg::is_rvmsg,
  .unpack      = RvMsg::unpack
};

bool
RvMsg::is_rvmsg( void *bb,  size_t off,  size_t end,  uint32_t ) noexcept
{
  uint32_t magic = 0;
  if ( off + 8 <= end )
    magic = get_u32<MD_BIG>( &((uint8_t *) bb)[ off + 4 ] );
  return magic == 0x9955eeaaU;
}

RvMsg *
RvMsg::unpack_rv( void *bb,  size_t off,  size_t end,  uint32_t,  MDDict *d,
                  MDMsgMem &m ) noexcept
{
  uint32_t magic    = get_u32<MD_BIG>( &((uint8_t *) bb)[ off + 4 ] );
  size_t   msg_size = get_u32<MD_BIG>( &((uint8_t *) bb)[ off ] );

  if ( magic != 0x9955eeaaU || msg_size < 8 )
    return NULL;
  size_t end2 = off + msg_size;
  void * ptr;
  if ( end2 > end )
    return NULL;
  m.incr_ref();
  m.alloc( sizeof( RvMsg ), &ptr );
  return new ( ptr ) RvMsg( bb, off, end2, d, m );
}

MDMsg *
RvMsg::unpack( void *bb,  size_t off,  size_t end,  uint32_t,  MDDict *d,
               MDMsgMem &m ) noexcept
{
  if ( off + 8 > end )
    return NULL;
  uint32_t magic    = get_u32<MD_BIG>( &((uint8_t *) bb)[ off + 4 ] );
  size_t   msg_size = get_u32<MD_BIG>( &((uint8_t *) bb)[ off ] );

  if ( magic != 0x9955eeaaU || msg_size < 8 )
    return NULL;
  /* check if another message is the first opaque field of the RvMsg */
  size_t off2 = off + 8,
         end2 = off + msg_size;
  if ( end2 > end )
    return NULL;
  MDMsg *msg = RvMsg::opaque_extract( (uint8_t *) bb, off2, end2, d, m );
  if ( msg == NULL ) {
    void * ptr;
    m.incr_ref();
    m.alloc( sizeof( RvMsg ), &ptr );
    msg = new ( ptr ) RvMsg( bb, off, end2, d, m );
  }
  return msg;
}

static inline bool
cmp_field( uint8_t *x,  size_t off,  uint8_t *y,  size_t ylen )
{
  for ( size_t i = 0; i < ylen; i++ )
    if ( x[ off + i ] != y[ i ] )
      return false;
  return true;
}

MDMsg *
RvMsg::opaque_extract( uint8_t *bb,  size_t off,  size_t end,  MDDict *d,
                       MDMsgMem &m ) noexcept
{
  /* quick filter of below fields */
  if ( off+19 > end || bb[ off ] < 7 || bb[ off ] > 8 || bb[ off + 1 ] != '_' )
    return NULL;

  static uint8_t tibrv_encap_TIBMSG[]= { 8, '_','T','I','B','M','S','G', 0 };
  static uint8_t tibrv_encap_QFORM[] = { 7, '_','Q','F','O','R','M', 0 };
  static uint8_t tibrv_encap_data[]  = { 7, '_','d','a','t','a','_', 0 };
  static uint8_t tibrv_encap_RAIMSG[]= { 8, '_','R','A','I','M','S','G', 0 };
  static uint8_t tibrv_encap_RWF[]   = { 8, '_','R','W','F','M','S','G', 0 };
  size_t i, fsize, szbytes = 0;
  bool is_tibmsg = false, is_qform = false, is_data = false, is_rwf = false;

  i = sizeof( tibrv_encap_TIBMSG );
  if ( ! (is_tibmsg = cmp_field( bb, off, tibrv_encap_TIBMSG, i )) ) {
    i = sizeof( tibrv_encap_QFORM );
    if ( ! (is_qform = cmp_field( bb, off, tibrv_encap_QFORM, i )) ) {
      i = sizeof( tibrv_encap_data );
      if ( ! (is_data = cmp_field( bb, off, tibrv_encap_data, i )) ) {
        i = sizeof( tibrv_encap_RAIMSG );
        if ( ! (is_tibmsg = cmp_field( bb, off, tibrv_encap_RAIMSG, i )) ) {
          i = sizeof( tibrv_encap_RWF );
          if ( ! (is_rwf = cmp_field( bb, off, tibrv_encap_RWF, i )) ) {
            return NULL;
          }
        }
      }
    }
  }

  off += i;
  if ( bb[ off++ ] != RV_OPAQUE )
    return NULL;

  fsize = bb[ off++ ];
  switch ( fsize ) {
    case RV_LONG_SIZE:
      fsize = get_u32<MD_BIG>( &bb[ off ] );
      szbytes = 4;
      break;
    case RV_SHORT_SIZE:
      fsize = get_u16<MD_BIG>( &bb[ off ] );
      szbytes = 2;
      break;
/*    case RV_TINY_SIZE:
      fsize = bb[ off ];
      szbytes = 1;
      break;*/
    default:
      break;
  }
  if ( fsize <= szbytes )
    return NULL;
  fsize -= szbytes;
  off   += szbytes;
  if ( off + fsize > end )
    return NULL;
  if ( ( is_tibmsg || is_data ) && TibMsg::is_tibmsg( bb, off, end, 0 ) )
    return TibMsg::unpack( bb, off, end, 0, d, m );
  if ( ( is_qform || is_data ) && TibSassMsg::is_tibsassmsg( bb, off, end, 0))
    return TibSassMsg::unpack( bb, off, end, 0, d, m );
  if ( is_rwf )
    return RwfMsg::unpack_message( bb, off, end, 0, d, m );
  return NULL;
}

void
RvMsg::init_auto_unpack( void ) noexcept
{
  MDMsg::add_match( rvmsg_match );
}

static const int rv_type_to_md_type[ 64 ] = {
    /*RV_BADDATA   =  0 */ 0, /* same as NODATA */
    /*RV_RVMSG     =  1 */ MD_MESSAGE,
    /*RV_SUBJECT   =  2 */ MD_SUBJECT,
    /*RV_DATETIME  =  3 */ MD_DATETIME,
                  /*  4 */ 0,
                  /*  5 */ 0,
                  /*  6 */ 0,
    /*RV_OPAQUE    =  7 */ MD_OPAQUE,
    /*RV_STRING    =  8 */ MD_STRING,
    /*RV_BOOLEAN   =  9 */ MD_BOOLEAN,
    /*RV_IPDATA    = 10 */ MD_IPDATA,
    /*RV_INT       = 11 */ MD_INT,
    /*RV_UINT      = 12 */ MD_UINT,
    /*RV_REAL      = 13 */ MD_REAL,
                  /* 14 */ 0,
                  /* 15 */ 0,
                  /* 16 */ 0,
    /* 17 */ 0, /* 18 */ 0, /* 19 */ 0, /* 20 */ 0, /* 21 */ 0,
    /* 22 */ 0, /* 23 */ 0, /* 24 */ 0, /* 25 */ 0, /* 26 */ 0,
    /* 27 */ 0, /* 28 */ 0, /* 29 */ 0, /* 30 */ 0, /* 31 */ 0,
    /*RV_ENCRYPTED = 32 */ MD_OPAQUE,
                  /* 33 */ 0,
    /*RV_ARRAY_I8  = 34 */ MD_ARRAY,
    /*RV_ARRAY_U8  = 35 */ MD_ARRAY,
    /*RV_ARRAY_I16 = 36 */ MD_ARRAY,
    /*RV_ARRAY_U16 = 37 */ MD_ARRAY,
    /*RV_ARRAY_I32 = 38 */ MD_ARRAY,
    /*RV_ARRAY_U32 = 39 */ MD_ARRAY,
    /*RV_ARRAY_I64 = 40 */ MD_ARRAY,
    /*RV_ARRAY_U64 = 41 */ MD_ARRAY,
                  /* 42 */ 0,
                  /* 43 */ 0,
    /*RV_ARRAY_F32 = 44 */ MD_ARRAY,
    /*RV_ARRAY_F64 = 45 */ MD_ARRAY,
                  /* 46 */ 0,
    /*RV_XML       = 47 */ MD_XML,
    /*RV_ARRAY_STR = 48 */ MD_ARRAY,
    /*RV_ARRAY_MSG = 49 */ MD_ARRAY
};


int
RvMsg::get_sub_msg( MDReference &mref, MDMsg *&msg,
                    MDFieldIter * ) noexcept
{
  uint8_t * bb    = (uint8_t *) this->msg_buf;
  size_t    start = (size_t) ( mref.fptr - bb );
  void    * ptr;

  this->mem->alloc( sizeof( RvMsg ), &ptr );
  msg = new ( ptr ) RvMsg( bb, start, start + mref.fsize, this->dict,
                           *this->mem );
  return 0;
}

int
RvMsg::get_field_iter( MDFieldIter *&iter ) noexcept
{
  void * ptr;
  this->mem->alloc( sizeof( RvFieldIter ), &ptr );
  iter = new ( ptr ) RvFieldIter( *this );
  return 0;
}


int
RvMsg::time_to_string( MDReference &mref,  char *&buf,  size_t &len ) noexcept
{
  if ( mref.ftype == MD_DATETIME && mref.fsize == 8 ) {
    const char *fmt = "%Y-%m-%d %H:%M:%S";
    uint64_t usec = get_uint<uint64_t>( mref.fptr, MD_BIG );
    time_t   sec  = usec >> 32;
    struct tm tm;
    md_gmtime( sec, &tm );
    char * gmt;
    this->mem->alloc( 32, &gmt );
    strftime( gmt, 32, fmt, &tm );
    /*154,979,000 Z*/
    char * p = &gmt[ ::strlen( gmt ) ],
         * e = &gmt[ 32 ];
    uint64_t nsec = (uint64_t) ( usec & 0xffffffffU ) * 1000;
    ::snprintf( p, e-p, "%" PRIu64 "Z", nsec + 1000000000 );
    *p = '.';
    buf = gmt;
    len = strlen( p ) + ( p - gmt );
    return 0;
  }
  return this->MDMsg::time_to_string( mref, buf, len );
}

int
RvMsg::xml_to_string( MDReference &mref,  char *&buf,  size_t &len ) noexcept
{
  if ( mref.ftype == MD_XML && mref.fsize > 0 ) {
    uint32_t doc_size = mref.fptr[ 0 ],
             szbytes  = 0;
    switch ( doc_size ) {
      case RV_LONG_SIZE:
        if ( 1 + 4 > mref.fsize )
          goto bad_bounds;
        doc_size = get_u32<MD_BIG>( &mref.fptr[ 1 ] );
        szbytes = 4;
        break;
      case RV_SHORT_SIZE:
        if ( 1 + 2 > mref.fsize )
          goto bad_bounds;
        doc_size = get_u16<MD_BIG>( &mref.fptr[ 1 ] );
        szbytes = 2;
        break;
      default:
        break;
    }
    z_stream zs;
    memset( &zs, 0, sizeof( zs ) );
    inflateInit( &zs );
    zs.avail_in  = mref.fsize - ( 1 + szbytes );
    zs.next_in   = (Bytef *) &mref.fptr[ 1 + szbytes ];
    zs.avail_out = doc_size - szbytes;
    zs.next_out  = (Bytef *) this->mem->make( zs.avail_out + 1 );
    len          = zs.avail_out;
    buf          = (char *) zs.next_out;
    int x = inflate( &zs, Z_FINISH );
    inflateEnd( &zs );
    buf[ len ]   = '\0';

    if ( x == Z_STREAM_ERROR )
      goto bad_bounds;

#if 0
    char tmp[ 32 ];
    ::snprintf( tmp, sizeof( tmp ), "[XML document: %u bytes]",
                doc_size - szbytes );
    len = ::strlen( tmp );
    buf = this->mem->stralloc( len, tmp );
#endif
    return 0;
  }
bad_bounds:;
  return this->MDMsg::xml_to_string( mref, buf, len );
}

MDFieldIter *
RvFieldIter::copy( void ) noexcept
{
  RvFieldIter *iter;
  void * ptr;
  this->iter_msg().mem->alloc( sizeof( RvFieldIter ), &ptr );
  iter = new ( ptr ) RvFieldIter( this->iter_msg() );
  this->dup_rv( *iter );
  return iter;
}

static inline void
get_rv_name( char *fname,  size_t fnamelen,  MDName &name )
{
  name.fid      = 0;
  name.fnamelen = fnamelen;
  if ( name.fnamelen > 0 ) {
    name.fname = fname;
    if ( fnamelen >= 3 && fname[ fnamelen - 3 ] == '\0' ) {
      name.fnamelen -= 2;
      name.fid = get_uint<uint16_t>( &fname[ name.fnamelen ], MD_BIG );
    }
  }
  else {
    name.fname = NULL;
  }
}

int
RvFieldIter::get_name( MDName &name ) noexcept
{
  get_rv_name( &((char *) this->iter_msg().msg_buf)[ this->field_start + 1 ],
               this->name_len, name );
  return 0;
}

int
RvFieldIter::get_reference( MDReference &mref ) noexcept
{
  uint8_t * buf = (uint8_t *) this->iter_msg().msg_buf;
  mref.fendian = MD_BIG;
  mref.ftype   = (MDType) rv_type_to_md_type[ this->type ];
  mref.fsize   = this->size;
  mref.fptr    = &buf[ this->field_end - this->size ];
  if ( mref.ftype == MD_ARRAY ) {
    switch ( this->type ) {
      default: break;
      case RV_ARRAY_I8:
      case RV_ARRAY_U8:  mref.fentrysz = 1; break;
      case RV_ARRAY_I16:
      case RV_ARRAY_U16: mref.fentrysz = 2; break;
      case RV_ARRAY_I32:
      case RV_ARRAY_U32:
      case RV_ARRAY_F32: mref.fentrysz = 4; break;
      case RV_ARRAY_I64:
      case RV_ARRAY_U64:
      case RV_ARRAY_F64: mref.fentrysz = 8; break;
      case RV_ARRAY_STR:
      case RV_ARRAY_MSG:
        if ( this->size >= 4 ) {
          uint32_t count = get_uint<uint32_t>( mref.fptr, MD_BIG );
          const char * ptr = (const char *) &mref.fptr[ 4 ],
                     * end = (const char *) &mref.fptr[ this->size ];
          if ( this->type == RV_ARRAY_STR ) {
            for ( uint32_t i = 0; i < count; i++ ) {
              size_t len = ::strnlen( ptr, end - ptr );
              if ( &ptr[ len ] >= end || ptr[ len ] != '\0' )
                return Err::BAD_FIELD_SIZE;
              ptr = &ptr[ len + 1 ];
            }
          }
          else {
            for ( uint32_t i = 0; i < count; i++ ) {
              if ( &ptr[ 4 ] > end )
                return Err::BAD_FIELD_SIZE;
              size_t len = get_uint<uint32_t>( ptr, MD_BIG );
              if ( &ptr[ len ] > end )
                return Err::BAD_FIELD_SIZE;
              ptr = &ptr[ len ];
            }
          }
          if ( ptr != end )
            return Err::BAD_FIELD_SIZE;
          mref.fsize = count;
          mref.fptr  = &mref.fptr[ 4 ];
        }
        else {
          mref.fsize = 0;
        }
        mref.fentrysz = 0;
        break;
    }
    switch ( this->type ) {
      default: break;
      case RV_ARRAY_U8:
      case RV_ARRAY_U16: 
      case RV_ARRAY_U32:
      case RV_ARRAY_U64: mref.fentrytp = MD_UINT; break;
      case RV_ARRAY_I8:
      case RV_ARRAY_I16:
      case RV_ARRAY_I32:
      case RV_ARRAY_I64: mref.fentrytp = MD_INT; break;
      case RV_ARRAY_F32: 
      case RV_ARRAY_F64: mref.fentrytp = MD_REAL; break;
      case RV_ARRAY_STR: mref.fentrytp = MD_STRING; break;
      case RV_ARRAY_MSG: mref.fentrytp = MD_MESSAGE; break;
    }
  }
  return 0;
}

int
RvMsg::get_array_ref( MDReference &mref,  size_t i, MDReference &aref ) noexcept
{
  size_t num_entries = mref.fsize;
  if ( mref.fentrysz != 0 ) {
    num_entries /= mref.fentrysz;
    if ( i < num_entries ) {
      aref.zero();
      aref.ftype   = mref.fentrytp;
      aref.fsize   = mref.fentrysz;
      aref.fendian = mref.fendian;
      aref.fptr = &mref.fptr[ i * (size_t) mref.fentrysz ];
      return 0;
    }
    return Err::NOT_FOUND;
  }
  if ( mref.fentrytp == MD_STRING && i < num_entries ) {
    const char * ptr = (const char *) mref.fptr;
    size_t       len = ::strlen( ptr );
    for ( ; i > 0; i-- ) {
      ptr = &ptr[ len + 1 ];
      len = ::strlen( ptr );
    }
    aref.set( (void *) ptr, len + 1, MD_STRING );
    return 0;
  }
  if ( mref.fentrytp == MD_MESSAGE && i < num_entries ) {
    const char * ptr = (const char *) mref.fptr;
    size_t len = get_uint<uint32_t>( ptr, MD_BIG );
    for ( ; i > 0; i-- ) {
      ptr = &ptr[ len ];
      len = get_uint<uint32_t>( ptr, MD_BIG );
    }
    aref.set( (void *) ptr, len, MD_MESSAGE);
    return 0;
  }
  aref.zero();
  return Err::NOT_FOUND;
}

int
RvFieldIter::find( const char *name,  size_t name_len,
                   MDReference &mref ) noexcept
{
  MDName n, n2;
  get_rv_name( (char *) name, name_len, n );

  char * buf = (char *) this->iter_msg().msg_buf;
  int status;
  if ( (status = this->first()) == 0 ) {
    do {
      get_rv_name( &buf[ this->field_start + 1 ], this->name_len, n2 );
      if ( ( n.fid != 0 && n2.fid != 0 && n.fid == n2.fid ) ||
           MDDict::dict_equals( n.fname, n.fnamelen, n2.fname, n2.fnamelen ) )
        return this->get_reference( mref );
    } while ( (status = this->next()) == 0 );
  }
  return status;
}

bool
RvFieldIter::is_named( const char *name,  size_t name_len ) noexcept
{
  MDName n, n2;
  get_rv_name( (char *) name, name_len, n );

  char * buf = (char *) this->iter_msg().msg_buf;
  get_rv_name( &buf[ this->field_start + 1 ], this->name_len, n2 );
  if ( ( n.fid != 0 && n2.fid != 0 && n.fid == n2.fid ) ||
       MDDict::dict_equals( n.fname, n.fnamelen, n2.fname, n2.fnamelen ) )
    return true;
  return false;
}

int
RvFieldIter::first( void ) noexcept
{
  this->field_start = this->iter_msg().msg_off + 8;
  this->field_end   = this->iter_msg().msg_end;
  this->field_index = 0;
  if ( this->field_start >= this->field_end )
    return Err::NOT_FOUND;
  return this->unpack();
}

int
RvFieldIter::next( void ) noexcept
{
  this->field_start = this->field_end;
  this->field_end   = this->iter_msg().msg_end;
  this->field_index++;
  if ( this->field_start >= this->field_end )
    return Err::NOT_FOUND;
  return this->unpack();
}

int
RvFieldIter::unpack( void ) noexcept
{
  const uint8_t * buf     = (uint8_t *) this->iter_msg().msg_buf;
  size_t          i       = this->field_start;
  uint8_t         szbytes = 0;

  if ( i >= this->field_end )
    goto bad_bounds;
  this->name_len = buf[ i++ ];
  i += this->name_len;
  if ( i >= this->field_end )
    goto bad_bounds;
  this->type = buf[ i++ ];

  switch ( this->type ) {
    case RV_RVMSG:
      if ( i + 5 > this->field_end )
        goto bad_bounds;
      if ( buf[ i++ ] != RV_LONG_SIZE )
        return Err::BAD_FIELD_SIZE;
      this->size = get_u32<MD_BIG>( &buf[ i ] );
      break;

    case RV_STRING:
    case RV_SUBJECT:
    case RV_ENCRYPTED:
    case RV_OPAQUE:
    case RV_ARRAY_I8:
    case RV_ARRAY_U8:
    case RV_ARRAY_I16:
    case RV_ARRAY_U16:
    case RV_ARRAY_I32:
    case RV_ARRAY_U32:
    case RV_ARRAY_I64:
    case RV_ARRAY_U64:
    case RV_ARRAY_F32:
    case RV_ARRAY_F64:
    case RV_ARRAY_STR:
    case RV_ARRAY_MSG:
    case RV_XML:
      this->size = buf[ i++ ];
      switch ( this->size ) {
        case RV_LONG_SIZE:
          if ( i + 4 > this->field_end )
            goto bad_bounds;
          this->size = get_u32<MD_BIG>( &buf[ i ] );
          szbytes = 4;
          break;
        case RV_SHORT_SIZE:
          if ( i + 2 > this->field_end )
            goto bad_bounds;
          this->size = get_u16<MD_BIG>( &buf[ i ] );
          szbytes = 2;
          break;
/*        case RV_TINY_SIZE:
          if ( i + 1 > this->field_end )
            goto bad_bounds;
          this->size = buf[ i ];
          szbytes = 1;
          break;*/
        default:
          break;
      }
      break;

    case RV_DATETIME:
    case RV_BOOLEAN:
    case RV_IPDATA:
    case RV_INT:
    case RV_UINT:
    case RV_REAL:
      this->size = buf[ i++ ];
      break;

    default:
      return Err::BAD_FIELD_TYPE;
  }

  i += this->size;
  if ( this->size < szbytes )
    goto bad_bounds;
  this->size -= szbytes;
  if ( i > this->field_end ) {
  bad_bounds:;
    return Err::BAD_FIELD_BOUNDS;
  }
  this->field_end = i;
  return 0;
}

int
RvFieldIter::update( MDReference &mref ) noexcept
{
  MDType ftype = (MDType) rv_type_to_md_type[ this->type ];
  if ( mref.ftype == ftype && mref.fsize == this->size ) {
    uint8_t * buf = (uint8_t *) this->iter_msg().msg_buf;
    size_t    i   = this->field_end - this->size;
    ::memcpy( &buf[ i ], mref.fptr, mref.fsize );
    return 0;
  }
  return Err::BAD_FIELD_TYPE;
}

bool
RvMsgWriter::resize( size_t len ) noexcept
{
  static const size_t max_size = 0x3fffffff; /* 1 << 30 - 1 == 1073741823 */
  if ( this->err != 0 )
    return false;
  RvMsgWriter *p = this;
  for ( ; p->parent != NULL; p = p->parent )
    ;
  if ( len > max_size )
    return false;
  size_t old_len = p->buflen,
         new_len = old_len + ( this->buflen + len - this->off );
  if ( new_len > max_size )
    return false;
  if ( new_len < old_len * 2 )
    new_len = old_len * 2;
  else
    new_len += 1024;
  if ( new_len > max_size )
    new_len = max_size;
  uint8_t * old_buf = p->buf,
          * new_buf = old_buf;
  this->mem().extend( old_len, new_len, &new_buf );
  uint8_t * end = &new_buf[ new_len ];

  p->buf    = new_buf;
  p->buflen = new_len;

  for ( RvMsgWriter *c = this; c != p; c = c->parent ) {
    if ( c->buf >= old_buf && c->buf < &old_buf[ old_len ] ) {
      size_t off = c->buf - old_buf;
      c->buf = &new_buf[ off ];
      c->buflen = end - c->buf;
    }
  }
  return this->off + len <= this->buflen;
}

namespace {
struct FnameWriter {
  const char * fname;
  size_t       fname_len,
               zpad; /* if fname is not null terminated, zpad = 1 */

  FnameWriter( const char *fn,  size_t fn_len )
      : fname( fn ), fname_len( fn_len ), zpad( 0 ) {
    if ( fn_len > 0 ) {
      if ( fn[ fn_len - 1 ] != '\0' )
        if ( fn_len < 3 || fn[ fn_len - 3 ] != '\0' )
          this->zpad = 1;
    }
  }
  size_t copy( uint8_t *buf,  size_t off ) const {
    buf[ off++ ] = (uint8_t) ( this->fname_len + this->zpad );
    if ( this->fname_len > 0 ) {
      ::memcpy( &buf[ off ], this->fname, this->fname_len );
      off += this->fname_len;
      if ( this->zpad )
        buf[ off++ ] = '\0';
    }
    return off;
  }
  size_t len( void ) const {
    return 1 + this->fname_len + this->zpad;
  }
  bool too_big( void ) const {
    return this->len() > 256;
  }
};
}

RvMsgWriter &
RvMsgWriter::append_msg( const char *fname,  size_t fname_len,
                         RvMsgWriter &submsg ) noexcept
{
  FnameWriter fwr( fname, fname_len );
  size_t      len     = fwr.len() + 1,
              szbytes = 5 + 4;

  len += szbytes;
  if ( fwr.too_big() )
    return this->error( Err::BAD_NAME );
  if ( ! this->has_space( len ) )
    return this->error( Err::NO_SPACE );

  this->off = fwr.copy( this->buf, this->off );
  this->buf[ this->off++ ] = RV_RVMSG;
  this->buf[ this->off++ ] = RV_LONG_SIZE;

  submsg.buf    = &this->buf[ this->off ];
  submsg.off    = 8;
  submsg.buflen = this->buflen - this->off;
  submsg.err    = 0;
  submsg.parent = this;
  return submsg;
}

RvMsgWriter &
RvMsgWriter::append_subject( const char *fname,  size_t fname_len,
                             const char *subj,  size_t subj_len ) noexcept
{
  FnameWriter  fwr( fname, fname_len );
  size_t       len = fwr.len() + 1,
               szbytes = 3,
               fsize = 3;
  const char * s,
             * x = subj;
  uint32_t     segs = 1;

  if ( subj_len == 0 )
    subj_len = ::strlen( subj );
  for ( s = subj; s < &subj[ subj_len ]; s++ ) {
    if ( *s == '.' ) {
      fsize += 2;
      if ( s - x >= 254 || s == x )
        return this->error( Err::BAD_FIELD_BOUNDS );
      x = s + 1;
      segs++;
    }
    else
      fsize++;
  }
  if ( segs > 255 )
    return this->error( Err::BAD_FIELD_BOUNDS );
  len += szbytes + fsize;

  if ( fwr.too_big() )
    return this->error( Err::BAD_NAME );
  if ( ! this->has_space( len ) )
    return this->error( Err::NO_SPACE );

  uint8_t * ptr = this->buf;
  ptr += fwr.copy( this->buf, this->off );

  ptr[ 0 ] = RV_SUBJECT;
  ptr[ 1 ] = RV_SHORT_SIZE;
  ptr[ 2 ] = ( ( fsize + 2 ) >> 8 ) & 0xffU;
  ptr[ 3 ] = ( fsize + 2 ) & 0xffU;

  uint32_t i = 2, j = 1;
  uint8_t * out = &ptr[ 4 ];
  segs = 1;
  for ( s = subj; s < &subj[ subj_len ]; s++ ) {
    if ( *s == '.' ) {
      out[ i++ ] = 0;
      out[ j ]   = (uint8_t) ( i - j );
      j = i++;
      segs++;
    }
    else {
      out[ i++ ] = *s;
    }
  }
  out[ i++ ] = 0;
  out[ j ]   = (uint8_t) ( i - j );
  out[ 0 ]   = (uint8_t) segs;

  this->off += len;
  return *this;
}

static bool
get_rv_array_type( MDReference &mref,  uint8_t &t ) noexcept
{
  switch ( mref.fentrytp ) {
    case MD_BOOLEAN:
      if ( mref.fentrysz != 1 )
        return false;
      t = RV_ARRAY_I8;
      break;
    case MD_UINT:
      if ( mref.fentrysz == 1 )
        t = RV_ARRAY_U8;
      else if ( mref.fentrysz == 2 )
        t = RV_ARRAY_U16;
      else if ( mref.fentrysz == 4 )
        t = RV_ARRAY_U32;
      else if ( mref.fentrysz == 8 )
        t = RV_ARRAY_U64;
      else
        return false;
      break;
    case MD_INT:
      if ( mref.fentrysz == 1 )
        t = RV_ARRAY_I8;
      else if ( mref.fentrysz == 2 )
        t = RV_ARRAY_I16;
      else if ( mref.fentrysz == 4 )
        t = RV_ARRAY_I32;
      else if ( mref.fentrysz == 8 )
        t = RV_ARRAY_I64;
      else
        return false;
      break;
    case MD_REAL:
      if ( mref.fentrysz == 4 )
        t = RV_ARRAY_F32;
      else if ( mref.fentrysz == 8 )
        t = RV_ARRAY_F64;
      else
        return false;
      break;
    case MD_STRING:
    default:
      return false;
  }
  return true;
}

static void
swap_rv_array( MDReference &mref,  uint8_t *fptr ) noexcept
{
  uint8_t *fend = &fptr[ mref.fsize ];

  switch ( mref.fentrytp ) {
    case MD_UINT:
      if ( mref.fentrysz == 2 )
        goto swap2;
      else if ( mref.fentrysz == 4 )
        goto swap4;
      else if ( mref.fentrysz == 8 )
        goto swap8;
      break;
    case MD_INT:
      if ( mref.fentrysz == 2 )
        goto swap2;
      else if ( mref.fentrysz == 4 )
        goto swap4;
      else if ( mref.fentrysz == 8 )
        goto swap8;
      break;
    case MD_REAL:
      if ( mref.fentrysz == 4 )
        goto swap4;
      else if ( mref.fentrysz == 8 )
        goto swap8;
      break;
    default:
      break;
  }
  return;
swap2:
  while ( fptr < fend ) {
    uint16_t x = get_uint<uint16_t>( fptr, MD_BIG );
    ::memcpy( fptr, &x, sizeof( x ) );
    fptr = &fptr[ 2 ];
  }
  return;
swap4:
  while ( fptr < fend ) {
    uint32_t x = get_uint<uint32_t>( fptr, MD_BIG );
    ::memcpy( fptr, &x, sizeof( x ) );
    fptr = &fptr[ 4 ];
  }
  return;
swap8:
  while ( fptr < fend ) {
    uint64_t x = get_uint<uint64_t>( fptr, MD_BIG );
    ::memcpy( fptr, &x, sizeof( x ) );
    fptr = &fptr[ 8 ];
  }
  return;
}

RvMsgWriter &
RvMsgWriter::append_ref( const char *fname,  size_t fname_len,
                         MDReference &mref ) noexcept
{
  size_t fsize = mref.fsize;

  if ( mref.ftype == MD_STRING &&
       ( fsize == 0 || mref.fptr[ fsize - 1 ] != '\0' ) )
    fsize++;

  FnameWriter fwr( fname, fname_len );
  size_t      len     = fwr.len() + 1 + fsize,
              szbytes = rv_size_bytes( fsize );

  len += szbytes;
  if ( fwr.too_big() )
    return this->error( Err::BAD_NAME );
  if ( ! this->has_space( len ) )
    return this->error( Err::NO_SPACE );

  uint8_t * ptr = this->buf;
  ptr += fwr.copy( this->buf, this->off );

  switch ( mref.ftype ) {
    default:
    case MD_OPAQUE:   ptr[ 0 ] = RV_OPAQUE;   break;
    case MD_PARTIAL:
    case MD_STRING:   ptr[ 0 ] = RV_STRING;   break;
    case MD_BOOLEAN:  ptr[ 0 ] = RV_BOOLEAN;  break;
    case MD_IPDATA:   ptr[ 0 ] = RV_IPDATA;   break;
    case MD_INT:      ptr[ 0 ] = RV_INT;      break;
    case MD_UINT:     ptr[ 0 ] = RV_UINT;     break;
    case MD_REAL:     ptr[ 0 ] = RV_REAL;     break;
    case MD_DATETIME: ptr[ 0 ] = RV_DATETIME; break;
    case MD_ARRAY:    if ( ! get_rv_array_type( mref, ptr[ 0 ] ) )
                        return this->error( Err::BAD_FIELD_TYPE );
                      break;
  }
  ptr += 1 + pack_rv_size( &ptr[ 1 ], fsize, szbytes );
  /* invert endian, for little -> big */
  if ( mref.fendian != MD_BIG && fsize > 1 &&
       ( mref.ftype == MD_UINT || mref.ftype == MD_INT ||
         mref.ftype == MD_REAL || mref.ftype == MD_DATETIME ||
         mref.ftype == MD_IPDATA || mref.ftype == MD_BOOLEAN ) ) {
    size_t off = fsize;
    ptr[ 0 ] = mref.fptr[ --off ];
    ptr[ 1 ] = mref.fptr[ --off ];
    if ( off > 0 ) {
      ptr[ 2 ] = mref.fptr[ --off ];
      ptr[ 3 ] = mref.fptr[ --off ];
      if ( off > 0 ) {
        ptr[ 4 ] = mref.fptr[ --off ];
        ptr[ 5 ] = mref.fptr[ --off ];
        ptr[ 6 ] = mref.fptr[ --off ];
        ptr[ 7 ] = mref.fptr[ --off ];
      }
    }
  }
  else {
    ::memcpy( ptr, mref.fptr, mref.fsize );
    if ( fsize > mref.fsize )
      ptr[ mref.fsize ] = '\0';
    else {
      if ( mref.fendian == MD_LITTLE && mref.ftype == MD_ARRAY &&
           mref.fentrysz > 1 )
        swap_rv_array( mref, ptr );
    }
  }
  this->off += len;
  return *this;
}

RvMsgWriter &
RvMsgWriter::append_string_array( const char *fname,  size_t fname_len,
                                  char **ar,  size_t array_size,
                                  size_t fsize ) noexcept
{
  FnameWriter fwr( fname, fname_len );

  if ( fsize == 0 ) {
    for ( size_t i = 0; i < array_size; i++ )
      fsize += ::strlen( ar[ i ] ) + 1;
  }
  size_t len = fwr.len() + 1 + fsize,
         szbytes = rv_size_bytes( fsize + 4 );

  fsize += 4; /* array size */
  len   += 4 + szbytes;
  if ( fwr.too_big() )
    return this->error( Err::BAD_NAME );
  if ( ! this->has_space( len ) )
    return this->error( Err::NO_SPACE );

  uint8_t * ptr = this->buf;
  ptr += fwr.copy( this->buf, this->off );

  ptr[ 0 ] = RV_ARRAY_STR;
  ptr += 1 + pack_rv_size( &ptr[ 1 ], fsize, szbytes );

  uint32_t tmp = (uint32_t) array_size;
  tmp = get_u32<MD_BIG>( &tmp );
  ::memcpy( ptr, &tmp, 4 );
  ptr = &ptr[ 4 ];

  for ( uint32_t i = 0; i < array_size; i++ ) {
    size_t slen = ::strlen( ar[ i ] ) + 1;
    ::memcpy( ptr, ar[ i ], slen );
    ptr = &ptr[ slen ];
  }
  this->off += len;
  return *this;
}

RvMsgWriter &
RvMsgWriter::append_decimal( const char *fname,  size_t fname_len,
                             MDDecimal &dec ) noexcept
{
  FnameWriter fwr( fname, fname_len );
  size_t      len = fwr.len() + 1 + 1 + 8;
  double      val;
  int         status;

  if ( fwr.too_big() )
    return this->error( Err::BAD_NAME );
  if ( ! this->has_space( len ) )
    return this->error( Err::NO_SPACE );
  if ( (status = dec.get_real( val )) != 0 )
    return this->error( status );

  uint8_t * ptr = this->buf;
  ptr += fwr.copy( this->buf, this->off );

  ptr[ 0 ] = (uint8_t) RV_REAL;
  ptr[ 1 ] = (uint8_t) 8;
  ptr = &ptr[ 2 ];

  if ( md_endian != MD_LITTLE )
    ::memcpy( ptr, &val, 8 );
  else {
    uint8_t * fptr = (uint8_t *) (void *) &val;
    for ( size_t i = 0; i < 8; i++ )
      ptr[ i ] = fptr[ 7 - i ];
  }
  this->off += len;
  return *this;
}

RvMsgWriter &
RvMsgWriter::append_time( const char *fname,  size_t fname_len,
                          MDTime &time ) noexcept
{
  FnameWriter fwr( fname, fname_len );
  char        sbuf[ 32 ];
  size_t      n   = time.get_string( sbuf, sizeof( sbuf ) );
  size_t      len = fwr.len() + 1 + 1 + n + 1;

  if ( fwr.too_big() )
    return this->error( Err::BAD_NAME );
  if ( ! this->has_space( len ) )
    return this->error( Err::NO_SPACE );

  uint8_t * ptr = this->buf;
  ptr += fwr.copy( this->buf, this->off );

  ptr[ 0 ] = (uint8_t) RV_STRING;
  ptr[ 1 ] = (uint8_t) ( n + 1 );
  ptr = &ptr[ 2 ];
  ::memcpy( ptr, sbuf, n + 1 );
  this->off += len;
  return *this;
}

RvMsgWriter &
RvMsgWriter::append_date( const char *fname,  size_t fname_len,
                          MDDate &date ) noexcept
{
  FnameWriter fwr( fname, fname_len );
  char        sbuf[ 32 ];
  size_t      n   = date.get_string( sbuf, sizeof( sbuf ) );
  size_t      len = fwr.len() + 1 + 1 + n + 1;

  if ( fwr.too_big() )
    return this->error( Err::BAD_NAME );
  if ( ! this->has_space( len ) )
    return this->error( Err::NO_SPACE );

  uint8_t * ptr = this->buf;
  ptr += fwr.copy( this->buf, this->off );

  ptr[ 0 ] = (uint8_t) RV_STRING;
  ptr[ 1 ] = (uint8_t) ( n + 1 );
  ptr = &ptr[ 2 ];
  ::memcpy( ptr, sbuf, n + 1 );
  this->off += len;
  return *this;
}

RvMsgWriter &
RvMsgWriter::append_stamp( const char *fname,  size_t fname_len,
                           MDStamp &time ) noexcept
{
  FnameWriter fwr( fname, fname_len );
  size_t      len = fwr.len() + 1 + 1 + 8;

  if ( fwr.too_big() )
    return this->error( Err::BAD_NAME );
  if ( ! this->has_space( len ) )
    return this->error( Err::NO_SPACE );

  uint8_t * ptr = this->buf;
  ptr += fwr.copy( this->buf, this->off );

  ptr[ 0 ] = (uint8_t) RV_DATETIME;
  ptr[ 1 ] = (uint8_t) 8;
  ptr = &ptr[ 2 ];

  uint64_t tmp  = time.micros();
  uint32_t sec  = (uint32_t) ( tmp / 1000000 );
  uint32_t usec = (uint32_t) ( tmp % 1000000 );
  sec = get_u32<MD_BIG>( &sec );
  ::memcpy( ptr, &sec, 4 );
  ptr = &ptr[ 4 ];
  usec = get_u32<MD_BIG>( &usec );
  ::memcpy( ptr, &usec, 4 );
  this->off += len;
  return *this;
}

RvMsgWriter &
RvMsgWriter::append_xml( const char *fname,  size_t fname_len,
                         const char *doc,  size_t doc_len ) noexcept
{
  char   chunk[ 1024 ],
       * zdoc = NULL;
  size_t zdoc_len = 0;

  z_stream zs;
  memset( &zs, 0, sizeof( zs ) );
  deflateInit( &zs, Z_DEFAULT_COMPRESSION );
  zs.avail_in = doc_len;
  zs.next_in  = (Bytef *) doc;
  for (;;) {
    zs.avail_out = sizeof( chunk );
    zs.next_out  = (Bytef *) chunk;
    int x = deflate( &zs, Z_FINISH );
    if ( x == Z_STREAM_ERROR )
      return *this;
    if ( zs.avail_out != 0 ) {
      if ( zdoc_len == 0 ) {
        zdoc     = chunk;
        zdoc_len = sizeof( chunk ) - zs.avail_out;
      }
      else {
        this->mem().extend( zdoc_len, zdoc_len + sizeof( chunk ) - zs.avail_out,
                            &zdoc );
        ::memcpy( &zdoc[ zdoc_len ], chunk, sizeof( chunk ) - zs.avail_out );
        zdoc_len += sizeof( chunk ) - zs.avail_out;
      }
      break;
    }
    this->mem().extend( zdoc_len, zdoc_len + sizeof( chunk ), &zdoc );
    ::memcpy( &zdoc[ zdoc_len ], chunk, sizeof( chunk ) );
    zdoc_len += sizeof( chunk );
  }
  deflateEnd( &zs );

  FnameWriter fwr( fname, fname_len );
  size_t usz = rv_size_bytes( doc_len ),
         zsz = rv_size_bytes( zdoc_len + usz ),
         len = fwr.len() + 1 + zsz + usz + zdoc_len;

  if ( fwr.too_big() )
    return this->error( Err::BAD_NAME );
  if ( ! this->has_space( len ) )
    return this->error( Err::NO_SPACE );

  uint8_t * ptr = this->buf;
  ptr += fwr.copy( this->buf, this->off );

  ptr[ 0 ] = (uint8_t) RV_XML;
  size_t i = 1;
  i += pack_rv_size( &ptr[ i ], zdoc_len + usz, zsz );
  i += pack_rv_size( &ptr[ i ], doc_len, usz );
  ptr = &ptr[ i ];
  ::memcpy( ptr, zdoc, zdoc_len );
  this->off += len;

  return *this;
}

RvMsgWriter &
RvMsgWriter::append_enum( const char *fname,  size_t fname_len,
                          MDEnum &enu ) noexcept
{
  FnameWriter fwr( fname, fname_len );
  size_t      len = fwr.len() + 1 + 1 + enu.disp_len + 1;

  if ( fwr.too_big() )
    return this->error( Err::BAD_NAME );
  if ( ! this->has_space( len ) )
    return this->error( Err::NO_SPACE );

  uint8_t * ptr = this->buf;
  ptr += fwr.copy( this->buf, this->off );

  ptr[ 0 ] = (uint8_t) RV_STRING;
  ptr[ 1 ] = (uint8_t) ( enu.disp_len + 1 );
  ptr = &ptr[ 2 ];
  ::memcpy( ptr, enu.disp, enu.disp_len );
  ptr[ enu.disp_len ] = '\0';
  this->off += len;
  return *this;
}

RvMsgWriter &
RvMsgWriter::append_msg_array( const char *fname,  size_t fname_len,
                               size_t &aroff ) noexcept
{
  FnameWriter fwr( fname, fname_len );
  size_t      len = fwr.len() + 1 + 1 + 4 + 4;

  aroff = 0;
  if ( fwr.too_big() )
    return this->error( Err::BAD_NAME );
  if ( ! this->has_space( len ) )
    return this->error( Err::NO_SPACE );

  uint8_t * ptr = this->buf;
  ptr += fwr.copy( this->buf, this->off );

  ptr[ 0 ] = (uint8_t) RV_ARRAY_MSG;
  ptr[ 1 ] = (uint8_t) RV_LONG_SIZE;
  ptr = &ptr[ 2 ];
  aroff = ptr - this->buf;
  size_t i = 0;
  ptr[ i++ ] = 0;
  ptr[ i++ ] = 0;
  ptr[ i++ ] = 0;
  ptr[ i++ ] = 8;
  ptr[ i++ ] = 0;
  ptr[ i++ ] = 0;
  ptr[ i++ ] = 0;
  ptr[ i++ ] = 0;

  this->off += len;
  return *this;
}

RvMsgWriter &
RvMsgWriter::append_msg_elem( RvMsgWriter &submsg ) noexcept
{
  if ( ! this->has_space( 8 ) )
    return this->error( Err::NO_SPACE );

  submsg.buf    = &this->buf[ this->off ];
  submsg.off    = 8;
  submsg.buflen = this->buflen - this->off;
  submsg.err    = 0;
  submsg.parent = this;
  return submsg;
}

int
RvMsgWriter::convert_msg( MDMsg &jmsg,  bool skip_hdr ) noexcept
{
  MDFieldIter *iter;
  int status;
  if ( (status = jmsg.get_field_iter( iter )) == 0 )
    status = iter->first();
  if ( status == 0 ) {
    do {
      MDName      name;
      MDReference mref;
      MDDecimal   dec;
      if ( iter->get_name( name ) == 0 && iter->get_reference( mref ) == 0 ) {
        if ( skip_hdr && is_sass_hdr( name ) )
          continue;
        switch ( mref.ftype ) {
          default:
            this->append_ref( name.fname, name.fnamelen, mref );
            status = this->err;
            break;

          case MD_TIME: {
            MDTime time;
            time.get_time( mref );
            this->append_time( name.fname, name.fnamelen, time );
            break;
          }
          case MD_DATE: {
            MDDate date;
            date.get_date( mref );
            this->append_date( name.fname, name.fnamelen, date );
            break;
          }
          case MD_ENUM: {
            MDEnum enu;
            if ( mref.ftype == MD_ENUM && iter->get_enum( mref, enu ) == 0 )
              this->append_enum( name.fname, name.fnamelen, enu );
            else
              mref.ftype = MD_UINT;
            break;
          }
          case MD_DECIMAL:
            dec.get_decimal( mref );
            if ( dec.hint == MD_DEC_INTEGER ) {
              if ( dec.ival == (int64_t) (int16_t) dec.ival )
                this->append_int<int16_t>( name.fname, name.fnamelen,
                                           (int16_t) dec.ival );
              else if ( dec.ival == (int64_t) (int32_t) dec.ival )
                this->append_int<int32_t>( name.fname, name.fnamelen,
                                           (int32_t) dec.ival );
              else
                this->append_int<int64_t>( name.fname, name.fnamelen, dec.ival );
            }
            else {
              this->append_decimal( name.fname, name.fnamelen, dec );
            }
            status = this->err;
            break;
          case MD_MESSAGE: {
            RvMsgWriter submsg( this->mem(), NULL, 0 );
            MDMsg * jmsg2 = NULL;
            this->append_msg( name.fname, name.fnamelen, submsg );
            status = this->err;
            if ( status == 0 )
              status = jmsg.get_sub_msg( mref, jmsg2, iter );
            if ( status == 0 ) {
              status = submsg.convert_msg( *jmsg2, false );
              if ( status == 0 )
                this->update_hdr( submsg );
            }
            break;
          }

          case MD_ARRAY: {
            MDReference aref;
            MDType      atype = MD_INT;
            size_t      asize = 1;
            size_t      i,
                        num_entries = mref.fsize;

            if ( mref.fentrysz > 0 )
              num_entries /= mref.fentrysz;

            for ( i = 0; i < num_entries; i++ ) {
              if ( (status = jmsg.get_array_ref( mref, i, aref )) != 0 )
                break;
              if ( aref.ftype == MD_DECIMAL ||
                   aref.ftype == MD_INT     ||
                   aref.ftype == MD_UINT    ||
                   aref.ftype == MD_BOOLEAN ) {
                if ( atype != MD_REAL ) {
                  dec.get_decimal( aref );
                  if ( dec.hint == MD_DEC_INTEGER ) {
                    if ( asize < 8 &&
                         dec.ival != (int64_t) (int32_t) dec.ival ) {
                      atype = MD_INT;
                      asize = 8;
                    }
                    else if ( asize < 4 &&
                              dec.ival != (int64_t) (int16_t) dec.ival ) {
                      atype = MD_INT;
                      asize = 4;
                    }
                    else if ( asize < 2 &&
                              dec.ival != (int64_t) (int8_t) dec.ival ) {
                      atype = MD_INT;
                      asize = 2;
                    }
                  }
                  else {
                    atype = MD_REAL;
                    asize = 8;
                  }
                }
                else if ( aref.ftype == MD_REAL ) {
                  atype = MD_REAL;
                  asize = 8;
                }
              }
              else {
                atype = MD_STRING;
                break;
              }
            }
            if ( status == 0 ) {
              if ( atype == MD_INT ) {
                void * ar = (int32_t *) jmsg.mem->make( num_entries * asize );
                for ( i = 0; i < num_entries; i++ ) {
                  jmsg.get_array_ref( mref, i, aref );
                  dec.get_decimal( aref );
                  if ( asize == 8 )
                    ((int64_t *) ar)[ i ] = dec.ival;
                  else if ( asize == 4 )
                    ((int32_t *) ar)[ i ] = (int32_t) dec.ival;
                  else if ( asize == 2 )
                    ((int16_t *) ar)[ i ] = (int16_t) dec.ival;
                  else
                    ((int8_t *) ar)[ i ] = (int8_t) dec.ival;
                }
                aref.set( ar, num_entries * asize, MD_ARRAY );
                aref.fentrytp = MD_INT;
                aref.fentrysz = asize;
                this->append_ref( name.fname, name.fnamelen, aref );
              }
              else if ( atype == MD_REAL ) {
                double * ar = (double *) jmsg.mem->make( num_entries * 8 );
                for ( i = 0; i < num_entries; i++ ) {
                  jmsg.get_array_ref( mref, i, aref );
                  if ( aref.ftype == MD_REAL )
                    ar[ i ] = get_float<double>( mref );
                  else if ( aref.ftype == MD_INT )
                    ar[ i ] = (double) get_uint<int64_t>( mref );
                  else if ( aref.ftype == MD_UINT || aref.ftype == MD_BOOLEAN )
                    ar[ i ] = (double) get_int<uint64_t>( mref );
                  else if ( dec.get_decimal( aref ) != 0 ||
                            dec.get_real( ar[ i ] ) != 0 )
                    ar[ i ] = 0;
                }
                aref.set( ar, num_entries * 8, MD_ARRAY );
                aref.fentrytp = MD_REAL;
                aref.fentrysz = 8;
                this->append_ref( name.fname, name.fnamelen, aref );
              }
              else if ( atype == MD_STRING ) {
                char ** ar = (char **)
                  jmsg.mem->make( num_entries * sizeof( char * ) );
                size_t len = 0, slen;
                for ( i = 0; i < num_entries; i++ ) {
                  jmsg.get_array_ref( mref, i, aref );
                  if ( (status = jmsg.get_string( aref, ar[ i ], slen )) != 0 )
                    break;
                  len += slen + 1;
                }
                if ( status == 0 )
                  this->append_string_array( name.fname, name.fnamelen,
                                             ar, num_entries, len );
              }
            }
            if ( status == 0 )
              status = this->err;
            break;
          }
        }
        if ( status != 0 )
          break;
      }
    } while ((status = iter->next()) == 0 );
  }
  if ( status != Err::NOT_FOUND )
    return status;
  return 0;
}

RvMsgWriter &
RvMsgWriter::append_iter( MDFieldIter *iter ) noexcept
{
  size_t len = iter->field_end - iter->field_start;

  if ( ! this->has_space( len ) )
    return this->error( Err::NO_SPACE );

  uint8_t * ptr = &this->buf[ this->off ];
  ::memcpy( ptr, &((uint8_t *) iter->iter_msg().msg_buf)[ iter->field_start ], len );
  this->off += len;
  return *this;
}

RvMsgWriter &
RvMsgWriter::append_writer( const RvMsgWriter &wr ) noexcept
{
  size_t len = wr.off - 8;
  return this->append_buffer( &wr.buf[ 8 ], len );
}

RvMsgWriter &
RvMsgWriter::append_rvmsg( const RvMsg &msg ) noexcept
{
  size_t len = msg.msg_end - ( msg.msg_off + 8 );
  return this->append_buffer( (uint8_t *) msg.msg_buf + msg.msg_off + 8, len );
}

RvMsgWriter &
RvMsgWriter::append_buffer( const void *buffer,  size_t len ) noexcept
{
  if ( len > 0 ) {
    if ( ! this->has_space( len ) )
      return this->error( Err::NO_SPACE );

    uint8_t * ptr = &this->buf[ this->off ];
    ::memcpy( ptr, buffer, len );
    this->off += len;
  }
  return *this;
}

