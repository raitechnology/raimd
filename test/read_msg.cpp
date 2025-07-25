#include <stdio.h>
#include <string.h>
#include <stdint.h>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <raimd/dict_load.h>
#include <raimd/md_replay.h>
#include <raimd/json_msg.h>
#include <raimd/tib_msg.h>
#include <raimd/rv_msg.h>
#include <raimd/tib_sass_msg.h>
#include <raimd/sass.h>
#include <raimd/mf_msg.h>
#include <raimd/rwf_msg.h>
#include <raimd/md_hash_tab.h>

using namespace rai;
using namespace md;

struct FieldIndex {
  static const uint32_t mask = 31;
  MDName * fld;
  size_t   idx_char;
  int      fld_cnt,
           fld_idx[ mask + 1 ],
           fld_idx2[ mask + 1 ];
  FieldIndex() : fld( 0 ), idx_char( 0 ), fld_cnt( 0 ) {}

  bool match( MDName &nm ) {
    if ( nm.fnamelen <= this->idx_char )
      return false;
    int j = this->fld_idx[ nm.fname[ this->idx_char ] & mask ],
        k = this->fld_idx2[ nm.fname[ this->idx_char / 2 ] & mask ];
    if ( j == 0 || k == 0 )
      return false;
    if ( k > j )
      j = k;
    for ( ; j <= this->fld_cnt; j++ ) {
      if ( nm.equals( this->fld[ j - 1 ] ) )
        return true;
    }
    return false;
  }
  void index( void ) {
    int i, j;
    this->idx_char = 0;
    ::memset( this->fld_idx, 0, sizeof( this->fld_idx ) );
    ::memset( this->fld_idx2, 0, sizeof( this->fld_idx2 ) );
    for ( i = 0; i < this->fld_cnt; i++ ) {
      if ( this->idx_char == 0 || this->fld[ i ].fnamelen < this->idx_char )
        this->idx_char = this->fld[ i ].fnamelen;
    }
    this->idx_char = ( this->idx_char >= 3 ? this->idx_char - 2 : 1 );
    for ( i = 0; i < this->fld_cnt; i++ ) {
      j = this->fld[ i ].fname[ this->idx_char ] & mask;
      if ( this->fld_idx[ j ] == 0 ) this->fld_idx[ j ] = i + 1;
      j = this->fld[ i ].fname[ this->idx_char / 2 ] & mask;
      if ( this->fld_idx2[ j ] == 0 ) this->fld_idx2[ j ] = i + 1;
    }
  }
};

template< class Writer >
int
filter( Writer &w,  MDMsg *m,  FieldIndex &rm,  FieldIndex &kp,
        size_t &msg_sz,  uint32_t &fldcnt )
{
  MDFieldIter *iter;
  MDName nm;
  int status;
  fldcnt = 0;
  msg_sz = 0;
  if ( (status = m->get_field_iter( iter )) == 0 ) {
    if ( (status = iter->first()) == 0 ) {
      do {
        status = iter->get_name( nm );
        if ( status == 0 ) {
          if ( ( rm.fld_cnt == 0 || ! rm.match( nm ) ) &&
               ( kp.fld_cnt == 0 || kp.match( nm ) ) ) {
            w.append_iter( iter );
            status = w.err;
            fldcnt++;
          }
        }
        if ( status != 0 )
          break;
      } while ( (status = iter->next()) == 0 );
    }
  }
  if ( status != Err::NOT_FOUND )
    return status;
  msg_sz = w.update_hdr();
  return 0;
}

static void *
memdiff( void *msg, void *buf, size_t sz ) noexcept
{
  for ( size_t i = 0; i < sz; i++  ) {
    if ( ((uint8_t *) msg)[ i ] != ((uint8_t *) buf)[ i ] )
      return &((uint8_t *) msg)[ i ];
  }
  return NULL;
}

void
cmp_msg( MDOutput &mout,  MDMsg *m,  MDDict *dict,  void *msg,
         size_t msg_sz ) noexcept
{
  MDMsgMem mem;
  void * p;
  int diff = 0, status, status2;
  uint8_t * buf = &((uint8_t *) m->msg_buf)[ m->msg_off ];
  size_t sz = m->msg_end - m->msg_off;

  if ( msg_sz != sz ) {
    mout.printf( "msg_sz %" PRIu64 " != %" PRIu64 "\n", msg_sz, sz );
    diff++;
  }
  size_t len = ( msg_sz < sz ? msg_sz : sz );
  for ( size_t off = 0; off < len; ) {
    p = memdiff( &((char *) msg)[ off ], &buf[ off ], len - off );
    if ( p == NULL )
      break;
    off = (char *) p - (char *) msg;
    mout.printf( "memcmp diff 0x%" PRIx64 "\n", off );
    off++;
    diff++;
  }
  if ( diff != 0 ) {
    mout.printf( "buf:\n" );
    mout.print_hex( buf, sz );
    mout.printf( "msg:\n" );
    mout.print_hex( msg, msg_sz );
  }
  MDMsg * m2 = MDMsg::unpack( msg, 0, msg_sz, 0, dict, mem );
  MDFieldIter *iter, *iter2;
  if ( m2 != NULL ) {
    if ( (status = m->get_field_iter( iter )) == 0 &&
         (status2 = m2->get_field_iter( iter2 )) == 0 ) {
      if ( (status = iter->first()) == 0 &&
           (status2 = iter2->first()) == 0 ) {
        do {
          MDName      n, n2;
          MDReference mref, href,
                      mref2, href2;
          bool        equal = false;
          if ( iter->get_name( n ) == 0 &&
               iter->get_reference( mref ) == 0 ) {
            iter->get_hint_reference( href );

            if ( iter->get_name( n2 ) == 0 &&
                 iter->get_reference( mref2 ) == 0 ) {
              iter->get_hint_reference( href2 );

              if ( n.equals( n2 ) && mref.equals( mref2 ) &&
                   href.equals( href2 ) ) {
                equal = true;
              }
            }
          }
          if ( ! equal ) {
            iter->print( &mout );
            iter2->print( &mout );
          }
        } while ( (status = iter->next()) == 0 &&
                  (status2 = iter2->next()) == 0 );
      }
    }
  }
}

struct WildIndex {
  char  ** sub;
  int      sub_cnt;
  WildIndex() : sub( 0 ), sub_cnt( 0 ) {}

  bool match( const char *subject ) {
    for ( int i = 0; i < this->sub_cnt; i++ ) {
      const char * w = this->sub[ i ],
                 * t = subject;
      for (;;) {
        if ( ( *w == '\0' && *t == '\0' ) || *w == '>' )
          return true;
        if ( *w == '\0' || *t == '\0' )
          break;
        if ( *w == '*' ) {
          w++;
          while ( *t != '\0' && *t != '.' )
            t++;
        }
        else if ( *w++ != *t++ )
          break;
      }
    }
    return false;
  }
};

struct MetaData : public MDSubjectKey {
  uint16_t rec_type,
           flist;
  uint32_t seqno;
  MDFormClass * form;

  void * operator new( size_t, void *ptr ) { return ptr; }
  MetaData( const char *s,  size_t len,  uint16_t t,  uint16_t f,
            MDFormClass *fm,  uint32_t n )
  : MDSubjectKey( s, len ), rec_type( t ), flist( f ), seqno( n ), form( fm ) {}

  static MetaData * make( const char *subj,  size_t len,  uint16_t rec_type,
                          uint16_t flist,  MDFormClass *form, uint32_t seqno ) {
    void * m = ::malloc( sizeof( MetaData ) + len + 1 );
    char * p = &((char *) m)[ sizeof( MetaData ) ];
    ::memcpy( p, subj, len );
    p[ len ] = '\0';
    return new ( m ) MetaData( p, len, rec_type, flist, form, seqno );
  }
};

typedef MDHashTabT<MDSubjectKey, MetaData> SubHT;

static const char *
get_arg( int argc, char *argv[], int b, const char *f,
         const char *def ) noexcept
{
  for ( int i = 1; i < argc - b; i++ )
    if ( ::strcmp( f, argv[ i ] ) == 0 ) /* -p port */
      return argv[ i + b ];
  return def; /* default value */
}

int
main( int argc, char **argv )
{
  MDOutput mout, bout; /* message output, uses printf to stdout */
  MDMsgMem mem;  /* memory for printing (converting numbers to strings) */
  MDDict * dict       = NULL, /* dictinonaries, cfile and RDM/appendix_a */
         * cfile_dict = NULL,
         * rdm_dict   = NULL,
         * flist_dict = NULL;
  const char * fn     = get_arg( argc, argv, 1, "-f", NULL ),
             * out    = get_arg( argc, argv, 1, "-o", NULL ),
             * path   = get_arg( argc, argv, 1, "-p", ::getenv( "cfile_path" ) ),
             * rmfld  = get_arg( argc, argv, 1, "-r", NULL ),
             * kpfld  = get_arg( argc, argv, 1, "-k", NULL ),
             * fmt    = get_arg( argc, argv, 1, "-t", NULL ),
             * sub    = get_arg( argc, argv, 1, "-s", NULL );
  bool         quiet  = get_arg( argc, argv, 0, "-q", NULL ) != NULL,
               binout = false;
  uint32_t cvt_type_id = 0;

  if ( get_arg( argc, argv, 0, "-h", NULL ) != NULL ) {
    fprintf( stderr,
      "Usage: %s [-f file] [-o out] [-p cfile_path] [-r flds] "
                "[-k flds] [-s subs ] [-q]\n"
      "Test reading messages from files on command line\n"
      "  -f file           = replay file to read\n"
      "  -o file           = output file to write\n"
      "  -p cfile_path     = load dictionary files from path\n"
      "  -r fields         = remove fields from message\n"
      "  -k fields         = keep fields in message\n"
      "  -t format         = convert message to (eg TIBMSG,RVMSG,RWFMSG)\n"
      "  -s wildcard       = filter subjects using wildcard\n"
      "  -q                = quiet, no printing of messages\n"
      "A file is a binary dump of messages, each with these lines:\n"
      "<subject>\n"
      "<num-bytes>\n"
      "<blob-of-data\n"
      "\n"
      "Example:\n"
      "test.subject\n"
      "15\n"
      "{ \"ten\" : 10 }\n", argv[ 0 ] );
    return 1;
  }
  if ( path != NULL )
    dict = load_dict_files( path );
  if ( dict != NULL ) {
    for ( MDDict *d = dict; d != NULL; d = d->get_next() ) {
      if ( d->dict_type[ 0 ] == 'c' )
        cfile_dict = d;
      else if ( d->dict_type[ 0 ] == 'a' )
        rdm_dict = d;
      else if ( d->dict_type[ 0 ] == 'f' )
        flist_dict = d;
    }
  }
  if ( out != NULL ) {
    if ( ::strcmp( out, "-" ) == 0 )
      quiet = true;
    else if ( bout.open( out, "wb" ) != 0 ) {
      perror( out );
      return 1;
    }
    binout = true;
  }
  md_init_auto_unpack();
  if ( fmt != NULL ) {
    uint32_t  i;
    MDMatch * m;
    for ( m = MDMsg::first_match( i ); m; m = MDMsg::next_match( i ) ) {
      if ( ::strcasecmp( m->name, fmt ) == 0 ) {
        cvt_type_id = m->hint[ 0 ];
        break;
      }
    }
    switch ( cvt_type_id ) {
      case JSON_TYPE_ID:
      case MARKETFEED_TYPE_ID:
      case RVMSG_TYPE_ID:
      case RWF_FIELD_LIST_TYPE_ID:
      case RWF_MSG_TYPE_ID:
      case TIBMSG_TYPE_ID:
      case TIB_SASS_TYPE_ID:
        break;
      default:
        fprintf( stderr, "format \"%s\" %s, types:\n", fmt,
                 cvt_type_id == 0 ? "not found" : "converter not implemented" );
        for ( m = MDMsg::first_match( i ); m; m = MDMsg::next_match( i ) ) {
          if ( m->hint[ 0 ] != 0 )
            fprintf( stderr, "  %s : %x\n", m->name, m->hint[ 0 ] );
        }
        return 1;
    }
  }
  if ( ! quiet ) 
    printf( "reading from %s\n", ( fn == NULL ? "stdin" : fn ) );

  FieldIndex rm, kp;
  WildIndex  wild;
  rm.fld = (MDName *) ::malloc( sizeof( MDName ) * argc * 2 );
  kp.fld = &rm.fld[ argc ];
  wild.sub = (char **) ::malloc( sizeof( char * ) * argc );
  int x = 0, y = 0, xx = 0, yy = 0, xxx = 0, yyy = 0;
  if ( rmfld != NULL || kpfld != NULL || sub != NULL ) {
    int i;
    for ( i = 1; i < argc; i++ ) {
      rm.fld[ i ].zero();
      kp.fld[ i ].zero();
      if ( argv[ i ] == rmfld ) {
        x = i;
        y = argc;
      }
      else if ( argv[ i ] == kpfld ) {
        xx = i;
        yy = argc;
      }
      else if ( argv[ i ] == sub ) {
        xxx = i;
        yyy = argc;
      }
      else if ( y == argc && argv[ i ][ 0 ] == '-' )
        y = i;
      else if ( yy == argc && argv[ i ][ 0 ] == '-' )
        yy = i;
      else if ( yyy == argc && argv[ i ][ 0 ] == '-' )
        yyy = i;
      if ( y == argc ) {
        rm.fld[ i ].fname = argv[ i ];
        rm.fld[ i ].fnamelen = ::strlen( argv[ i ] ) + 1;
      }
      else if ( yy == argc ) {
        kp.fld[ i ].fname = argv[ i ];
        kp.fld[ i ].fnamelen = ::strlen( argv[ i ] ) + 1;
      }
      else if ( yyy == argc ) {
        wild.sub[ i ] = argv[ i ];
      }
    }
    rm.fld       = &rm.fld[ x ];
    rm.fld_cnt   = y - x;
    kp.fld       = &kp.fld[ xx ];
    kp.fld_cnt   = yy - xx;
    wild.sub     = &wild.sub[ xxx ];
    wild.sub_cnt = yyy - xxx;
    rm.index();
    kp.index();
  }
  /* unpack auto recognizes messages by magic numbers, etc */
  /*md_init_auto_unpack();*/

  /* read subject, size, message data */
  FILE *filep = ( fn == NULL ? stdin : fopen( fn, "rb" ) );
  if ( filep == NULL ) {
    perror( fn );
    return 1;
  }
  MDReplay replay( filep );
  SubHT    sub_ht( 128 );
  uint64_t msg_count   = 0,
           err_cnt     = 0,
           discard_cnt = 0;

  for ( bool b = replay.first(); b; b = replay.next() ) {
    /* strip newline on subject */
    char * subj = replay.subj;
    size_t slen = replay.subjlen;
    subj[ slen ] = '\0';
    if ( wild.sub_cnt != 0 && ! wild.match( subj ) )
      continue;

    /* try to unpack it */
    mem.reuse(); /* reset mem for next msg */
    MDMsg  * m      = MDMsg::unpack( replay.msgbuf, 0, replay.msglen, 0,
                                     dict, mem );
    void   * msg    = replay.msgbuf;
    size_t   msg_sz = replay.msglen;
    uint32_t fldcnt = 0;
    int      status = -1;

    if ( m == NULL ) {
      err_cnt++;
      continue;
    }
    if ( rm.fld_cnt != 0 || kp.fld_cnt != 0 ) {
      size_t buf_sz  = msg_sz * 2;
      void * buf_ptr = mem.make( buf_sz );
      if ( m->get_type_id() == TIBMSG_TYPE_ID ) {
        TibMsgWriter w( mem, buf_ptr, buf_sz );
        status = filter<TibMsgWriter>( w, m, rm, kp, msg_sz, fldcnt );
        m = NULL;
        if ( status == 0 && fldcnt > 0 ) {
          msg = w.buf;
          m = TibMsg::unpack( msg, 0, msg_sz, 0, cfile_dict, mem );
        }
      }
      else if ( m->get_type_id() == RVMSG_TYPE_ID ) {
        RvMsgWriter w( mem, buf_ptr, buf_sz );
        status = filter<RvMsgWriter>( w, m, rm, kp, msg_sz, fldcnt );
        m = NULL;
        if ( status == 0 && fldcnt > 0 ) {
          msg = w.buf;
          m = RvMsg::unpack( msg, 0, msg_sz, 0, cfile_dict, mem );
        }
      }
      else if ( m->get_type_id() == TIB_SASS_TYPE_ID ) {
        TibSassMsgWriter w( mem, cfile_dict, buf_ptr, buf_sz );
        status = filter<TibSassMsgWriter>( w, m, rm, kp, msg_sz, fldcnt );
        m = NULL;
        if ( status == 0 && fldcnt > 0 ) {
          msg = w.buf;
          m = TibSassMsg::unpack( msg, 0, msg_sz, 0, cfile_dict, mem );
        }
      }
      if ( status != 0 ) {
        err_cnt++;
        continue;
      }
    }
    if ( cvt_type_id != 0 && m != NULL ) {
      MDFormClass * form  = NULL;
      size_t   buf_sz     = ( msg_sz > 1024 ? msg_sz : 1024 );
      void   * buf_ptr    = mem.make( buf_sz );
      uint16_t flist      = 0,
               msg_type   = MD_UPDATE_TYPE,
               rec_type   = 0;
      uint32_t seqno      = 0;

      switch ( m->get_type_id() ) {
        case MARKETFEED_TYPE_ID: {
          MktfdMsg & mf = *(MktfdMsg *) m;
          msg_type = mf_func_to_sass_msg_type( mf.func );
          flist = mf.flist;
          seqno = mf.rtl;
          break;
        }
        case RWF_MSG_TYPE_ID: {
          RwfMsg & rwf = *(RwfMsg *) m;
          msg_type = rwf_to_sass_msg_type( rwf );
          RwfMsg * fl = rwf.get_container_msg();
          if ( fl != NULL )
            flist = fl->fields.flist;
          if ( rwf.msg.test( X_HAS_SEQ_NUM ) )
            seqno = rwf.msg.seq_num;
          break;
        }
        case RWF_FIELD_LIST_TYPE_ID: {
          RwfMsg & rwf = *(RwfMsg *) m;
          flist = rwf.fields.flist;
          break;
        }
        default: {
          MDFieldReader rd( *m );
          if ( rd.find( MD_SASS_MSG_TYPE, MD_SASS_MSG_TYPE_LEN ) )
            rd.get_uint( msg_type );
          if ( rd.find( MD_SASS_SEQ_NO, MD_SASS_SEQ_NO_LEN ) )
            rd.get_uint( seqno );
          break;
        }
      }
      status = -1;
      if ( cvt_type_id == TIBMSG_TYPE_ID || cvt_type_id == TIB_SASS_TYPE_ID ) {
        if ( flist != 0 && flist_dict != NULL && cfile_dict != NULL ) {
          MDLookup by( flist );
          if ( flist_dict->lookup( by ) ) {
            MDLookup fc( by.fname, by.fname_len );
            if ( cfile_dict->get( fc ) && fc.ftype == MD_MESSAGE ) {
              rec_type = fc.fid;
              if ( fc.map_num != 0 )
                form = cfile_dict->get_form_class( fc );
              else
                form = NULL;
              MDSubjectKey k( subj, slen );
              MetaData * data;
              size_t pos;
              if ( (data = sub_ht.find( k, pos )) == NULL ) {
                sub_ht.insert( pos, MetaData::make( subj, slen, rec_type, flist,
                                                    form, ++seqno ) );
              }
              else {
                data->rec_type = rec_type;
                data->flist    = flist;
                data->form     = form;
                seqno          = ++data->seqno;
              }
            }
          }
        }
        else {
          MDSubjectKey k( subj, slen );
          MetaData * data = sub_ht.find( k );
          if ( data != NULL ) {
            rec_type = data->rec_type;
            flist    = data->flist;
            form     = data->form;
            seqno    = ++data->seqno;
          }
        }
      }
      if ( seqno == 0 ) {
        MDSubjectKey k( subj, slen );
        MetaData * data;
        size_t pos;
        if ( (data = sub_ht.find( k, pos )) == NULL ) {
          sub_ht.insert( pos, MetaData::make( subj, slen, rec_type, flist,
                                              form, ++seqno ) );
        }
        else {
          rec_type = data->rec_type;
          flist    = data->flist;
          form     = data->form;
          seqno    = ++data->seqno;
        }
      }
      switch ( cvt_type_id ) {
        case JSON_TYPE_ID: {
          JsonMsgWriter w( mem, buf_ptr, buf_sz );
          if ( (status = w.convert_msg( *m )) == 0 ) {
            msg    = w.buf;
            msg_sz = w.update_hdr();
            m = JsonMsg::unpack( msg, 0, msg_sz, 0, dict, mem );
          }
          break;
        }
        case MARKETFEED_TYPE_ID: {
          break;
        }
        case RVMSG_TYPE_ID: {
          RvMsgWriter w( mem, buf_ptr, buf_sz );
          append_sass_hdr<RvMsgWriter>( w, form, msg_type, rec_type, seqno, 0,
                                        subj, slen );
          if ( (status = w.convert_msg( *m, true )) == 0 ) {
            msg    = w.buf;
            msg_sz = w.update_hdr();
            m = RvMsg::unpack( msg, 0, msg_sz, 0, cfile_dict, mem );
          }
          break;
        }
        case RWF_MSG_TYPE_ID: {
          RwfMsgClass msg_class = ( msg_type == MD_INITIAL_TYPE ?
                                    REFRESH_MSG_CLASS : UPDATE_MSG_CLASS );
          uint32_t stream_id = MDDict::dict_hash( subj, slen );
          RwfMsgWriter w( mem, rdm_dict, buf_ptr, buf_sz,
                          msg_class, MARKET_PRICE_DOMAIN, stream_id );
          if ( msg_class == REFRESH_MSG_CLASS )
            w.set( X_CLEAR_CACHE, X_REFRESH_COMPLETE );
          w.add_seq_num( seqno )
           .add_msg_key()
           .name( subj, slen )
           .name_type( NAME_TYPE_RIC )
           .end_msg_key();
          RwfFieldListWriter & fl = w.add_field_list();
          if ( flist != 0 )
            fl.add_flist( flist );
          status = w.err;
          if ( status == 0 )
            status = fl.convert_msg( *m, true );
          if ( status == 0 )
            w.end_msg();
          if ( (status = w.err) == 0 ) {
            msg    = w.buf;
            msg_sz = w.off;
            m = RwfMsg::unpack_message( msg, 0, msg_sz, 0, rdm_dict, mem );
          }
          break;
        }
        case RWF_FIELD_LIST_TYPE_ID: {
          RwfFieldListWriter w( mem, rdm_dict, buf_ptr, buf_sz );
          if ( flist != 0 )
            w.add_flist( flist );
          if ( (status = w.convert_msg( *m, false )) == 0 ) {
            msg    = w.buf;
            msg_sz = w.update_hdr();
            m = RwfMsg::unpack_field_list( msg, 0, msg_sz, 0, rdm_dict, mem );
          }
          break;
        }
        case TIBMSG_TYPE_ID: {
          TibMsgWriter w( mem, buf_ptr, buf_sz );
          append_sass_hdr<TibMsgWriter>( w, form, msg_type, rec_type, seqno, 0,
                                         subj, slen );
          if ( (status = w.convert_msg( *m, true )) == 0 ) {
            msg    = w.buf;
            msg_sz = w.update_hdr();
            m = TibMsg::unpack( msg, 0, msg_sz, 0, cfile_dict, mem );
          }
          break;
        }
        case TIB_SASS_TYPE_ID: {
          if ( form != NULL ) {
            TibSassMsgWriter w( mem, *form, buf_ptr, buf_sz );
            append_sass_hdr<TibSassMsgWriter>( w, form, msg_type, rec_type,
                                               seqno, 0, subj, slen );
            if ( msg_type == MD_INITIAL_TYPE )
              w.append_form_record();
            if ( (status = w.convert_msg( *m, true )) == 0 ) {
              msg    = w.buf;
              msg_sz = w.update_hdr();
              m = TibSassMsg::unpack( msg, 0, msg_sz, 0, cfile_dict, mem );
            }
          }
          else {
            TibSassMsgWriter w( mem, cfile_dict, buf_ptr, buf_sz );
            append_sass_hdr<TibSassMsgWriter>( w, form, msg_type, rec_type,
                                               seqno, 0, subj, slen );
            if ( (status = w.convert_msg( *m, true )) == 0 ) {
              msg    = w.buf;
              msg_sz = w.update_hdr();
              m = TibSassMsg::unpack( msg, 0, msg_sz, 0, cfile_dict, mem );
            }
          }
          break;
        }
      }
      if ( status != 0 || m == NULL ) {
        if ( m != NULL ) {
          mout.printf( "%s: seqno %u error %d\n", subj, seqno, status );
          m->print( &mout );
        }
        err_cnt++;
        continue;
      }
    }
    if ( ! quiet && m != NULL ) {
      /* print it */
      mout.printf( "\n## %s (fmt %s)\n", subj, m->get_proto_string() );
      m->print( &mout );
    }
    /*cmp_msg( mout, m, dict, msg, msg_sz );*/
    if ( binout ) {
      if ( m != NULL ) {
        switch ( m->get_type_id() ) {
          case RWF_FIELD_LIST_TYPE_ID:
          case RWF_MSG_TYPE_ID:
            if ( get_u32<MD_BIG>( msg ) != RWF_FIELD_LIST_TYPE_ID ) {
              uint8_t buf[ 8 ];
              set_u32<MD_BIG>( buf, RWF_FIELD_LIST_TYPE_ID );
              set_u32<MD_BIG>( &buf[ 4 ], ((RwfMsg *) m)->base.type_id );
              bout.printf( "%s\n%" PRIu64 "\n", subj, msg_sz + 8 );
              bout.write( buf, 8 );
              break;
            }
            /* FALLTHRU */
          default:
            bout.printf( "%s\n%" PRIu64 "\n", subj, msg_sz );
            break;
        }
        if ( bout.write( msg, msg_sz ) != msg_sz )
          err_cnt++;
      }
      else {
        discard_cnt++;
      }
    }
    msg_count++;
  }
  /*if ( ! quiet )*/ {
    fprintf( stderr, "found %" PRIu64 " messages, %" PRIu64 " erorrs\n",
             msg_count, err_cnt );
    if ( discard_cnt > 0 )
      fprintf( stderr, "discarded %" PRIu64 "\n", discard_cnt );
  }
  if ( fn != NULL && filep != NULL )
    fclose( filep );
  if ( bout.close() != 0 ) {
    perror( out );
    err_cnt++;
  }
  return err_cnt == 0 ? 0 : 2;
}

