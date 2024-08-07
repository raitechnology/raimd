#include <stdio.h>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <raimd/dict_load.h>
#include <raimd/json_msg.h>
#include <raimd/tib_msg.h>
#include <raimd/rv_msg.h>
#include <raimd/tib_sass_msg.h>
#include <raimd/rwf_msg.h>

using namespace rai;
using namespace md;

template< class Writer >
size_t
test_write( Writer &writer )
{
  static char msg_type[]   = "MSG_TYPE",
              rec_type[]   = "REC_TYPE",
              seq_no[]     = "SEQ_NO",
              rec_status[] = "REC_STATUS",
              symbol[]     = "SYMBOL",
              bid_size[]   = "BIDSIZE",
              ask_size[]   = "ASKSIZE",
              bid[]        = "BID",
              ask[]        = "ASK",
              timact[]     = "TIMACT",
              trade_date[] = "TRADE_DATE",
              trdtim_ms[]  = "TRDTIM_MS",
              sent_words[] = "SENT_WORDS",
              row64_1[]    = "ROW64_1";
  MDDecimal dec;
  MDTime time;
  MDDate date;

  writer.append_int( msg_type, sizeof( msg_type ),  (short) 1 );
  writer.append_int( rec_type, sizeof( rec_type ),  (short) 0 );
  writer.append_int( seq_no, sizeof( seq_no ),  (short) 100 );
  writer.append_int( rec_status, sizeof( rec_status ), (short) 0 );
  writer.append_string( symbol, sizeof( symbol ), "INTC.O", 7 );
  writer.append_real( bid_size, sizeof( bid_size ), 10.0 );
  writer.append_real( ask_size, sizeof( ask_size ), 20.0 );

  dec.ival = 17500;
  dec.hint = MD_DEC_LOGn10_3; /* / 1000 */
  writer.append_decimal( bid, sizeof( bid ), dec );

  dec.ival = -17750;
  dec.hint = MD_DEC_LOGn10_3; /* / 1000 */
  writer.append_decimal( ask, sizeof( ask ), dec );

  time.hour       = 13;
  time.minute     = 15;
  time.sec        = 0;
  time.resolution = MD_RES_MINUTES;
  time.fraction   = 0;
  writer.append_time( timact, sizeof( timact ), time );

  date.year = 2019;
  date.mon  = 4;
  date.day  = 9;
  writer.append_date( trade_date, sizeof( trade_date ), date );

  writer.append_int( trdtim_ms, sizeof( trdtim_ms ), 1687808832990ULL );
  writer.append_int( sent_words, sizeof( sent_words ), (int64_t) -123456789012LL );

  static char head[] = "*WORLD HEADLINE";
  MDReference mref;
  mref.zero();
  mref.fptr     = (uint8_t *) head;
  mref.ftype    = MD_PARTIAL;
  mref.fsize    = sizeof( head ) - 1;
  mref.fentrysz = 8;
  writer.append_ref( row64_1, sizeof( row64_1 ), mref );

  return writer.update_hdr();
}

static void
test_json( void )
{
  MDMsgMem mem;
  MDMsg  * m;
  char buf[ 1024 ];
  ::strcpy( buf, "{\n"
                 "  MSG_TYPE : UPDATE,\n"
                 "  REC_STATUS : OK,\n"
                 "  TEXT : \"one two three\",\n"
                 "  NAME : \"d\\'angelo\",\n"
                 "  INF  : inf,\n"
                 "  INF  : -inf,\n"
                 "  NAN  : nan,\n"
                 "  NAN  : -nan\n"
                 "}" );
  size_t len = ::strlen( buf );
  m = MDMsg::unpack( buf, 0, len, 0, NULL, mem );
  if ( m != NULL ) {
    MDOutput mout;
    printf( "json mem %" PRIu64 "\n", mem.mem_off * sizeof( void * ) );
    m->print( &mout );
  }
}

static void
test_variable( void )
{
  const char *str =
"session     { CLASS_ID    1; DATA_SIZE 128; IS_FIXED false; DATA_TYPE string; }\n"
"digest      { CLASS_ID    2; DATA_SIZE 16; DATA_TYPE opaque; }\n"
"auth_digest { CLASS_ID    3; DATA_SIZE 16; DATA_TYPE opaque; }\n"
"cnonce      { CLASS_ID    4; DATA_SIZE 16; DATA_TYPE opaque; }\n"
"seqno       { CLASS_ID    5; DATA_TYPE u_int; }\n"
"seqno_8     { CLASS_ID  261; DATA_TYPE u_long; }\n"
"ack         { CLASS_ID   31; DATA_TYPE boolean; }\n";

  MDDictBuild dict_build;
  MDDict * str_dict = NULL;
  MDMsgMem mem;

  CFile::parse_string( dict_build, str, ::strlen( str ) );
  dict_build.index_dict( "cfile", str_dict );

  static char session[] = "session";
  static char digest[]  = "digest";
  static char seqno[]   = "seqno";
  static char seqno_8[] = "seqno_8";
  static char ack[]     = "ack";
  static char session_data[] = "sam.abcdefgh";
  static char digest_data[]  = "0123456789abcdef";

  TibSassMsgWriter w( mem, str_dict, mem.make( 128 ), 128 );

  w.append_string( session, sizeof( session ), session_data,
                   ::strlen( session_data ) );
  w.append_string( digest, sizeof( digest ), digest_data,
                   ::strlen( digest_data ) );
  w.append_uint<uint8_t>( ack, sizeof( ack ), true );
  w.append_uint<uint32_t>( seqno, sizeof( seqno ), 100 );
  w.append_uint<uint64_t>( seqno_8, sizeof( seqno_8 ), 10010001 );
  w.append_uint<uint8_t>( ack, sizeof( ack ), false );
  size_t sz = w.update_hdr();

  MDMsg  * m;
  m = MDMsg::unpack( w.buf, 0, sz, 0, str_dict, mem );
  if ( m != NULL ) {
    MDOutput mout;
    printf( "var msg test\n" );
    mout.print_hex( w.buf, sz );
    printf( "var msg sz %" PRIu64 "\n", sz );
    m->print( &mout );
  }
}

int
main( int argc, char **argv )
{
  if ( argc > 1 && ::strcmp( argv[ 1 ], "-h" ) == 0 ) {
    fprintf( stderr,
      "Test constructing and unpacking md msgs\n"
      "Always do rv, tib formats\n"
      "Only do tib_sass, rwf when cfile & RDM dicts are in $cfile_path\n" );
    return 1;
  }

  MDOutput mout;
  MDMsgMem mem;
  MDDict * dict;
  MDMsg  * m;
  size_t sz;

  dict = load_dict_files( ::getenv( "cfile_path" ) );
  /*md_init_auto_unpack();*/

  TibMsgWriter tibmsg( mem, mem.make( 128 ), 128 );
  sz = test_write<TibMsgWriter>( tibmsg );
  printf( "TibMsg test:\n" );
  mout.print_hex( tibmsg.buf, sz );
  printf( "tibmsg sz %" PRIu64 "\n", sz );

  m = MDMsg::unpack( tibmsg.buf, 0, sz, 0, dict, mem );
  if ( m != NULL )
    m->print( &mout );
#if 0
  MDFieldReader rd( *m );
  MDName nm;
  if ( rd.first( nm ) ) {
    do {
      MDValue val;
      printf( "%.*s : ", (int) nm.fnamelen, nm.fname );
      switch ( rd.type() ) {
        case MD_INT:
          if ( rd.get_int( val.i32 ) )
            printf( "%d", val.i32 );
          break;
        case MD_UINT:
          if ( rd.get_int( val.u32 ) )
            printf( "%u", val.u32 );
          break;
        default:
          if ( rd.get_string( val.str, sz ) )
            printf( "%.*s", (int) sz, val.str );
          break;
      }
      printf( "\n" );
    } while ( rd.next( nm ) );
  }
#endif
  mem.reuse();

  RvMsgWriter rvmsg( mem, mem.make( 128 ), 128 );
  sz = test_write<RvMsgWriter>( rvmsg );
  printf( "RvMsg test:\n" );
  mout.print_hex( rvmsg.buf, sz );
  printf( "rvmsg sz %" PRIu64 "\n", sz );

  m = MDMsg::unpack( rvmsg.buf, 0, sz, 0, dict, mem );
  if ( m != NULL )
    m->print( &mout );
  mem.reuse();

  RvMsgWriter rvmsg2( mem, mem.make( 128 ), 128 );
  RvMsgWriter submsg( mem, NULL, 0 );
  rvmsg2.append_string( "mtype", 6, "D", 2 );
  rvmsg2.append_subject( "sub", 4, "TEST.REC.XYZ.NaE" );
  rvmsg2.append_msg( "data", 5, submsg );

  uint32_t local_ip   = 0x7f000001;
  uint16_t local_port = 7500;
  submsg.append_type( "ipaddr", 7, local_ip, MD_IPDATA );
  submsg.append_type( "ipport", 7, local_port, MD_IPDATA );
  submsg.append_int<int32_t>( "vmaj", 5, 5 );
  submsg.append_int<int32_t>( "vmin", 5, 4 );
  submsg.append_int<int32_t>( "vupd", 5, 2 );
  sz = rvmsg2.update_hdr( submsg );
  printf( "RvMsg test2:\n" );
  mout.print_hex( rvmsg2.buf, sz );
  printf( "rvmsg2 sz %" PRIu64 "\n", sz );

  m = MDMsg::unpack( rvmsg2.buf, 0, sz, 0, dict, mem );
  if ( m != NULL )
    m->print( &mout );
  mem.reuse();

  TibMsgWriter tibmsg2( mem, mem.make( 128 ), 128 );
  TibMsgWriter tsubmsg( mem, NULL, 0 );
  tibmsg2.append_string( "mtype", 6, "D", 2 );
  tibmsg2.append_string( "sub", 4, "TEST.REC.XYZ.NaE", 
                         ::strlen( "TEST.REC.XYZ.NaE" ) + 1 );
  tibmsg2.append_msg( "data", 5, tsubmsg );

  tsubmsg.append_type( "ipaddr", 7, local_ip, MD_IPDATA );
  tsubmsg.append_type( "ipport", 7, local_port, MD_IPDATA );
  tsubmsg.append_int<int32_t>( "vmaj", 5, 5 );
  tsubmsg.append_int<int32_t>( "vmin", 5, 4 );
  tsubmsg.append_int<int32_t>( "vupd", 5, 2 );
  sz = tibmsg2.update_hdr( tsubmsg );
  printf( "TibMsg test2:\n" );
  mout.print_hex( tibmsg2.buf, sz );
  printf( "rvmsg2 sz %" PRIu64 "\n", sz );

  m = MDMsg::unpack( tibmsg2.buf, 0, sz, 0, dict, mem );
  if ( m != NULL )
    m->print( &mout );
  mem.reuse();

  if ( dict != NULL ) {
    TibSassMsgWriter tibsassmsg( mem, dict, mem.make( 128 ), 128 );
    sz = test_write<TibSassMsgWriter>( tibsassmsg );
    printf( "TibSassMsg test:\n" );
    mout.print_hex( tibsassmsg.buf, sz );
    printf( "tibsassmsg sz %" PRIu64 "\n", sz );

    m = MDMsg::unpack( tibsassmsg.buf, 0, sz, 0, dict, mem );
    if ( m != NULL )
      m->print( &mout );
    mem.reuse();

    RwfFieldListWriter rwfmsg( mem, dict, mem.make( 128 ), 128 );
    sz = test_write<RwfFieldListWriter>( rwfmsg );
    printf( "RwfMsg test:\n" );
    mout.print_hex( rwfmsg.buf, sz );
    printf( "rwfmsg sz %" PRIu64 "\n", sz );

    m = MDMsg::unpack( rwfmsg.buf, 0, sz, RWF_FIELD_LIST_TYPE_ID, dict, mem );
    if ( m != NULL )
      m->print( &mout );
    mem.reuse();
  }
  test_json();
  test_variable();

  return 0;
}

