#ifndef __rai_raimd__md_int_h__
#define __rai_raimd__md_int_h__

#include <math.h>
#ifdef _MSC_VER
#include <intrin.h>
/* disable delete() constructor (4291), fopen deprecated (4996) */
#pragma warning( disable : 4291 4996 )
#endif

#include <raimd/md_int_c.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _MSC_VER
inline uint32_t
md_ctzl( uint64_t val )
{
  unsigned long z;
  if ( _BitScanForward64( &z, val ) )
    return (uint32_t) z;
  return 64;
}
inline uint32_t
md_clzl( uint64_t val )
{
  unsigned long z;
  if ( _BitScanReverse64( &z, val ) )
    return (uint32_t) ( 63 - z );
  return 64;
}
typedef ptrdiff_t ssize_t;
#else
inline uint32_t md_ctzl( uint64_t val ) { return __builtin_ctzll( val ); }
inline uint32_t md_clzl( uint64_t val ) { return __builtin_clzll( val ); }
#endif

#ifdef __cplusplus 
} 
  
namespace rai {
namespace md {

template<class Int>
static inline Int to_ival( const void *val ) {
  Int i;
  ::memcpy( &i, val, sizeof( i ) );
  return i;
}

template<MDEndian end>
static inline uint16_t get_u16( const void *val ) {
  return end == MD_LITTLE ? get_u16_md_little( val ) : get_u16_md_big( val );
}

template<MDEndian end>
static inline void set_u16( void *val,  uint16_t i ) {
  end == MD_LITTLE ? set_u16_md_little( val, i ) : set_u16_md_big( val, i );
}

template<MDEndian end>
static inline int16_t get_i16( const void *val ) {
  return (int16_t) get_u16<end>( val );
}

template<MDEndian end>
static inline uint32_t get_u32( const void *val ) {
  return end == MD_LITTLE ? get_u32_md_little( val ) : get_u32_md_big( val );
}

template<MDEndian end>
static inline void set_u32( void *val,  uint32_t i ) {
  end == MD_LITTLE ? set_u32_md_little( val, i ) : set_u32_md_big( val, i );
}

template<MDEndian end>
static inline int32_t get_i32( const void *val ) {
  return (int32_t) get_u32<end>( val );
}

template<MDEndian end>
static inline float get_f32( const void *val ) {
  union { uint32_t u; float f; } x;
  x.u = get_u32<end>( val );
  return x.f;
}

template<MDEndian end>
static inline uint64_t get_u64( const void *val ) {
  return end == MD_LITTLE ? get_u64_md_little( val ) : get_u64_md_big( val );
}

template<MDEndian end>
static inline void set_u64( void *val,  uint64_t i ) {
  end == MD_LITTLE ? set_u64_md_little( val, i ) : set_u64_md_big( val, i );
}

template<MDEndian end>
static inline int64_t get_i64( const void *val ) {
  return (int64_t) get_u64<end>( val );
}

template<MDEndian end>
static inline double get_f64( const void *val ) {
  union { uint64_t u; double f; } x;
  x.u = get_u64<end>( val );
  return x.f;
}

template<class T>
static inline T get_uint( const void *val, MDEndian end ) {
  if ( sizeof( T ) == sizeof( uint16_t ) )
    return (T) ( end == MD_LITTLE ? get_u16<MD_LITTLE>( val ): get_u16<MD_BIG>( val ) );
  if ( sizeof( T ) == sizeof( uint32_t ) )
    return (T) ( end == MD_LITTLE ? get_u32<MD_LITTLE>( val ): get_u32<MD_BIG>( val ) );
  if ( sizeof( T ) == sizeof( uint64_t ) )
    return (T) ( end == MD_LITTLE ? get_u64<MD_LITTLE>( val ): get_u64<MD_BIG>( val ) );
  return (T) ((const uint8_t *) val)[ 0 ];
}

template<class T>
static inline T get_uint( const MDReference &mref ) {
  if ( mref.fsize == sizeof( uint16_t ) )
    return (T) get_uint<uint16_t>( mref.fptr, mref.fendian );
  if ( mref.fsize == sizeof( uint32_t ) )
    return (T) get_uint<uint32_t>( mref.fptr, mref.fendian );
  if ( mref.fsize == sizeof( uint64_t ) )
    return (T) get_uint<uint64_t>( mref.fptr, mref.fendian );
  return (T) get_uint<uint8_t>( mref.fptr, mref.fendian );
}

template<class T>
static inline T get_int( const void *val, MDEndian end ) {
  if ( sizeof( T ) == sizeof( int16_t ) )
    return (T) ( end == MD_LITTLE ? get_i16<MD_LITTLE>( val ): get_i16<MD_BIG>( val ) );
  if ( sizeof( T ) == sizeof( int32_t ) )
    return (T) ( end == MD_LITTLE ? get_i32<MD_LITTLE>( val ): get_i32<MD_BIG>( val ) );
  if ( sizeof( T ) == sizeof( int64_t ) )
    return (T) ( end == MD_LITTLE ? get_i64<MD_LITTLE>( val ): get_i64<MD_BIG>( val ) );
  return ((const int8_t *) val)[ 0 ];
}

template<class T>
static inline T get_int( const MDReference &mref ) {
  if ( mref.fsize == sizeof( int16_t ) )
    return (T) get_int<int16_t>( mref.fptr, mref.fendian );
  if ( mref.fsize == sizeof( int32_t ) )
    return (T) get_int<int32_t>( mref.fptr, mref.fendian );
  if ( mref.fsize == sizeof( int64_t ) )
    return (T) get_int<int64_t>( mref.fptr, mref.fendian );
  return (T) get_int<int8_t>( mref.fptr, mref.fendian );
}

template<class T>
static inline T get_float( const void *val, MDEndian end ) {
  if ( sizeof( T ) == sizeof( float ) )
    return (T)( end == MD_LITTLE ? get_f32<MD_LITTLE>( val ): get_f32<MD_BIG>( val ) );
  if ( sizeof( T ) == sizeof( double ) )
    return (T)( end == MD_LITTLE ? get_f64<MD_LITTLE>( val ): get_f64<MD_BIG>( val ) );
  return 0;
}

template<class T>
static inline T get_float( const MDReference &mref ) {
  if ( mref.fsize == sizeof( float ) )
    return (T) get_float<float>( mref.fptr, mref.fendian );
  if ( mref.fsize == sizeof( double ) )
    return (T) get_float<double>( mref.fptr, mref.fendian );
  return 0;
}

static inline void xchgb( uint8_t &a,  uint8_t &b ) { uint8_t x = a; a = b; b = x; }
static inline void flip_endian( void *p,  size_t sz ) {
  if ( sz == 2 )
    set_u16<MD_BIG>( p, get_u16<MD_LITTLE>( p ) );
  else if ( sz == 4 )
    set_u32<MD_BIG>( p, get_u32<MD_LITTLE>( p ) );
  else if ( sz == 8 )
    set_u64<MD_BIG>( p, get_u64<MD_LITTLE>( p ) );
}

static inline uint16_t parse_u16( const char *val,  char **end ) {
  return (uint16_t) ::strtoul( val, end, 0 );
}
static inline int16_t parse_i16( const char *val,  char **end ) {
  return (int16_t) ::strtol( val, end, 0 );
}
static inline uint32_t parse_u32( const char *val,  char **end ) {
  return ::strtoul( val, end, 0 );
}
static inline int32_t parse_i32( const char *val,  char **end ) {
  return ::strtol( val, end, 0 );
}
static inline float parse_f32( const char *val,  char **end ) {
  return ::strtof( val, end );
}
static inline uint64_t parse_u64( const char *val,  char **end ) {
  return ::strtoull( val, end, 0 );
}
static inline int64_t parse_i64( const char *val,  char **end ) {
  return ::strtoll( val, end, 0 );
}
static inline double parse_f64( const char *val,  char **end ) {
  return ::strtod( val, end );
}
static inline uint8_t parse_bool( const char *val,  size_t len ) {
  if ( len > 0 ) {
    switch ( val[ 0 ] ) {
      case 't': case 'T': case '1': return 1;
      case '0': case 'f': case 'F': default:  return 0;
    }
  }
  return 0;
}

template<class T>
static inline T parse_int( const char *val,  char **end ) {
  if ( sizeof( T ) == sizeof( int16_t ) )
    return parse_i16( val, end );
  if ( sizeof( T ) == sizeof( int32_t ) )
    return parse_i32( val, end );
  if ( sizeof( T ) == sizeof( int64_t ) )
    return parse_i64( val, end );
  return 0;
}

template<class T>
static inline T parse_uint( const char *val,  char **end ) {
  if ( sizeof( T ) == sizeof( uint16_t ) )
    return parse_u16( val, end );
  if ( sizeof( T ) == sizeof( uint32_t ) )
    return parse_u32( val, end );
  if ( sizeof( T ) == sizeof( uint64_t ) )
    return parse_u64( val, end );
  return 0;
}

template<class T>
static inline T parse_float( const char *val,  char **end ) {
  if ( sizeof( T ) == sizeof( float ) )
    return parse_f32( val, end );
  if ( sizeof( T ) == sizeof( double ) )
    return parse_f64( val, end );
  return 0;
}

template<class T>
static inline int cvt_number( const MDReference &mref, T &val ) {
  switch ( mref.ftype ) {
    case MD_BOOLEAN:
    case MD_ENUM:
    case MD_UINT:   val = (T) get_uint<uint64_t>( mref ); return 0;
    case MD_INT:    val = (T) get_int<int64_t>( mref ); return 0;
    case MD_REAL:   val = (T) get_float<double>( mref ); return 0;
    case MD_STRING: val = (T) parse_u64( (char *) mref.fptr, NULL ); return 0;
    case MD_DECIMAL: {
      MDDecimal dec;
      double f;
      dec.get_decimal( mref );
      if ( dec.hint == MD_DEC_INTEGER )
        val = (T) dec.ival;
      else {
        dec.get_real( f );
        val = (T) f;
      }
      return 0;
    }
    default: break;
  }
  return Err::BAD_CVT_NUMBER;
}

static inline bool is_mask( uint32_t m ) { return ( m & ( m + 1 ) ) == 0; }

/* integer to string routines */
static inline uint64_t nega( int64_t v ) {
  if ( (uint64_t) v == ( (uint64_t) 1 << 63 ) )
    return ( (uint64_t) 1 << 63 );
  return (uint64_t) -v;
}

static inline size_t uint_digs( uint64_t v ) {
  for ( size_t n = 1; ; n += 4 ) {
    if ( v < 10 )    return n;
    if ( v < 100 )   return n + 1;
    if ( v < 1000 )  return n + 2;
    if ( v < 10000 ) return n + 3;
    v /= 10000;
  }
}

static inline size_t int_digs( int64_t v ) {
  if ( v < 0 ) return 1 + uint_digs( nega( v ) );
  return uint_digs( v );
}
/* does not null terminate (most strings have lengths, not nulls) */
static inline size_t uint_str( uint64_t v,  char *buf,  size_t len ) {
  for ( size_t pos = len; v >= 10; ) {
    const uint64_t q = v / 10, r = v % 10;
    buf[ --pos ] = '0' + (char) r;
    v = q;
  }
  buf[ 0 ] = '0' + (char) v;
  return len;
}

static inline size_t uint_str( uint64_t v,  char *buf ) {
  return uint_str( v, buf, uint_digs( v ) );
}

static inline size_t int_str( int64_t v,  char *buf,  size_t len ) {
  if ( v < 0 ) {
    buf[ 0 ] = '-';
    return 1 + uint_str( nega( v ), &buf[ 1 ], len - 1 );
  }
  return uint_str( v, buf, len );
}

static inline size_t int_str( int64_t v,  char *buf ) {
  return int_str( v, buf, int_digs( v ) );
}

static inline size_t float_str( double f,  char *buf ) {
  size_t places = 14,
         off    = 0;
  if ( isnan( f ) ) {
    if ( f < 0 )
      buf[ off++ ] = '-';
    ::memcpy( &buf[ off ], "NaN", 4 );
    return off + 3;
  }
  if ( isinf( f ) ) {
    if ( f < 0 )
      buf[ off++ ] = '-';
    ::memcpy( &buf[ off ], "Inf", 4 );
    return off + 3;
  }

  if ( f < 0.0 ) {
    buf[ off++ ] = '-';
    f = -f;
  }
  /* find the integer and the fraction + 1.0 */
  double       integral,
               tmp,
               p10,
               decimal;
  const double fraction      = modf( (double) f, &integral );
  uint64_t     integral_ival = (uint64_t) integral,
               decimal_ival;

  static const double powtab[] = {
    1.0, 10.0, 100.0, 1000.0, 10000.0,
    100000.0, 1000000.0, 10000000.0,
    100000000.0, 1000000000.0, 10000000000.0,
    100000000000.0, 1000000000000.0, 10000000000000.0,
    100000000000000.0
    };
  static const size_t ptsz = sizeof( powtab ) / sizeof( powtab[ 0 ] );
  if ( places < ptsz )
    p10 = powtab[ places ];
  else
    p10 = pow( (double) 10.0, (double) places );

  /* multiply fraction + 1 * places wanted (.25 + 1.0) * 1000.0 = 1250 */
  tmp = modf( ( fraction + 1.0 ) * p10, &decimal );
  /* round up, if fraction of decimal places >= 0.5 */
  if ( tmp >= 0.5 ) {
    decimal += 1.0;
    if ( decimal >= p10 * 2.0 )
      integral_ival++;
  }
  else if ( decimal >= p10 * 2.0 ) {
    decimal--;
  }

  off += uint_str( integral_ival, &buf[ off ] );
  /* convert the decimal to 1ddddd, the 1 is replaced with a '.' below */
  decimal_ival = (uint64_t) decimal;

  while ( decimal_ival >= 10000 && decimal_ival % 10000 == 0 )
    decimal_ival /= 10000;
  while ( decimal_ival > 1 && decimal_ival % 10 == 0 )
    decimal_ival /= 10;
  /* at least one zero */
  if ( decimal_ival <= 2 ) {
    buf[ off++ ] = '.';
    buf[ off++ ] = '0';
  }
  else {
    size_t pt = off;
    off += int_str( decimal_ival, &buf[ off ] );
    buf[ pt ] = '.';
  }
  return off;
}

static inline char hexchar( uint8_t n ) {
  return (char) ( n <= 9 ? ( n + '0' ) : ( n - 10 + 'a' ) );
}
static inline char hexchar2( uint8_t n ) { /* upper case */
  return (char) ( n <= 9 ? ( n + '0' ) : ( n - 10 + 'A' ) );
}
static inline void hexstr16( uint16_t n,  char *s ) {
  int j = 0;
  for ( int i = 16; i > 0; ) {
    i -= 4;
    s[ j++ ] = hexchar2( ( n >> i ) & 0xf );
  }
}
static inline void hexstr32( uint32_t n,  char *s ) {
  int j = 0;
  for ( int i = 32; i > 0; ) {
    i -= 4;
    s[ j++ ] = hexchar2( ( n >> i ) & 0xf );
  }
}
static inline void hexstr64( uint64_t n,  char *s ) {
  int j = 0;
  for ( int i = 64; i > 0; ) {
    i -= 4;
    s[ j++ ] = hexchar2( ( n >> i ) & 0xf );
  }
}

static inline int to_string( const MDReference &mref, char *sbuf,
                             size_t &slen ) {
  switch ( mref.ftype ) {
    case MD_UINT:
      slen = uint_str( get_uint<uint64_t>( mref ), sbuf ); return 0;
    case MD_INT:
      slen = int_str( get_int<int64_t>( mref ), sbuf ); return 0;
    case MD_REAL:
      slen = float_str( get_float<double>( mref ), sbuf ); return 0;
    case MD_DECIMAL: {
      MDDecimal dec;
      dec.get_decimal( mref );
      slen = dec.get_string( sbuf, slen );
      return 0;
    }
    case MD_DATE: {
      MDDate date;
      date.get_date( mref );
      slen = date.get_string( sbuf, slen );
      return 0;
    }
    case MD_TIME: {
      MDTime time;
      time.get_time( mref );
      slen = time.get_string( sbuf, slen );
      return 0;
    }
    case MD_BOOLEAN: {
      if ( get_uint<uint8_t>( mref ) ) {
        slen = 4;
        ::memcpy( sbuf, "true", 5 );
      }
      else {
        slen = 5;
        ::memcpy( sbuf, "false", 6 );
      }
      return 0;
    }
    default:
      return Err::BAD_CVT_STRING;
  }
}
/* 0 -> 0xfd : 1 byte, 0xfe <short> : 3 bytes, 0xff <int> : 5 bytes */
template <class Int>
static inline size_t
get_fe_prefix( const uint8_t *buf,  const uint8_t *end,  Int &sz )
{
  if ( &buf[ 1 ] <= end ) {
    sz = *buf++;
    if ( sz < 0xfe )
      return 1;
    if ( sz == 0xfe ) { /* size == 2 bytes */
      if ( &buf[ 2 ] <= end ) {
        sz = get_u16<MD_BIG>( buf );
        return 3;
      }
    }
    else if ( &buf[ 4 ] <= end ) { /* size == 4 bytes */
      sz = get_u32<MD_BIG>( buf );
      return 5;
    }
  }
  return 0;
}
template <class Int>
static inline size_t
get_fe_prefix_len( Int sz )
{
  if ( sz < (Int) 0xfeU )    /* . */
    return 1;
  if ( sz <= (Int) 0xffffU ) /* fe.. */
    return 3;
  return 5;            /* ff.... */
}
template <class Int>
static inline size_t
set_fe_prefix( uint8_t *buf,  Int sz )
{
  if ( sz < (Int) 0xfeU ) {
    buf[ 0 ] = (uint8_t) sz;
    return 1;
  }
  if ( sz <= (Int) 0xffffU ) {
    buf[ 0 ] = 0xfeU;
    set_u16<MD_BIG>( &buf[ 1 ], (uint16_t) sz );
    return 3;
  }
  buf[ 0 ] = 0xffU;
  set_u32<MD_BIG>( &buf[ 1 ], (uint32_t) sz );
  return 5;
}
/* 0 -> 0x3f : 1 byte, 0x8000 | <short> : 2 byte, 0x400000 | <int24> : 3 byte
 * 0xC0000000 | <int32> : 4 bytes */
template <class Int>
static inline size_t
get_u30_prefix( const uint8_t *buf,  const uint8_t *end,  Int &sz )
{
  if ( &buf[ 1 ] <= end ) {
    sz = (Int) *buf++;
    if ( sz <= 0x3f )
      return 1;
    uint8_t i = ( sz & 0xc0 );
    if ( &buf[ 1 ] <= end ) {
      sz = ( ( sz & ~0xc0 ) << 8 ) | (Int) *buf++;
      if ( i == 0x80 )
        return 2;
      if ( &buf[ 1 ] <= end ) {
        sz = ( sz << 8 ) | (Int) *buf++;
        if ( i == 0x40 )
          return 3;
        if ( &buf[ 1 ] <= end ) {
          sz = ( sz << 8 ) | (Int) *buf;
          return 4;
        }
      }
    }
  }
  return 0;
}
static inline size_t
get_n32_prefix_len( const uint8_t *buf,  const uint8_t *end )
{
  if ( &buf[ 1 ] <= end ) {
    if ( buf[ 0 ] == 0x20 )
      return 1;
    if ( ( buf[ 0 ] & 0xC0 ) == 0 )
      return 2;
    if ( ( buf[ 0 ] & 0xC0 ) == 0x40 )
      return 3;
    if ( ( buf[ 0 ] & 0xC0 ) == 0x80 )
      return 4;
    return 5;
  }
  return 0;
}
static inline size_t
get_n64_prefix_len( const uint8_t *buf,  const uint8_t *end )
{
  size_t n = get_n32_prefix_len( buf, end );
  if ( n > 1 ) n = n * 2 - 1;
  return n;
}
template <class Int>
static inline size_t
get_u15_prefix_len( Int sz )
{
  if ( sz < (Int) 0x80U )    /* . */
    return 1;
  return 2;            /* ff.... */
}
/* 0 -> 0x7f : 1, 0x8000 | short : 2 */
template <class Int>
static inline size_t
get_u15_prefix( const uint8_t *buf,  const uint8_t *end,  Int &sz )
{
  if ( &buf[ 1 ] <= end ) {
    sz = (Int) *buf++;
    if ( sz <= 0x7f )
      return 1;
    if ( &buf[ 1 ] <= end ) {
      sz = ( ( sz & ~0x80 ) << 8 ) | (Int) *buf;
      return 2;
    }
  }
  return 0;
}
template <class Int>
static inline size_t
set_u15_prefix( uint8_t *buf,  Int sz )
{
  if ( sz <= 0x7f ) {
    buf[ 0 ] = (uint8_t) sz;
    return 1;
  }
  buf[ 0 ] = 0x80 | (uint8_t) ( sz >> 8 );
  buf[ 1 ] = (uint8_t) ( sz & 0xff );
  return 2;
}
/* 0 : 1 byte, 1 : 2 bytes, 2 ... 8 : n+1 bytes */
template <class Int>
static inline size_t
get_u64_prefix( const uint8_t *buf,  const uint8_t *end,  Int &sz )
{
  if ( &buf[ 1 ] <= end ) {
    size_t i = *buf++;
    if ( &buf[ i ] <= end ) {
      if ( i == 0 )
        sz = (Int) i;
      if ( i == 1 )
        sz = (Int) *buf;
      else if ( i <= 3 ) {
        sz = (Int) get_u16<MD_BIG>( buf );
        if ( i == 3 )
          sz = ( sz << 8 ) | (Int) buf[ 2 ];
      }
      else if ( i <= 7 ) {
        sz = (Int) get_u32<MD_BIG>( buf );
        end = &buf[ i ];
        for ( buf += 4; buf < end; )
          sz = ( sz << 8 ) | (Int) *buf++;
      }
      else
        sz = (Int) get_u64<MD_BIG>( buf );
      return i + 1;
    }
  }
  return 0;
}

}
}

#endif
#endif
