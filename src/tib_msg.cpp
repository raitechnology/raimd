#include <stdio.h>
#include <raimd/tib_msg.h>
#include <raimd/md_dict.h>
#include <raimd/sass.h>

using namespace rai;
using namespace md;

extern "C" {
MDMsg_t * tib_msg_unpack( void *bb,  size_t off,  size_t end,  uint32_t h,
                          MDDict_t *d,  MDMsgMem_t *m )
{
  return TibMsg::unpack( bb, off, end, h, (MDDict *)d,  *(MDMsgMem *) m );
}
MDMsgWriter_t *
tib_msg_writer_create( MDMsgMem_t *mem,  MDDict_t *,
                       void *buf_ptr, size_t buf_sz )
{
  void * p = ((MDMsgMem *) mem)->make( sizeof( TibMsgWriter ) );
  return new ( p ) TibMsgWriter( *(MDMsgMem *) mem, buf_ptr, buf_sz );
}
}

static const char TibMsg_proto_string[] = "TIBMSG";
const char *
TibMsg::get_proto_string( void ) noexcept
{
  return TibMsg_proto_string;
}

uint32_t
TibMsg::get_type_id( void ) noexcept
{
  return RAIMSG_TYPE_ID;
}

static MDMatch tibmsg_match = {
  .name        = TibMsg_proto_string,
  .off         = 0,
  .len         = 4, /* cnt of buf[] */
  .hint_size   = 2, /* cnt of hint[] */
  .ftype       = (uint8_t) RAIMSG_TYPE_ID,
  .buf         = { 0xce, 0x13, 0xaa, 0x1f },
  .hint        = { RAIMSG_TYPE_ID, 0x3f4c369e },
  .is_msg_type = TibMsg::is_tibmsg,
  .unpack      = (md_msg_unpack_f) TibMsg::unpack
};

bool
TibMsg::is_tibmsg( void *bb,  size_t off,  size_t end,  uint32_t ) noexcept
{
  uint32_t magic = 0;
  if ( off + 9 <= end )
    magic = get_u32<MD_BIG>( &((uint8_t *) bb)[ off ] );
  return magic == 0xce13aa1fU;
}

TibMsg *
TibMsg::unpack( void *bb,  size_t off,  size_t end,  uint32_t,  MDDict *d,
                MDMsgMem &m ) noexcept
{
  if ( off + 9 > end )
    return NULL;
  uint32_t magic    = get_u32<MD_BIG>( &((uint8_t *) bb)[ off ] );
  size_t   msg_size = get_u32<MD_BIG>( &((uint8_t *) bb)[ off + 5 ] );

  if ( magic != 0xce13aa1fU )
    return NULL;
  if ( off + msg_size + 9 > end )
    return NULL;
  void * ptr;
  m.incr_ref();
  m.alloc( sizeof( TibMsg ), &ptr );
  return new ( ptr ) TibMsg( bb, off, off + msg_size + 9, d, m );
}

void
TibMsg::init_auto_unpack( void ) noexcept
{
  MDMsg::add_match( tibmsg_match );
}

int
TibMsg::get_sub_msg( MDReference &mref, MDMsg *&msg,
                     MDFieldIter * ) noexcept
{
  uint8_t * bb    = (uint8_t *) this->msg_buf;
  size_t    start = (size_t) ( mref.fptr - bb );
  TibMsg  * tib_msg;
  void    * ptr;

  this->mem->alloc( sizeof( TibMsg ), &ptr );
  tib_msg = new ( ptr ) TibMsg( bb, start, start + mref.fsize, this->dict,
                                *this->mem );
  tib_msg->is_submsg = true;
  msg = tib_msg;
  return 0;
}

int
TibMsg::get_field_iter( MDFieldIter *&iter ) noexcept
{
  void * ptr;
  this->mem->alloc( sizeof( TibFieldIter ), &ptr );
  iter = new ( ptr ) TibFieldIter( *this );
  return 0;
}

MDFieldIter *
TibFieldIter::copy( void ) noexcept
{
  TibFieldIter *iter;
  void * ptr;
  this->iter_msg().mem->alloc( sizeof( TibFieldIter ), &ptr );
  iter = new ( ptr ) TibFieldIter( (TibMsg &) this->iter_msg() );
  this->dup_tib( *iter );
  return iter;
}

int
TibFieldIter::get_name( MDName &name ) noexcept
{
  uint8_t * buf = (uint8_t *) this->iter_msg().msg_buf;
  name.fid      = 0;
  name.fnamelen = this->name_len;
  if ( name.fnamelen > 0 )
    name.fname = (char *) &buf[ this->field_start + 1 ];
  else
    name.fname = NULL;
  return 0;
}

int
TibFieldIter::get_reference( MDReference &mref ) noexcept
{
  uint8_t * buf = (uint8_t *) this->iter_msg().msg_buf;
  mref.fendian  = MD_BIG;
  mref.fsize    = this->size;
  mref.ftype    = (MDType) this->type;
  mref.fptr     = &buf[ this->data_off ];
  mref.fentrysz = this->hint_size;
  mref.fentrytp = (MDType) this->hint_type;
  if ( this->type == MD_DECIMAL ) {
    TibMsg::set_decimal( this->dec, get_float<double>( mref ),
                         this->decimal_hint );
    mref.fendian = md_endian;
    mref.fptr    = (uint8_t *) (void *) &this->dec;
    mref.fsize   = sizeof( this->dec );
  }
  else if ( this->type == MD_TIME ) {
    if ( this->time.parse( (char *) mref.fptr, mref.fsize ) == 0 ) {
      mref.fendian = md_endian;
      mref.fptr    = (uint8_t *) (void *) &this->time;
      mref.fsize   = sizeof( this->time );
    }
    else {
      mref.ftype = MD_STRING;
    }
  }
  else if ( this->type == MD_DATE ) {
    if ( this->date.parse( (char *) mref.fptr, mref.fsize ) == 0 ) {
      mref.fendian = md_endian;
      mref.fptr    = (uint8_t *) (void *) &this->date;
      mref.fsize   = sizeof( this->date );
    }
    else {
      mref.ftype = MD_STRING;
    }
  }
  return 0;
}

int
TibMsg::get_array_ref( MDReference &mref, size_t i, MDReference &aref ) noexcept
{
  size_t num_entries = mref.fsize;
  aref.zero();
  if ( mref.fentrysz != 0 ) {
    num_entries /= mref.fentrysz;
    if ( i < num_entries ) {
      aref.ftype   = mref.fentrytp;
      aref.fsize   = mref.fentrysz;
      aref.fendian = mref.fendian;
      aref.fptr = &mref.fptr[ i * (size_t) mref.fentrysz ];
      return 0;
    }
  }
  return Err::NOT_FOUND;
}

int
TibFieldIter::get_hint_reference( MDReference &mref ) noexcept
{
  if ( this->hint_type != MD_NODATA ) {
    if ( this->type != TIB_PARTIAL && this->type != TIB_ARRAY ) {
      uint8_t * buf = (uint8_t *) this->iter_msg().msg_buf;
      mref.fendian  = MD_BIG;
      mref.fsize    = this->hint_size;
      mref.ftype    = (MDType) this->hint_type;
      mref.fptr     = &buf[ this->field_end - this->hint_size ];
      mref.fentrysz = 0;
      mref.fentrytp = MD_NODATA;
      return 0;
    }
  }
  mref.zero();
  return Err::NOT_FOUND;
}

int
TibFieldIter::find( const char *name,  size_t name_len,
                    MDReference &mref ) noexcept
{
  uint8_t * buf = (uint8_t *) this->iter_msg().msg_buf;
  int status;
  if ( (status = this->first()) == 0 ) {
    do {
      const char * fname = (char *) &buf[ this->field_start + 1 ];
      if ( MDDict::dict_equals( name, name_len, fname, this->name_len ) )
        return this->get_reference( mref );
    } while ( (status = this->next()) == 0 );
  }
  return status;
}

int
TibFieldIter::first( void ) noexcept
{
  this->field_start = this->iter_msg().msg_off;
  if ( ! this->is_submsg )
    this->field_start += 9;
  this->field_end   = this->iter_msg().msg_end;
  this->field_index = 0;
  if ( this->field_start >= this->field_end )
    return Err::NOT_FOUND;
  return this->unpack();
}

int
TibFieldIter::next( void ) noexcept
{
  this->field_start = this->field_end;
  this->field_end   = this->iter_msg().msg_end;
  this->field_index++;
  if ( this->field_start >= this->field_end )
    return Err::NOT_FOUND;
  return this->unpack();
}

int
TibFieldIter::unpack( void ) noexcept
{
  const uint8_t * buf = (uint8_t *) this->iter_msg().msg_buf;
  size_t          i   = this->field_start;
  uint8_t         typekey;

  if ( i >= this->field_end )
    goto bad_bounds;
  this->name_len = buf[ i++ ];
  i += this->name_len;
  if ( i + 1 >= this->field_end )
    goto bad_bounds;
  typekey = buf[ i++ ];
  this->type = typekey & 0xfU;

  if ( ( typekey & 0x80 ) == 0 )
    this->size = buf[ i++ ];
  else {
    if ( i + 4 >= this->field_end )
      goto bad_bounds;
    this->size = get_u32<MD_BIG>( &buf[ i ] );
    i += 4;
  }
  this->data_off = (uint32_t) i;
  i += this->size;

  switch ( this->type ) {
    case TIB_STRING:
    case TIB_OPAQUE:
    case TIB_IPDATA:
    case TIB_INT:
    case TIB_UINT:
    case TIB_REAL:
    case TIB_BOOLEAN:
      if ( ( typekey & 0x40 ) == 0 ) {
        this->hint_type = TIB_NODATA;
        this->hint_size = 0;
        break;
      }
      /* FALLTHRU */
    case TIB_PARTIAL:
    case TIB_ARRAY:
      if ( i + 1 >= this->field_end )
        goto bad_bounds;
      typekey = buf[ i++ ];
      this->hint_type = typekey & 0xf;
      if ( ( typekey & 0x80 ) == 0 )
        this->hint_size = buf[ i++ ];
      else {
        if ( i + 4 >= this->field_end )
          goto bad_bounds;
        this->hint_size = get_u32<MD_BIG>( &buf[ i ] );
        i += 4;
      }

      switch ( this->hint_type ) {
        case TIB_STRING:
        case TIB_OPAQUE:
        case TIB_IPDATA:
        case TIB_INT:
        case TIB_UINT:
        case TIB_REAL:
        case TIB_BOOLEAN:
          if ( this->type != TIB_PARTIAL && this->type != TIB_ARRAY )
            i += this->hint_size;
          break;

        default:
          return Err::BAD_FIELD_TYPE;
      }
      break;

    case TIB_MESSAGE:
      this->hint_type = TIB_NODATA;
      this->hint_size = 0;
      break;

    default:
      return Err::BAD_FIELD_TYPE;
  }

  if ( i > this->field_end ) {
  bad_bounds:;
    return Err::BAD_FIELD_BOUNDS;
  }
  this->field_end = i;
  /* determine if decimal */
  if ( this->hint_size > 0 ) {
    if ( this->type == TIB_REAL || this->type == TIB_STRING ||
         this->type == TIB_INT || this->type == TIB_UINT ) {
      MDReference href;
      if ( this->get_hint_reference( href ) == 0 ) {
        if ( this->type == TIB_REAL || this->type == TIB_STRING ) {
          uint32_t hint = get_int<uint32_t>( href );
          if ( this->type == TIB_REAL ) {
            if ( hint <= TIB_HINT_DENOM_256 ||
                 ( hint >= 16 && hint < 32 ) ||
                 hint == TIB_HINT_BLANK_VALUE ) {
              this->type = MD_DECIMAL;
              this->decimal_hint = hint;
            }
          }
          else {
            switch ( hint ) {
              case TIB_HINT_DATE_TYPE:
              case TIB_HINT_MF_DATE_TYPE:
                this->type = MD_DATE;
                break;
              case TIB_HINT_TIME_TYPE:
              case TIB_HINT_MF_TIME_TYPE:
              case TIB_HINT_MF_TIME_SECONDS:
                this->type = MD_TIME;
                break;
              default:
                break;
            }
          }
        }
        else if ( this->type == TIB_INT || this->type == TIB_UINT ) {
          uint32_t hint = get_int<uint32_t>( href );
          if ( hint == TIB_HINT_MF_ENUM )
            this->type = MD_ENUM;
        }
      }
    }
  }
  return 0;
}

bool
TibMsg::set_decimal( MDDecimal &dec,  double val,  uint8_t tib_hint ) noexcept
{
  uint32_t precision = 256;
  double   denom     = 0;

  switch ( tib_hint ) {
    case TIB_HINT_BLANK_VALUE:
      precision = 0;
      dec.ival = 0;
      dec.hint = MD_DEC_NULL;
      return true;
    case TIB_HINT_NONE:
      dec.set_real( val ); /* determine the hint, NONE is ambiguous */
      return true;
    case TIB_HINT_DENOM_2:
      denom = 2;   dec.hint = MD_DEC_FRAC_2; break;
    case TIB_HINT_DENOM_4:
      denom = 4;   dec.hint = MD_DEC_FRAC_4; break;
    case TIB_HINT_DENOM_8:
      denom = 8;   dec.hint = MD_DEC_FRAC_8; break;
    case TIB_HINT_DENOM_16:
      denom = 16;  dec.hint = MD_DEC_FRAC_16; break;
    case TIB_HINT_DENOM_32:
      denom = 32;  dec.hint = MD_DEC_FRAC_32; break;
    case TIB_HINT_DENOM_64:
      denom = 64;  dec.hint = MD_DEC_FRAC_64; break;
    case TIB_HINT_DENOM_128:
      denom = 128; dec.hint = MD_DEC_FRAC_128; break;
    case TIB_HINT_DENOM_256:
      denom = 256; dec.hint = MD_DEC_FRAC_256; break;

    case TIB_HINT_PRECISION_1: case TIB_HINT_PRECISION_2:
    case TIB_HINT_PRECISION_3: case TIB_HINT_PRECISION_4:
    case TIB_HINT_PRECISION_5: case TIB_HINT_PRECISION_6:
    case TIB_HINT_PRECISION_7: case TIB_HINT_PRECISION_8:
    case TIB_HINT_PRECISION_9:
    default:
      if ( tib_hint >= 16 && tib_hint < 16 + 32 )
        precision = tib_hint - 16;
      break;
  }
  if ( denom != 0 ) {
    dec.ival = (int64_t) ( val * denom );
    return true;
  }
  if ( precision != 256 ) {
    if ( precision == 0 ) {
      dec.ival = (int64_t) val;
      dec.hint = MD_DEC_INTEGER;;
    }
    else {
      static const double dec_powers_f[] = MD_DECIMAL_POWERS;
      uint32_t n = sizeof( dec_powers_f ) / sizeof( dec_powers_f[ 0 ] ) - 1;
      double p;

      if ( precision <= n ) {
        p = dec_powers_f[ precision ];
      }
      else {
        p = dec_powers_f[ n ];
        while ( n < precision ) {
          p *= 10;
          n++;
        }
      }
      dec.ival = (int64_t) ( val * p );
      dec.hint = -10 - (int8_t) precision;
    }
    return true;
  }
  dec.zero();
  return false;
}

bool
TibMsgWriter::resize( size_t len ) noexcept
{
  static const size_t max_size = 0x3fffffff; /* 1 << 30 - 1 == 1073741823 */
  if ( this->err != 0 )
    return false;
  TibMsgWriter *p = this; 
  for ( ; p->parent != NULL; p = p->parent )
    ;
  if ( len > max_size )
    return false;
  size_t old_len = p->buflen, 
         new_len = old_len + ( this->buflen + len + this->hdrlen - this->off );
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

  for ( TibMsgWriter *c = this; c != p; c = c->parent ) {
    if ( c->buf >= old_buf && c->buf < &old_buf[ old_len ] ) {
      size_t off = c->buf - old_buf;  
      c->buf = &new_buf[ off ];
      c->buflen = end - c->buf;
    }
  }
  return this->off + this->hdrlen + len <= this->buflen;
}

namespace {
struct TnameWriter {
  const char * fname;
  size_t       fname_len,
               zpad; /* if fname is not null terminated, zpad = 1 */

  TnameWriter( const char *fn,  size_t fn_len )
      : fname( fn ), fname_len( fn_len ), zpad( 0 ) {
    if ( fn_len > 0 && fn[ fn_len - 1 ] != '\0' )
      this->zpad = 1;
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

TibMsgWriter &
TibMsgWriter::append_ref( const char *fname,  size_t fname_len,
                          MDReference &mref ) noexcept
{
  MDReference href;
  href.zero();
  return this->append_ref( fname, fname_len, mref, href );
}

TibMsgWriter &
TibMsgWriter::append_ref( const char *fname,  size_t fname_len,
                          MDReference &mref,  MDReference &href ) noexcept
{
  int status;
  if ( mref.ftype == MD_DECIMAL ) {
    MDDecimal dec;
    if ( (status = dec.get_decimal( mref )) != 0 )
      return this->error( status );
    return this->append_decimal( fname, fname_len, dec );
  }
  else if ( mref.ftype == MD_DATE ) {
    MDDate date;
    if ( (status = date.get_date( mref )) != 0 )
      return this->error( status );
    return this->append_date( fname, fname_len, date );
  }
  else if ( mref.ftype == MD_TIME ) {
    MDTime time;
    if ( (status = time.get_time( mref )) != 0 )
      return this->error( status );
    if ( href.ftype == MD_UINT || href.ftype == MD_INT ) {
      uint16_t res = get_uint<uint16_t>( href );
      if ( res == TIB_HINT_MF_TIME_SECONDS /* 260 */ )
        time.resolution = ( time.resolution & MD_RES_NULL ) | MD_RES_SECONDS;
      if ( res == TIB_HINT_MF_TIME_TYPE /* 259 */ )
        time.resolution = ( time.resolution & MD_RES_NULL ) | MD_RES_MINUTES;
    }
    return this->append_time( fname, fname_len, time );
  }
  size_t fsize = mref.fsize;
  if ( mref.ftype == MD_STRING &&
       ( fsize == 0 || mref.fptr[ fsize - 1 ] != '\0' ) )
    fsize++;

  TnameWriter fwr( fname, fname_len );
  size_t len = fwr.len() + 1 + ( fsize <= 0xffU ? 1 : 4 ) + fsize;

  if ( fwr.too_big() )
    return this->error( Err::BAD_NAME );
  if ( ! this->has_space( len ) )
    return this->error( Err::NO_SPACE );

  uint8_t * ptr = this->buf;
  ptr += fwr.copy( this->buf, this->off + this->hdrlen );

  uint8_t x = (uint8_t) ( mref.ftype & 0xfU ) | /* one byte size */
              (uint8_t) ( fsize <= 0xffU ? 0 : 0x80U ); /* 4 byte size */
  if ( mref.ftype >= TIB_MAX_TYPE ) {
    if ( mref.ftype == MD_ENUM || mref.ftype == MD_STAMP )
      x = ( x & ~0xf ) | MD_UINT;
    else
      x = ( x & ~0xf ) | MD_OPAQUE;
  }
  ptr[ 0 ] = x;
  if ( href.ftype != MD_NODATA || mref.ftype == MD_PARTIAL ||
       mref.ftype == MD_ENUM )
    ptr[ 0 ] |= 0x40;
  if ( fsize <= 0xffU ) {
    ptr[ 1 ] = (uint8_t) fsize;
    ptr = &ptr[ 2 ];
  }
  else {
    ptr[ 1 ] = (uint8_t) ( ( fsize >> 24 ) & 0xffU );
    ptr[ 2 ] = (uint8_t) ( ( fsize >> 16 ) & 0xffU );
    ptr[ 3 ] = (uint8_t) ( ( fsize >> 8 ) & 0xffU );
    ptr[ 4 ] = (uint8_t) ( fsize & 0xffU );
    ptr = &ptr[ 5 ];
  }
  /* invert endian, for little -> big */
  if ( mref.fendian != MD_BIG && is_endian_type( mref.ftype ) ) {
    size_t off = fsize;
    ptr[ 0 ] = mref.fptr[ --off ];
    if ( off > 0 ) {
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
  }
  else {
    ::memcpy( ptr, mref.fptr, mref.fsize );
    if ( fsize > mref.fsize )
      ptr[ mref.fsize ] = '\0';
  }
  this->off += len;
  if ( mref.ftype == MD_PARTIAL ) {
    /*size_t plen = ( ( mref.fentrysz <= 0xffU ) ? 2 : 5 );*/
    /* need hint data */
    if ( ! this->has_space( 2 ) )
      return this->error( Err::NO_SPACE );

    ptr = &this->buf[ this->off + this->hdrlen ];
    ptr[ 0 ] = MD_UINT;
    ptr[ 1 ] = (uint8_t) mref.fentrysz;
    this->off += 2;
  }
  else if ( mref.ftype == MD_ENUM ) {
    if ( ! this->has_space( 4 ) )
      return this->error( Err::NO_SPACE );

    ptr = &this->buf[ this->off + this->hdrlen ];
    ptr[ 0 ] = MD_UINT;
    ptr[ 1 ] = 2;
    ptr[ 2 ] = (uint8_t) ( TIB_HINT_MF_ENUM >> 8 );
    ptr[ 3 ] = (uint8_t) ( TIB_HINT_MF_ENUM & 0xff );
    this->off += 4;
  }
  else if ( href.ftype != MD_NODATA ) {
    if ( ! this->has_space( href.fsize + 2 ) )
      return this->error( Err::NO_SPACE );

    ptr = &this->buf[ this->off + this->hdrlen ];
    ptr[ 0 ] = href.ftype;
    ptr[ 1 ] = (uint8_t) href.fsize;
    ::memcpy( &ptr[ 2 ], href.fptr, href.fsize );
    this->off += href.fsize + 2;
  }
  return *this;
}

TibMsgWriter &
TibMsgWriter::append_decimal( const char *fname,  size_t fname_len,
                              MDDecimal &dec ) noexcept
{
  TnameWriter fwr( fname, fname_len );
  size_t len = fwr.len() + 1 + 1 + 8 + 3;
  double val;
  int    status;

  if ( fwr.too_big() )
    return this->error( Err::BAD_NAME );
  if ( ! this->has_space( len ) )
    return this->error( Err::NO_SPACE );
  if ( (status = dec.get_real( val )) != 0 )
    return this->error( status );

  uint8_t * ptr = this->buf;
  ptr += fwr.copy( this->buf, this->off + this->hdrlen );

  ptr[ 0 ] = (uint8_t) MD_REAL | 0x40U; /* real with hint */
  ptr[ 1 ] = (uint8_t) 8;
  ptr = &ptr[ 2 ];

  if ( md_endian != MD_LITTLE )
    ::memcpy( ptr, &val, 8 );
  else {
    uint8_t * fptr = (uint8_t *) (void *) &val;
    for ( size_t i = 0; i < 8; i++ )
      ptr[ i ] = fptr[ 7 - i ];
  }
  ptr = &ptr[ 8 ];
  ptr[ 0 ] = (uint8_t) MD_UINT;
  ptr[ 1 ] = 1;

  uint8_t h; /* translate md hint into tib hint */
  switch ( dec.hint ) {
    default:
      if ( dec.hint == MD_DEC_INTEGER ) {
        h = TIB_HINT_NONE;
        break;
      }
      else if ( dec.hint <= MD_DEC_LOGn10_1 ) {
        h = -( (int8_t) dec.hint - MD_DEC_LOGn10_1 ) + TIB_HINT_PRECISION_1;
        break;
      }
      else if ( dec.hint >= MD_DEC_FRAC_2 &&
                dec.hint <= MD_DEC_FRAC_512 ) {
        h = ( (int8_t) dec.hint - MD_DEC_FRAC_2 + TIB_HINT_DENOM_2 );
        break;
      }
      /* FALLTHRU */
    case MD_DEC_NNAN:
    case MD_DEC_NAN:
    case MD_DEC_NINF:
    case MD_DEC_INF:  h = TIB_HINT_NONE; break;
    case MD_DEC_NULL: h = TIB_HINT_BLANK_VALUE; break;
  }
  ptr[ 2 ] = h;
  this->off += len;

  return *this;
}

TibMsgWriter &
TibMsgWriter::append_time( const char *fname,  size_t fname_len,
                           MDTime &time ) noexcept
{
  TnameWriter fwr( fname, fname_len );
  char   sbuf[ 32 ];
  size_t n   = time.get_string( sbuf, sizeof( sbuf ) );
  size_t len = fwr.len() + 1 + 1 + n + 1 + 4;

  if ( fwr.too_big() )
    return this->error( Err::BAD_NAME );
  if ( ! this->has_space( len ) )
    return this->error( Err::NO_SPACE );

  uint8_t * ptr = this->buf;
  ptr += fwr.copy( this->buf, this->off + this->hdrlen );

  ptr[ 0 ] = (uint8_t) MD_STRING | 0x40U; /* string with hint */
  ptr[ 1 ] = (uint8_t) ( n + 1 );
  ptr = &ptr[ 2 ];

  ::memcpy( ptr, sbuf, n + 1 );
  ptr = &ptr[ n + 1 ];
  ptr[ 0 ] = (uint8_t) MD_UINT;
  ptr[ 1 ] = 2; /* uint size */
  uint16_t hint = 
    ( ( time.resolution & ~MD_RES_NULL ) >= MD_RES_MINUTES ) ?
      TIB_HINT_MF_TIME_TYPE : TIB_HINT_MF_TIME_SECONDS;
  ptr[ 2 ] = (uint8_t) ( hint >> 8 );
  ptr[ 3 ] = (uint8_t) ( hint & 0xff );
  this->off += len;

  return *this;
}

TibMsgWriter &
TibMsgWriter::append_date( const char *fname,  size_t fname_len,
                           MDDate &date ) noexcept
{
  TnameWriter fwr( fname, fname_len );
  char   sbuf[ 32 ];
  size_t n   = date.get_string( sbuf, sizeof( sbuf ) );
  size_t len = fwr.len() + 1 + 1 + n + 1 + 4;

  if ( fwr.too_big() ) 
    return this->error( Err::BAD_NAME );
  if ( ! this->has_space( len ) )
    return this->error( Err::NO_SPACE );

  uint8_t * ptr = this->buf;
  ptr += fwr.copy( this->buf, this->off + this->hdrlen );

  ptr[ 0 ] = (uint8_t) MD_STRING | 0x40U; /* string with hint */
  ptr[ 1 ] = (uint8_t) ( n + 1 );
  ptr = &ptr[ 2 ];

  ::memcpy( ptr, sbuf, n + 1 );
  ptr = &ptr[ n + 1 ];
  ptr[ 0 ] = (uint8_t) MD_UINT;
  ptr[ 1 ] = 2; /* uint size */
  ptr[ 2 ] = (uint8_t) ( TIB_HINT_MF_DATE_TYPE >> 8 );
  ptr[ 3 ] = (uint8_t) ( TIB_HINT_MF_DATE_TYPE & 0xff ); /* hint date */
  this->off += len;

  return *this;
}

TibMsgWriter &
TibMsgWriter::append_enum( const char *fname,  size_t fname_len,
                           MDEnum &enu ) noexcept
{
  TnameWriter fwr( fname, fname_len );
  size_t len = fwr.len() + 1 + 1 + enu.disp_len + 1 + 4;

  if ( fwr.too_big() )
    return this->error( Err::BAD_NAME );
  if ( ! this->has_space( len ) )
    return this->error( Err::NO_SPACE );

  uint8_t * ptr = this->buf;
  ptr += fwr.copy( this->buf, this->off + this->hdrlen );

  ptr[ 0 ] = (uint8_t) MD_STRING | 0x40U; /* string with hint */
  ptr[ 1 ] = (uint8_t) ( enu.disp_len + 1 );
  ptr = &ptr[ 2 ];

  ::memcpy( ptr, enu.disp, enu.disp_len );
  ptr[ enu.disp_len ] = '\0';
  ptr = &ptr[ enu.disp_len + 1 ];
  ptr[ 0 ] = (uint8_t) MD_UINT;
  ptr[ 1 ] = 2; /* uint size */
  ptr[ 2 ] = (uint8_t) ( TIB_HINT_MF_ENUM >> 8 );
  ptr[ 3 ] = (uint8_t) ( TIB_HINT_MF_ENUM & 0xff ); /* hint date */
  this->off += len;

  return *this;
}

TibMsgWriter &
TibMsgWriter::append_iter( MDFieldIter *iter ) noexcept
{
  size_t len = iter->field_end - iter->field_start;

  if ( ! this->has_space( len ) )
    return this->error( Err::NO_SPACE );

  uint8_t * ptr = &this->buf[ this->off + this->hdrlen ];
  ::memcpy( ptr, &((uint8_t *) iter->iter_msg().msg_buf)[ iter->field_start ], len );
  this->off += len;
  return *this;
}

TibMsgWriter &
TibMsgWriter::append_msg( const char *fname,  size_t fname_len,
                          TibMsgWriter &submsg ) noexcept
{
  TnameWriter fwr( fname, fname_len );
  size_t      len     = fwr.len() + 1,
              szbytes = 4;

  len += szbytes;
  if ( fwr.too_big() )
    return this->error( Err::BAD_NAME );
  if ( ! this->has_space( len ) )
    return this->error( Err::NO_SPACE );

  uint8_t * ptr = this->buf;
  ptr += fwr.copy( this->buf, this->off + this->hdrlen );
  ptr[ 0 ] = 0x80 | MD_MESSAGE;
  this->off  = &ptr[ 1 ] - this->buf;
  this->off -= this->hdrlen;

  submsg.buf    = &ptr[ 1 ];
  submsg.off    = 0;
  submsg.hdrlen = 4;
  submsg.buflen = &this->buf[ this->buflen ] - submsg.buf;
  submsg.err    = 0;
  submsg.parent = this;
  return submsg;
}

int
TibMsgWriter::convert_msg( MDMsg &msg,  bool skip_hdr ) noexcept
{
  MDFieldIter *iter;
  int status;
  if ( (status = msg.get_field_iter( iter )) == 0 &&
       (status = iter->first()) == 0 ) {
    do {
      MDName      n;
      MDReference mref, href;
      MDEnum      enu;
      if ( (status = iter->get_name( n )) == 0 &&
           (status = iter->get_reference( mref )) == 0 ) {
        if ( skip_hdr && is_sass_hdr( n ) )
          continue;
        if ( mref.ftype == MD_ENUM ) {
          if ( iter->get_enum( mref, enu ) == 0 )
            this->append_enum( n.fname, n.fnamelen, enu );
          else
            this->append_uint( n.fname, n.fnamelen,
                               get_uint<uint16_t>( mref ) );
        }
        else if ( mref.ftype == MD_MESSAGE ) {
          TibMsgWriter submsg( this->mem(), NULL, 0 );
          MDMsg * msg2 = NULL;
          this->append_msg( n.fname, n.fnamelen, submsg );
          status = this->err;
          if ( status == 0 )
            status = msg.get_sub_msg( mref, msg2, iter );
          if ( status == 0 ) {
            status = submsg.convert_msg( *msg2, false );
            if ( status == 0 )
              this->update_hdr( submsg );
          }
        }
        else {
          iter->get_hint_reference( href );
          this->append_ref( n.fname, n.fnamelen, mref, href );
        }
        status = this->err;
      }
      if ( status != 0 )
        break;
    } while ( (status = iter->next()) == 0 );
  }
  if ( status != Err::NOT_FOUND )
    return status;
  return 0;
}

