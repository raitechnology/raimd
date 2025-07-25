#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include <raimd/md_replay.h>

using namespace rai;
using namespace md;

bool
MDReplay::open( const char *fname ) noexcept
{
  this->close();
  if ( fname == NULL || ::strcmp( fname, "-" ) == 0 )
    this->input = NULL;
  else {
    this->input = ::fopen( fname, "rb" );
    if ( this->input == NULL ) {
      perror( "fopen" );
      return false;
    }
  }
  return true;
}

void
MDReplay::close( void ) noexcept
{
  if ( this->input != NULL ) {
    ::fclose( (FILE *) this->input );
    this->input = NULL;
  }
}

MDReplay::~MDReplay() noexcept
{
  if ( this->input != NULL )
    this->close();
  md_msg_mem_release( &this->mem );
}

bool
MDReplay::first( void ) noexcept
{
  this->bufoff  = 0;
  this->buflen  = 0;
  this->subjlen = 0;
  this->msglen  = 0;
  this->msgbuf  = this->buf;
  this->resize( 2 * 1024 );
  return this->next();
}

void
MDReplay::resize( size_t sz ) noexcept
{
  size_t oldsz = this->bufsz;
  if ( oldsz == 0 || oldsz > sz ) {
    ((MDMsgMem &) this->mem).reuse();
    this->buf = (char *) ((MDMsgMem &) this->mem).make( sz );
  }
  else {
    ((MDMsgMem &) this->mem).extend( oldsz, sz, &this->buf );
  }
  this->bufsz  = sz;
  this->subj   = this->buf;
  this->msgbuf = this->buf;
}

bool
MDReplay::next( void ) noexcept
{
  if ( this->input == NULL )
    return false;

  this->bufoff = &this->msgbuf[ this->msglen ] - this->buf;
  return this->parse();
}

bool
MDReplay::fillbuf( size_t need_bytes ) noexcept
{
  size_t left = this->bufsz - this->buflen;
  if ( left < need_bytes ) {
    char * start = &this->buf[ this->bufoff ];
    if ( start > this->buf ) {
      memmove( this->buf, start, this->buflen - this->bufoff );
      this->buflen -= this->bufoff;
      this->bufoff = 0;
      left = this->bufsz - this->buflen;
    }
    if ( left < need_bytes ) {
      size_t newsz = this->bufsz + need_bytes - left;
      this->resize( newsz );
      left = this->bufsz - this->buflen;
    }
  }
  for (;;) {
    FILE  * fp = (FILE *) this->input;
    ssize_t n;
    n = ::fread( &this->buf[ this->buflen ], 1, left, fp ? fp : stdin );
    if ( n <= 0 ) {
      if ( n < 0 )
        perror( "fread" );
      return false;
    }
    this->buflen += n;
    if ( (size_t) n >= need_bytes )
      break;
    need_bytes -= n;
    left -= n;
  }
  return true;
}

bool
MDReplay::parse( void ) noexcept
{
  size_t sz,
         len[ 2 ];
  char * ar[ 2 ],
       * s   = &this->buf[ this->bufoff ],
       * eol,
       * end = &this->buf[ this->buflen ];
  int    i   = 0;

  sz = end - s;
  for ( ; i < 2; i++ ) {
    while ( (eol = (char *) ::memchr( s, '\n', sz )) == NULL ) {
      if ( ! this->fillbuf( 1 ) )
        return false;
      s   = &this->buf[ this->bufoff ];
      end = &this->buf[ this->buflen ];
      sz  = end - s;
      i   = 0;
    }
    ar[ i ] = s;
    len[ i ] = eol - s;
    if ( len[ i ] > 0 && ar[ i ][ len[ i ] - 1 ] == '\r' )
      len[ i ]--;
    s  = eol + 1;
    sz = end - s;
  }

  long j = strtol( ar[ 1 ], &end, 0 );
  if ( end == ar[ 1 ] || j < 0 || j > 0x7fffffff )
    return false;

  if ( (size_t) j > sz ) {
    size_t need = (size_t) j - sz,
           off[ 2 ];
    off[ 0 ] = ar[ 0 ] - &this->buf[ this->bufoff ];
    off[ 1 ] = s - &this->buf[ this->bufoff ];

    if ( ! this->fillbuf( need ) )
      return false;

    ar[ 0 ] = &this->buf[ this->bufoff + off[ 0 ] ];
    s       = &this->buf[ this->bufoff + off[ 1 ] ];
  }
  this->subj    = ar[ 0 ];
  this->subjlen = len[ 0 ];
  this->msgbuf  = s;
  this->msglen  = j;
  return true;
}

extern "C" {
bool md_replay_create( MDReplay_t **rr,  void *inp ) {
  void *p = ::malloc( sizeof( MDReplay ) );
  if ( ! p ) { *rr = NULL; return false; }
  MDReplay *r = new ( p ) MDReplay();
  /*r->init( inp );*/
  r->input = inp;
  *rr = r;
  return true;
}
void md_replay_destroy( MDReplay_t *r ) {
  if ( r->input != NULL )
    ((MDReplay *) r)->close();
  delete (MDReplay *) r;
}
void md_replay_init( MDReplay_t *r,  void *inp ) { ((MDReplay *) r)->init( inp ); }
bool md_replay_open( MDReplay_t *r,  const char *fname ) { return ((MDReplay *) r)->open( fname ); }
void md_replay_close( MDReplay_t *r ) { ((MDReplay *) r)->close(); }
void md_replay_resize( MDReplay_t *r,  size_t sz ) { ((MDReplay *) r)->resize( sz ); }
bool md_replay_fillbuf( MDReplay_t *r,  size_t need_bytes ) { return ((MDReplay *) r)->fillbuf( need_bytes ); }
bool md_replay_first( MDReplay_t *r ) { return ((MDReplay *) r)->first(); }
bool md_replay_next( MDReplay_t *r ) { return ((MDReplay *) r)->next(); }
bool md_replay_parse( MDReplay_t *r ) { return ((MDReplay *) r)->parse(); }
}
