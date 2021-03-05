#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>

#include "iniparser.h"
#include "fru-defs.h"

#define TOOL_VERSION "0.2"

char usage[] =
    "\nUsage: %s [OPTIONS...]\n\n"
    "OPTIONS:\n"
    "\t-h\t\tThis help text\n"
    "\t-v\t\tPrint version and exit\n"
    "\t-r\t\tRead FRU data from file specified by -i\n"
    "\t-i FILE\t\tFRU data file (use with -r)\n"
    "\t-w\t\tWrite FRU data to file specified in -o\n"
    "\t-c FILE\t\tFRU Config file\n"
    "\t-s SIZE\t\tMaximum file size (in bytes) allowed for the FRU data file\n"
    "\t-a\t\tUse 8-bit ASCII\n"
    "\t-o FILE\t\tOutput FRU data filename (use with -w)\n\n";

/* Std IPMI FRU Section headers */
const char *IUA = "iua";
const char *CIA = "cia";
const char *BIA = "bia";
const char *PIA = "pia";
const char *MIA_MAR = "mia_mar";
const char *MIA_VER = "mia_ver";
const char *MIA_MAC = "mia_mac";
const char *MIA_FAN = "mia_fan";
const char *MIA_BCI = "mia_bci";
const char *MIA_SC  = "mia_sc";


/* IUA section must-have keys */
const char* BINFILE = "bin_file";

/* predefined keys */
const char* CHASSIS_TYPE    = "chassis_type";
const char* LANGUAGE_CODE   = "language_code";
const char* MFG_DATETIME    = "mfg_datetime";

const char* PART_NUMBER     = "part_number";
const char* SERIAL_NUMBER   = "serial_number";
const char* MANUFACTURER    = "manufacturer";
const char* VERSION         = "version";
const char* ASSET_TAG       = "asset_tag";

const char* SKU_ID          = "sku_id";
const char* FRU_ID          = "fru_id";
const char* FRU_FILE_ID     = "fru_file_id";

const char* PRODUCT_NAME    = "product_name";
const char* PRODUCT_FAMILY  = "product_family";

const char* PART_NUMBER_SIZE     = "part_number_size";
const char* SERIAL_NUMBER_SIZE   = "serial_number_size";
const char* MANUFACTURER_SIZE    = "manufacturer_size";
const char* VERSION_SIZE         = "version_size";
const char* ASSET_TAG_SIZE       = "asset_tag_size";

const char* SKU_ID_SIZE          = "sku_id_size";
const char* FRU_ID_SIZE          = "fru_id_size";
const char* FRU_FILE_ID_SIZE     = "fru_file_id_size";

const char* PRODUCT_NAME_SIZE    = "product_name_size";
const char* FAMILY_SIZE          = "family_size";

const char* RECORD_TYPE_ID       = "type_id";
const char* RECORD_FORMAT_VERSION    = "format_version";
const char* SUB_RECORD_TYPE      = "sub_type";
const char* RECORD_DATA          = "record_data";

const char* OEM_MAJOR_VER        = "oem_vpd_major_version";
const char* OEM_MINOR_VER        = "oem_vpd_minor_version";

const char* HOST_MAC_COUNT       = "host_mac_address_count";
const char* HOST_BASE_MAC        = "host_base_mac_address";

const char* BMC_MAC_COUNT        = "bmc_mac_address_count";
const char* BMC_BASE_MAC         = "bmc_base_mac_address";

const char* SWITCH_MAC_COUNT     = "switch_mac_address_count";
const char* SWITCH_BASE_MAC      = "switch_base_mac_address";

const char* MAX_FAN_SPEED        = "max_fan_speed";
const char* FAN_AIRFLOW          = "fan_airflow";

const char* VENDOR_ID            = "vendor_id";
const char* FAMILY               = "family";
const char* CONTROLLER_TYPE      = "controller_type";

const char* CUSTOMER_ID          = "customer_id";


int ( *packer )( const char *, char ** );
int ( *packerascii )( const char *, int, char ** );


uint8_t get_6bit_ascii( char c )
{
    return ( c - 0x20 ) & 0x3f;
}

uint8_t get_aligned_size( uint8_t size, uint8_t align )
{
    return ( size + align - 1 ) & ~( align - 1 );
}

uint8_t get_fru_tl_type( struct fru_type_length *ftl )
{
    return ftl->type_length & 0xc0;
}

uint8_t get_fru_tl_length( struct fru_type_length *ftl )
{
    return ftl->type_length & 0x3f;
}

uint8_t get_zero_cksum( uint8_t *data, int num_bytes )
{
    int sum = 0;
    while( num_bytes-- )
    {
        sum += *( data++ );
    }
    return -( sum % 256 );
}

char *get_key( const char *section, const char* key )
{
    int len;
    char *concat;

    /* 1 byte for : and another for nul */
    len = strlen( section ) + strlen( key ) + 2;
    concat = ( char * ) malloc( len );
    strcpy( concat, section );
    strcat( concat, ":" );
    strcat( concat, key );

    return concat;
}

int pack_ascii8_length( const char *str, int type_length, char **raw_data )
{
    char *data;
    struct fru_type_length *ftl;
    uint8_t tl = TYPE_CODE_UNILATIN;
    int len, size;

    len = strlen( str );
    size = 0;

    //uint8_t numbytes = len & 0x3f;

    uint8_t numbytes = type_length & 0x3f;

    /* Set length. It can be a max of 64 bytes */
    tl |= numbytes;

    size = numbytes + sizeof( struct fru_type_length );

    data = ( char * ) calloc( size, 1 );
    memset( data, 0x20, size ); //0x20 for unused bytes
    ftl = ( struct fru_type_length * ) data;
    ftl->type_length = tl;

    memcpy( ftl->data, str, len );

    *raw_data = data;
    return size;
}

int pack_ascii8( const char *str, char **raw_data )
{
    char *data;
    struct fru_type_length *ftl;
    uint8_t tl = TYPE_CODE_UNILATIN;
    int len, size;

    len = strlen( str );
    size = 0;

    uint8_t numbytes = len & 0x3f;

    /* Set length. It can be a max of 64 bytes */
    tl |= numbytes;

    size = numbytes + sizeof( struct fru_type_length );

    data = ( char * ) calloc( size, 1 );
    ftl = ( struct fru_type_length * ) data;
    ftl->type_length = tl;

    memcpy( ftl->data, str, len );

    *raw_data = data;
    return size;
}

int pack_ascii6( const char *str, char **raw_data )
{
    char *data;
    struct fru_type_length *ftl;
    uint8_t tl = TYPE_CODE_ASCII6;
    int len, size, i, j;

    len = strlen( str );
    size = 0;

    /* 6-bit ASCII packed allocates 6 bits per char */
    int rem = ( len * 6 ) % 8;
    int div = ( len * 6 ) / 8;
    uint8_t numbytes = ( rem ? div + 1 : div ) & 0x3f;

    /* Set length. It can be a max of 64 bytes */
    tl |= numbytes;

    size = numbytes + sizeof( struct fru_type_length );

    data = ( char * ) calloc( size, 1 );
    ftl = ( struct fru_type_length * ) data;
    ftl->type_length = tl;

    j = 0;
    for( i = 0; i + 3 < len; i += 4 )
    {
        *( ftl->data + j ) = get_6bit_ascii( str[i] ) | ( get_6bit_ascii( str[i + 1] ) << 6 );
        *( ftl->data + j + 1 ) = ( get_6bit_ascii( str[i + 1] ) >> 2 ) | ( get_6bit_ascii( str[i + 2] ) << 4 );
        *( ftl->data + j + 2 ) = ( get_6bit_ascii( str[i + 2] ) >> 4 ) | ( get_6bit_ascii( str[i + 3] ) << 2 );
        j += 3;
    }

    /* pack remaining (< 4) bytes */
    switch( ( len - i ) % 4 )
    {
        case 3:
            *( ftl->data + j ) = get_6bit_ascii( str[i] ) | ( get_6bit_ascii( str[i + 1] ) << 6 );
            *( ftl->data + j + 1 ) = ( get_6bit_ascii( str[i + 1] ) >> 2 ) | ( get_6bit_ascii( str[i + 2] ) << 4 );
            *( ftl->data + j + 2 ) = get_6bit_ascii( str[i + 2] ) >> 4;
            break;
        case 2:
            *( ftl->data + j ) = get_6bit_ascii( str[i] ) | ( get_6bit_ascii( str[i + 1] ) << 6 );
            *( ftl->data + j + 1 ) = get_6bit_ascii( str[i + 1] ) >> 2;
            break;
        case 1:
            *( ftl->data + j ) = get_6bit_ascii( str[i] );
        default:
            break;
    }

    *raw_data = data;
    return size;
}

/* All gen_* functions, except gen_iua(), return size as multiples of 8 */
int gen_iua( dictionary *ini, char **iua_data )
{
    int cksum, size;
    char *data;
    struct internal_use_area *iua;

    /* initialize some sane values */
    cksum = size = 0;
    data = NULL;

    size = sizeof( struct internal_use_area );
    data = ( char * ) calloc( size, 1 );

    /* Write format version */
    iua = ( ( struct internal_use_area * ) data );
    iua->format_version = 0x01;
    iua->area_length = 0x05;

    iua->end = 0xc1;
    cksum = get_zero_cksum( ( uint8_t * ) data, size - 1 );
    memcpy( data + size - 1, &cksum, 1 );

    *iua_data = data;

    return size;
}

int gen_cia( dictionary *ini, char **cia_data )
{
    struct chassis_info_area *cia;
    char *data,
         *str_data,
         *packed_ascii,
         *key,
         **sec_keys,
         *part_num_packed,
         *serial_num_packed,
         *name_packed,
         *skuid_packed,
         *version_packed,
         *manufacturer_packed,
         *asset_tag_packed;

    int chassis_type,
        size,
        offset,
        num_keys,
        part_num_size,
        serial_num_size,
        name_size,
        skuid_size,
        version_size,
        manufacturer_size,
        asset_tag_size,
        packed_size,
        key_size,
        i;

    uint8_t end_marker, empty_marker, cksum;

    cia = NULL;
    size = offset = cksum = empty_marker = key_size = 0;
    end_marker = 0xc1;

    chassis_type = iniparser_getint( ini, get_key( CIA, CHASSIS_TYPE ), 0 );
    if( !chassis_type )
    {
        /* 0 is an illegal chassis type */
        fprintf( stderr, "\nInvalid chassis type! Aborting\n\n" );
        exit( EXIT_FAILURE );
    }
    size += sizeof( struct chassis_info_area );

    str_data = iniparser_getstring( ini, get_key( CIA, PART_NUMBER ), NULL );
    if( str_data && strlen( str_data ) )
    {
        key_size = iniparser_getint( ini, get_key( CIA, PART_NUMBER_SIZE ), 0 );
        if( !key_size )
            key_size = ( strlen( str_data ) & 0x3f ) | TYPE_CODE_UNILATIN;
        part_num_size = ( *packerascii )( str_data, key_size, &part_num_packed );
        size += part_num_size;
    }
    else
    {
        /* predfined fields with no data take 1 byte (for type/length) */
        part_num_packed = NULL;
        size += 1;
    }

    str_data = iniparser_getstring( ini, get_key( CIA, SERIAL_NUMBER ), NULL );
    if( str_data && strlen( str_data ) )
    {
        key_size = iniparser_getint( ini, get_key( CIA, SERIAL_NUMBER_SIZE ), 0 );
        if( !key_size )
            key_size = ( strlen( str_data ) & 0x3f ) | TYPE_CODE_UNILATIN;
        serial_num_size = ( *packerascii )( str_data, key_size, &serial_num_packed );
        size += serial_num_size;
    }
    else
    {
        serial_num_packed = NULL;
        size += 1;
    }


    str_data = iniparser_getstring( ini, get_key( CIA, PRODUCT_NAME ), NULL );
    if( str_data && strlen( str_data ) )
    {
        key_size = iniparser_getint( ini, get_key( CIA, PRODUCT_NAME_SIZE ), 0 );
        if( !key_size )
            key_size = ( strlen( str_data ) & 0x3f ) | TYPE_CODE_UNILATIN;
        name_size = ( *packerascii )( str_data, key_size, &name_packed );
        size += name_size;
    }
    else
    {
        name_packed = NULL;
        size += 1;
    }

    str_data = iniparser_getstring( ini, get_key( CIA, SKU_ID ), NULL );
    if( str_data && strlen( str_data ) )
    {
        key_size = iniparser_getint( ini, get_key( CIA, SKU_ID_SIZE ), 0 );
        if( !key_size )
            key_size = ( strlen( str_data ) & 0x3f ) | TYPE_CODE_UNILATIN;
        skuid_size = ( *packerascii )( str_data, key_size, &skuid_packed );
        size += skuid_size;
    }
    else
    {
        skuid_packed = NULL;
        size += 1;
    }

    str_data = iniparser_getstring( ini, get_key( CIA, MANUFACTURER ), NULL );
    if( str_data && strlen( str_data ) )
    {
        key_size = iniparser_getint( ini, get_key( CIA, MANUFACTURER_SIZE ), 0 );
        if( !key_size )
            key_size = ( strlen( str_data ) & 0x3f ) | TYPE_CODE_UNILATIN;
        manufacturer_size = ( *packerascii )( str_data, key_size, &manufacturer_packed );
        size += manufacturer_size;
    }
    else
    {
        manufacturer_packed = NULL;
        size += 1;
    }

    str_data = iniparser_getstring( ini, get_key( CIA, VERSION ), NULL );
    if( str_data && strlen( str_data ) )
    {
        key_size = iniparser_getint( ini, get_key( CIA, VERSION_SIZE ), 0 );
        if( !key_size )
            key_size = ( strlen( str_data ) & 0x3f ) | TYPE_CODE_UNILATIN;
        version_size = ( *packerascii )( str_data, key_size, &version_packed );
        size += version_size;
    }
    else
    {
        version_packed = NULL;
        size += 1;
    }

    str_data = iniparser_getstring( ini, get_key( CIA, ASSET_TAG ), NULL );
    if( str_data && strlen( str_data ) )
    {
        key_size = iniparser_getint( ini, get_key( CIA, ASSET_TAG_SIZE ), 0 );
        if( !key_size )
            key_size = ( strlen( str_data ) & 0x3f ) | TYPE_CODE_UNILATIN;
        asset_tag_size = ( *packerascii )( str_data, key_size, &asset_tag_packed );
        size += asset_tag_size;
    }
    else
    {
        asset_tag_packed = NULL;
        size += 1;
    }

    num_keys = iniparser_getsecnkeys( ini, CIA );
    sec_keys = iniparser_getseckeys( ini, CIA );

    for( i = 0; i < num_keys; i++ )
    {
        key = sec_keys[i];
        /* Skip keys we've already accounted for */
        if( !strcmp( key, get_key( CIA, CHASSIS_TYPE ) ) ||
            !strcmp( key, get_key( CIA, PART_NUMBER ) ) ||
            !strcmp( key, get_key( CIA, SERIAL_NUMBER ) ) ||
            !strcmp( key, get_key( CIA, PRODUCT_NAME ) ) ||
            !strcmp( key, get_key( CIA, SKU_ID ) ) ||
            !strcmp( key, get_key( CIA, MANUFACTURER ) ) ||
            !strcmp( key, get_key( CIA, VERSION ) ) ||
            !strcmp( key, get_key( CIA, ASSET_TAG ) ) ||

            !strcmp( key, get_key( CIA, PART_NUMBER_SIZE ) ) ||
            !strcmp( key, get_key( CIA, SERIAL_NUMBER_SIZE ) ) ||
            !strcmp( key, get_key( CIA, PRODUCT_NAME_SIZE ) ) ||
            !strcmp( key, get_key( CIA, SKU_ID_SIZE ) ) ||
            !strcmp( key, get_key( CIA, MANUFACTURER_SIZE ) ) ||
            !strcmp( key, get_key( CIA, VERSION_SIZE ) ) ||
            !strcmp( key, get_key( CIA, ASSET_TAG_SIZE ) ) )
        {
            continue;
        }
        str_data = iniparser_getstring( ini, key, NULL );
        if( str_data && strlen( str_data ) )
        {
            size += ( *packer )( str_data, &packed_ascii );
        }
    }

    /* 2 bytes added for chksum & pad and end marker */
    size = get_aligned_size( size + 2 + 4, 8 );

    data = ( char * ) calloc( size, 1 );
    cia = ( struct chassis_info_area * ) data;

    /* Fill up CIA */
    cia->format_version = 0x01;
    /* Length is in multiples of 8 bytes */
    cia->area_length = size / 8;
    cia->chassis_type = chassis_type;

    if( part_num_packed )
    {
        memcpy( cia->tl + offset, part_num_packed, part_num_size );
        offset += part_num_size;
    }
    else
    {
        /* predfined fields with no data take 1 byte (for type/length) */
        memcpy( cia->tl + offset, &empty_marker, 1 );
        offset += 1;
    }

    if( serial_num_packed )
    {
        memcpy( cia->tl + offset, serial_num_packed, serial_num_size );
        offset += serial_num_size;
    }
    else
    {
        memcpy( cia->tl + offset, &empty_marker, 1 );
        offset += 1;
    }

    if( name_packed )
    {
        memcpy( cia->tl + offset, name_packed, name_size );
        offset += name_size;
    }
    else
    {
        memcpy( cia->tl + offset, &empty_marker, 1 );
        offset += 1;
    }


    if( skuid_packed )
    {
        memcpy( cia->tl + offset, skuid_packed, skuid_size );
        offset += skuid_size;
    }
    else
    {
        memcpy( cia->tl + offset, &empty_marker, 1 );
        offset += 1;
    }

    if( manufacturer_packed )
    {
        memcpy( cia->tl + offset, manufacturer_packed, manufacturer_size );
        offset += manufacturer_size;
    }
    else
    {
        memcpy( cia->tl + offset, &empty_marker, 1 );
        offset += 1;
    }

    if( version_packed )
    {
        memcpy( cia->tl + offset, version_packed, version_size );
        offset += version_size;
    }
    else
    {
        memcpy( cia->tl + offset, &empty_marker, 1 );
        offset += 1;
    }

    if( asset_tag_packed )
    {
        memcpy( cia->tl + offset, asset_tag_packed, asset_tag_size );
        offset += asset_tag_size;
    }
    else
    {
        memcpy( cia->tl + offset, &empty_marker, 1 );
        offset += 1;
    }

    for( i = 0; i < num_keys; i++ )
    {
        key = sec_keys[i];
        /* Skip keys we've already accounted for */
        if( !strcmp( key, get_key( CIA, CHASSIS_TYPE ) ) ||
            !strcmp( key, get_key( CIA, PART_NUMBER ) ) ||
            !strcmp( key, get_key( CIA, SERIAL_NUMBER ) ) ||
            !strcmp( key, get_key( CIA, PRODUCT_NAME ) ) ||
            !strcmp( key, get_key( CIA, SKU_ID ) ) ||
            !strcmp( key, get_key( CIA, MANUFACTURER ) ) ||
            !strcmp( key, get_key( CIA, VERSION ) ) ||
            !strcmp( key, get_key( CIA, ASSET_TAG ) ) ||

            !strcmp( key, get_key( CIA, PART_NUMBER_SIZE ) ) ||
            !strcmp( key, get_key( CIA, SERIAL_NUMBER_SIZE ) ) ||
            !strcmp( key, get_key( CIA, PRODUCT_NAME_SIZE ) ) ||
            !strcmp( key, get_key( CIA, SKU_ID_SIZE ) ) ||
            !strcmp( key, get_key( CIA, MANUFACTURER_SIZE ) ) ||
            !strcmp( key, get_key( CIA, VERSION_SIZE ) ) ||
            !strcmp( key, get_key( CIA, ASSET_TAG_SIZE ) ) )
        {

            continue;
        }
        str_data = iniparser_getstring( ini, key, NULL );
        if( str_data && strlen( str_data ) )
        {
            packed_size = ( *packer )( str_data, &packed_ascii );
            memcpy( cia->tl + offset, packed_ascii, packed_size );
            offset += packed_size;
        }
    }

    /* write the end marker 'C1' */
    memcpy( cia->tl + offset, &end_marker, 1 );
    offset += 1;
    //memcpy(cia->tl + offset, &pad, 4);
    /* Calculate checksum of entire CIA */
    cksum = get_zero_cksum( ( uint8_t * ) data, size - 1 );
    memcpy( data + size - 1, &cksum, 1 );

    *cia_data = data;

    return cia->area_length;
}

int gen_bia( dictionary *ini, char **bia_data )
{
    struct board_info_area *bia;

    char *data,
         *str_data,
         *packed_ascii,
         *mfg_packed,
         *name_packed,
         *serial_num_packed,
         *part_num_packed,
         *fru_file_id_packed,
         *version_packed,
         *asset_tag_packed,
         *key,
         **sec_keys;

    int lang_code,
        mfg_date,
        size,
        offset,
        mfg_size,
        name_size,
        serial_num_size,
        part_num_size,
        fru_file_id_size,
        version_size,
        asset_tag_size,
        num_keys,
        packed_size,
        key_size,
        i;

    uint8_t end_marker, empty_marker, cksum;
    static const uint64_t  secs_from_1970_1996 = 820454400;
    struct timeval tval;

    bia = NULL;
    size = offset = cksum = empty_marker = 0;
    end_marker = 0xc1;

    lang_code = iniparser_getint( ini, get_key( BIA, LANGUAGE_CODE ), -1 );
    if( lang_code == -1 )
    {
        fprintf( stdout, "Board language code not specified. "
                 "Defaulting to English\n" );
        lang_code = 0;
    }

    mfg_date = iniparser_getint( ini, get_key( BIA, MFG_DATETIME ), -1 );
    if( mfg_date == -1 )
    {
        fprintf( stdout, "Manufacturing time not specified. "
                 "Defaulting to current date\n" );

        gettimeofday( &tval, NULL );
        mfg_date = ( tval.tv_sec - secs_from_1970_1996 ) / 60;
        printf( "current: %ld, mfg: %d\n", tval.tv_sec, mfg_date );
    }
    size += sizeof( struct board_info_area );

    str_data = iniparser_getstring( ini, get_key( BIA, MANUFACTURER ), NULL );
    if( str_data && strlen( str_data ) )
    {
        key_size = iniparser_getint( ini, get_key( BIA, MANUFACTURER_SIZE ), 0 );
        if( !key_size )
            key_size = ( strlen( str_data ) & 0x3f ) | TYPE_CODE_UNILATIN;
        mfg_size = ( *packerascii )( str_data, key_size, &mfg_packed );
        size += mfg_size;
    }
    else
    {
        mfg_packed = NULL;
        size += 1;
    }

    str_data = iniparser_getstring( ini, get_key( BIA, PRODUCT_NAME ), NULL );
    if( str_data && strlen( str_data ) )
    {
        key_size = iniparser_getint( ini, get_key( BIA, PRODUCT_NAME_SIZE ), 0 );
        if( !key_size )
            key_size = ( strlen( str_data ) & 0x3f ) | TYPE_CODE_UNILATIN;
        name_size = ( *packerascii )( str_data, key_size, &name_packed );
        size += name_size;
    }
    else
    {
        name_packed = NULL;
        size += 1;
    }

    str_data = iniparser_getstring( ini, get_key( BIA, SERIAL_NUMBER ), NULL );
    if( str_data && strlen( str_data ) )
    {
        key_size = iniparser_getint( ini, get_key( BIA, SERIAL_NUMBER_SIZE ), 0 );
        if( !key_size )
            key_size = ( strlen( str_data ) & 0x3f ) | TYPE_CODE_UNILATIN;
        serial_num_size = ( *packerascii )( str_data, key_size, &serial_num_packed );
        size += serial_num_size;
    }
    else
    {
        serial_num_packed = NULL;
        size += 1;
    }

    str_data = iniparser_getstring( ini, get_key( BIA, PART_NUMBER ), NULL );
    if( str_data && strlen( str_data ) )
    {
        key_size = iniparser_getint( ini, get_key( BIA, PART_NUMBER_SIZE ), 0 );
        if( !key_size )
            key_size = ( strlen( str_data ) & 0x3f ) | TYPE_CODE_UNILATIN;
        part_num_size = ( *packerascii )( str_data, key_size, &part_num_packed );
        size += part_num_size;
    }
    else
    {
        part_num_packed = NULL;
        size += 1;
    }

    str_data = iniparser_getstring( ini, get_key( BIA, FRU_FILE_ID ), NULL );
    if( str_data && strlen( str_data ) )
    {
        key_size = iniparser_getint( ini, get_key( BIA, FRU_FILE_ID_SIZE ), 0 );
        if( !key_size )
            key_size = ( strlen( str_data ) & 0x3f ) | TYPE_CODE_UNILATIN;
        fru_file_id_size = ( *packerascii )( str_data, key_size, &fru_file_id_packed );
        size += fru_file_id_size;
    }
    else
    {
        fru_file_id_packed = NULL;
        size += 1;
    }

    str_data = iniparser_getstring( ini, get_key( BIA, VERSION ), NULL );
    if( str_data && strlen( str_data ) )
    {
        key_size = iniparser_getint( ini, get_key( BIA, VERSION_SIZE ), 0 );
        if( !key_size )
            key_size = ( strlen( str_data ) & 0x3f ) | TYPE_CODE_UNILATIN;
        version_size = ( *packerascii )( str_data, key_size, &version_packed );
        size += version_size;
    }
    else
    {
        version_packed = NULL;
        size += 1;
    }

    str_data = iniparser_getstring( ini, get_key( BIA, ASSET_TAG ), NULL );
    if( str_data && strlen( str_data ) )
    {
        key_size = iniparser_getint( ini, get_key( BIA, ASSET_TAG_SIZE ), 0 );
        if( !key_size )
            key_size = ( strlen( str_data ) & 0x3f ) | TYPE_CODE_UNILATIN;
        asset_tag_size = ( *packerascii )( str_data, key_size, &asset_tag_packed );
        size += asset_tag_size;
    }
    else
    {
        asset_tag_packed = NULL;
        size += 1;
    }


    /* We don't handle FRU File ID for now... */
    //size += 1;

    num_keys = iniparser_getsecnkeys( ini, BIA );
    sec_keys = iniparser_getseckeys( ini, BIA );

    for( i = 0; i < num_keys; i++ )
    {
        key = sec_keys[i];
        /* Skip keys we've already accounted for */
        if( !strcmp( key, get_key( BIA, LANGUAGE_CODE ) ) ||
            !strcmp( key, get_key( BIA, MFG_DATETIME ) ) ||
            !strcmp( key, get_key( BIA, MANUFACTURER ) ) ||
            !strcmp( key, get_key( BIA, PRODUCT_NAME ) ) ||
            !strcmp( key, get_key( BIA, SERIAL_NUMBER ) ) ||
            !strcmp( key, get_key( BIA, PART_NUMBER ) ) ||
            !strcmp( key, get_key( BIA, FRU_FILE_ID ) ) ||
            !strcmp( key, get_key( BIA, VERSION ) ) ||
            !strcmp( key, get_key( BIA, ASSET_TAG ) ) ||
            !strcmp( key, get_key( BIA, MANUFACTURER_SIZE ) ) ||
            !strcmp( key, get_key( BIA, PRODUCT_NAME_SIZE ) ) ||
            !strcmp( key, get_key( BIA, SERIAL_NUMBER_SIZE ) ) ||
            !strcmp( key, get_key( BIA, PART_NUMBER_SIZE ) ) ||
            !strcmp( key, get_key( BIA, FRU_FILE_ID_SIZE ) ) ||
            !strcmp( key, get_key( BIA, VERSION_SIZE ) ) ||
            !strcmp( key, get_key( BIA, ASSET_TAG_SIZE ) ) )
        {
            continue;
        }
        str_data = iniparser_getstring( ini, key, NULL );
        if( str_data && strlen( str_data ) )
        {
            size += ( *packer )( str_data, &packed_ascii );
        }
    }

    size = get_aligned_size( size + 2 + 4, 8 );

    data = ( char * ) calloc( size, 1 );
    bia = ( struct board_info_area * ) data;

    /* Fill up BIA */
    bia->format_version = 0x01;
    /* Length is in multiples of 8 bytes */
    bia->area_length = size / 8;
    bia->language_code = lang_code;
    mfg_date = htole32( mfg_date );
    memcpy( bia->mfg_date, &mfg_date, 3 );

    if( mfg_packed )
    {
        memcpy( bia->tl + offset, mfg_packed, mfg_size );
        offset += mfg_size;
    }
    else
    {
        /* predfined fields with no data take 1 byte (for type/length) */
        memcpy( bia->tl + offset, &empty_marker, 1 );
        offset += 1;
    }

    if( name_packed )
    {
        memcpy( bia->tl + offset, name_packed, name_size );
        offset += name_size;
    }
    else
    {
        memcpy( bia->tl + offset, &empty_marker, 1 );
        offset += 1;
    }

    if( serial_num_packed )
    {
        memcpy( bia->tl + offset, serial_num_packed, serial_num_size );
        offset += serial_num_size;
    }
    else
    {
        memcpy( bia->tl + offset, &empty_marker, 1 );
        offset += 1;
    }

    if( part_num_packed )
    {
        memcpy( bia->tl + offset, part_num_packed, part_num_size );
        offset += part_num_size;
    }
    else
    {
        memcpy( bia->tl + offset, &empty_marker, 1 );
        offset += 1;
    }

    if( fru_file_id_packed )
    {
        memcpy( bia->tl + offset, fru_file_id_packed, fru_file_id_size );
        offset += fru_file_id_size;
    }
    else
    {
        memcpy( bia->tl + offset, &empty_marker, 1 );
        offset += 1;
    }

    if( version_packed )
    {
        memcpy( bia->tl + offset, version_packed, version_size );
        offset += version_size;
    }
    else
    {
        memcpy( bia->tl + offset, &empty_marker, 1 );
        offset += 1;
    }

    if( asset_tag_packed )
    {
        memcpy( bia->tl + offset, asset_tag_packed, asset_tag_size );
        offset += asset_tag_size;
    }
    else
    {
        memcpy( bia->tl + offset, &empty_marker, 1 );
        offset += 1;
    }

    /* We don't handle FRU File ID for now... */
    //memcpy(bia->tl + offset, &empty_marker, 1);
    //offset += 1;

    for( i = 0; i < num_keys; i++ )
    {
        key = sec_keys[i];
        /* Skip keys we've already accounted for */
        if( !strcmp( key, get_key( BIA, LANGUAGE_CODE ) ) ||
            !strcmp( key, get_key( BIA, MFG_DATETIME ) ) ||
            !strcmp( key, get_key( BIA, MANUFACTURER ) ) ||
            !strcmp( key, get_key( BIA, PRODUCT_NAME ) ) ||
            !strcmp( key, get_key( BIA, SERIAL_NUMBER ) ) ||
            !strcmp( key, get_key( BIA, PART_NUMBER ) ) ||
            !strcmp( key, get_key( BIA, FRU_FILE_ID ) ) ||
            !strcmp( key, get_key( BIA, VERSION ) ) ||
            !strcmp( key, get_key( BIA, ASSET_TAG ) ) ||
            !strcmp( key, get_key( BIA, MANUFACTURER_SIZE ) ) ||
            !strcmp( key, get_key( BIA, PRODUCT_NAME_SIZE ) ) ||
            !strcmp( key, get_key( BIA, SERIAL_NUMBER_SIZE ) ) ||
            !strcmp( key, get_key( BIA, PART_NUMBER_SIZE ) ) ||
            !strcmp( key, get_key( BIA, FRU_FILE_ID_SIZE ) ) ||
            !strcmp( key, get_key( BIA, VERSION_SIZE ) ) ||
            !strcmp( key, get_key( BIA, ASSET_TAG_SIZE ) ) )
        {
            continue;
        }
        str_data = iniparser_getstring( ini, key, NULL );
        if( str_data && strlen( str_data ) )
        {
            packed_size = ( *packer )( str_data, &packed_ascii );
            memcpy( bia->tl + offset, packed_ascii, packed_size );
            offset += packed_size;
        }
    }
    /* write the end marker 'C1' */
    memcpy( bia->tl + offset, &end_marker, 1 );
    /* Calculate checksum of entire BIA */
    cksum = get_zero_cksum( ( uint8_t * ) data, size - 1 );
    memcpy( data + size - 1, &cksum, 1 );

    *bia_data = data;

    return bia->area_length;
}

int gen_pia( dictionary *ini, char **pia_data )
{
    struct product_info_area *pia;

    char *data,
         *str_data,
         *packed_ascii,
         *mfg_packed,
         *name_packed,
         *part_num_packed,
         *version_packed,
         *serial_num_packed,
         *asset_tag_packed,
         *fru_file_id_packed,
         *skuid_packed,
         *family_packed,
         *key,
         **sec_keys;

    int lang_code,
        size,
        offset,
        mfg_size,
        name_size,
        part_num_size,
        version_size,
        serial_num_size,
        asset_tag_size,
        fru_file_id_size,
        skuid_size,
        family_size,
        packed_size,
        num_keys,
        key_size,
        i;

    uint8_t end_marker, empty_marker, cksum;

    pia = NULL;
    size = offset = cksum = empty_marker = 0;
    end_marker = 0xc1;

    lang_code = iniparser_getint( ini, get_key( PIA, LANGUAGE_CODE ), -1 );
    if( lang_code == -1 )
    {
        fprintf( stdout, "Product language code not specified. "
                 "Defaulting to English\n" );
        lang_code = 0;
    }
    size += sizeof( struct product_info_area );

    str_data = iniparser_getstring( ini, get_key( PIA, MANUFACTURER ), NULL );
    if( str_data && strlen( str_data ) )
    {
        key_size = iniparser_getint( ini, get_key( PIA, MANUFACTURER_SIZE ), 0 );
        if( !key_size )
            key_size = ( strlen( str_data ) & 0x3f ) | TYPE_CODE_UNILATIN;
        mfg_size = ( *packerascii )( str_data, key_size, &mfg_packed );
        size += mfg_size;
    }
    else
    {
        mfg_packed = NULL;
        size += 1;
    }

    str_data = iniparser_getstring( ini, get_key( PIA, PRODUCT_NAME ), NULL );
    if( str_data && strlen( str_data ) )
    {
        key_size = iniparser_getint( ini, get_key( PIA, PRODUCT_NAME_SIZE ), 0 );
        if( !key_size )
            key_size = ( strlen( str_data ) & 0x3f ) | TYPE_CODE_UNILATIN;
        name_size = ( *packerascii )( str_data, key_size, &name_packed );
        size += name_size;
    }
    else
    {
        name_packed = NULL;
        size += 1;
    }

    str_data = iniparser_getstring( ini, get_key( PIA, PART_NUMBER ), NULL );
    if( str_data && strlen( str_data ) )
    {
        key_size = iniparser_getint( ini, get_key( PIA, PART_NUMBER_SIZE ), 0 );
        if( !key_size )
            key_size = ( strlen( str_data ) & 0x3f ) | TYPE_CODE_UNILATIN;
        part_num_size = ( *packerascii )( str_data, key_size, &part_num_packed );
        size += part_num_size;
    }
    else
    {
        part_num_packed = NULL;
        size += 1;
    }

    str_data = iniparser_getstring( ini, get_key( PIA, VERSION ), NULL );
    if( str_data && strlen( str_data ) )
    {
        key_size = iniparser_getint( ini, get_key( PIA, VERSION_SIZE ), 0 );
        if( !key_size )
            key_size = ( strlen( str_data ) & 0x3f ) | TYPE_CODE_UNILATIN;
        version_size = ( *packerascii )( str_data, key_size, &version_packed );
        size += version_size;
    }
    else
    {
        version_packed = NULL;
        size += 1;
    }

    str_data = iniparser_getstring( ini, get_key( PIA, SERIAL_NUMBER ), NULL );
    if( str_data && strlen( str_data ) )
    {
        key_size = iniparser_getint( ini, get_key( PIA, SERIAL_NUMBER_SIZE ), 0 );
        if( !key_size )
            key_size = ( strlen( str_data ) & 0x3f ) | TYPE_CODE_UNILATIN;
        serial_num_size = ( *packerascii )( str_data, key_size, &serial_num_packed );
        size += serial_num_size;
    }
    else
    {
        serial_num_packed = NULL;
        size += 1;
    }

    str_data = iniparser_getstring( ini, get_key( PIA, ASSET_TAG ), NULL );
    if( str_data && strlen( str_data ) )
    {
        key_size = iniparser_getint( ini, get_key( PIA, ASSET_TAG_SIZE ), 0 );
        if( !key_size )
            key_size = ( strlen( str_data ) & 0x3f ) | TYPE_CODE_UNILATIN;
        asset_tag_size = ( *packerascii )( str_data, key_size, &asset_tag_packed );
        size += asset_tag_size;
    }
    else
    {
        asset_tag_packed = NULL;
        size += 1;
    }

    str_data = iniparser_getstring( ini, get_key( PIA, FRU_FILE_ID ), NULL );
    if( str_data && strlen( str_data ) )
    {
        key_size = iniparser_getint( ini, get_key( PIA, FRU_FILE_ID_SIZE ), 0 );
        if( !key_size )
            key_size = ( strlen( str_data ) & 0x3f ) | TYPE_CODE_UNILATIN;
        fru_file_id_size = ( *packerascii )( str_data, key_size, &fru_file_id_packed );
        size += fru_file_id_size;
    }
    else
    {
        fru_file_id_packed = NULL;
        size += 1;
    }

    /* We don't handle FRU File ID for now... */
    //size += 1;
    str_data = iniparser_getstring( ini, get_key( PIA, PRODUCT_FAMILY ), NULL );
    if( str_data && strlen( str_data ) )
    {
        key_size = iniparser_getint( ini, get_key( PIA, FAMILY_SIZE ), 0 );
        if( !key_size )
            key_size = ( strlen( str_data ) & 0x3f ) | TYPE_CODE_UNILATIN;
        family_size = ( *packerascii )( str_data, key_size, &family_packed );
        size += family_size;
    }
    else
    {
        family_packed = NULL;
        size += 1;
    }

    str_data = iniparser_getstring( ini, get_key( PIA, SKU_ID ), NULL );
    if( str_data && strlen( str_data ) )
    {
        key_size = iniparser_getint( ini, get_key( PIA, SKU_ID_SIZE ), 0 );
        if( !key_size )
            key_size = ( strlen( str_data ) & 0x3f ) | TYPE_CODE_UNILATIN;
        skuid_size = ( *packerascii )( str_data, key_size, &skuid_packed );
        size += skuid_size;
    }
    else
    {
        skuid_packed = NULL;
        size += 1;
    }


    num_keys = iniparser_getsecnkeys( ini, PIA );
    sec_keys = iniparser_getseckeys( ini, PIA );

    /* first iteration calculates the amount of space needed */
    for( i = 0; i < num_keys; i++ )
    {
        key = sec_keys[i];
        /* Skip keys we've already accounted for */
        if( !strcmp( key, get_key( PIA, LANGUAGE_CODE ) ) ||
            !strcmp( key, get_key( PIA, MANUFACTURER ) ) ||
            !strcmp( key, get_key( PIA, PRODUCT_NAME ) ) ||
            !strcmp( key, get_key( PIA, PART_NUMBER ) ) ||
            !strcmp( key, get_key( PIA, VERSION ) ) ||
            !strcmp( key, get_key( PIA, SERIAL_NUMBER ) ) ||
            !strcmp( key, get_key( PIA, ASSET_TAG ) ) ||
            !strcmp( key, get_key( PIA, FRU_FILE_ID ) ) ||
            !strcmp( key, get_key( PIA, PRODUCT_FAMILY ) ) ||
            !strcmp( key, get_key( PIA, SKU_ID ) ) ||
            !strcmp( key, get_key( PIA, MANUFACTURER_SIZE ) ) ||
            !strcmp( key, get_key( PIA, PRODUCT_NAME_SIZE ) ) ||
            !strcmp( key, get_key( PIA, PART_NUMBER_SIZE ) ) ||
            !strcmp( key, get_key( PIA, VERSION_SIZE ) ) ||
            !strcmp( key, get_key( PIA, SERIAL_NUMBER_SIZE ) ) ||
            !strcmp( key, get_key( PIA, ASSET_TAG_SIZE ) ) ||
            !strcmp( key, get_key( PIA, FRU_FILE_ID_SIZE ) ) ||
            !strcmp( key, get_key( PIA, FAMILY_SIZE ) ) ||
            !strcmp( key, get_key( PIA, SKU_ID_SIZE ) ) )
        {
            continue;
        }
        str_data = iniparser_getstring( ini, key, NULL );
        if( str_data && strlen( str_data ) )
        {
            size += ( *packer )( str_data, &packed_ascii );
        }
    }

    size = get_aligned_size( size + 2, 8 );

    data = ( char * ) calloc( size, 1 );
    pia = ( struct product_info_area * ) data;

    /* Fill up PIA */
    pia->format_version = 0x01;
    /* Length is in multiples of 8 bytes */
    pia->area_length = size / 8;
    pia->language_code = lang_code;

    if( mfg_packed )
    {
        memcpy( pia->tl + offset, mfg_packed, mfg_size );
        offset += mfg_size;
    }
    else
    {
        memcpy( pia->tl + offset, &empty_marker, 1 );
        offset += 1;
    }

    if( name_packed )
    {
        memcpy( pia->tl + offset, name_packed, name_size );
        offset += name_size;
    }
    else
    {
        /* predfined fields with no data take 1 byte (for type/length) */
        memcpy( pia->tl + offset, &empty_marker, 1 );
        offset += 1;
    }

    if( part_num_packed )
    {
        memcpy( pia->tl + offset, part_num_packed, part_num_size );
        offset += part_num_size;
    }
    else
    {
        memcpy( pia->tl + offset, &empty_marker, 1 );
        offset += 1;
    }

    if( version_packed )
    {
        memcpy( pia->tl + offset, version_packed, version_size );
        offset += version_size;
    }
    else
    {
        memcpy( pia->tl + offset, &empty_marker, 1 );
        offset += 1;
    }

    if( serial_num_packed )
    {
        memcpy( pia->tl + offset, serial_num_packed, serial_num_size );
        offset += serial_num_size;
    }
    else
    {
        memcpy( pia->tl + offset, &empty_marker, 1 );
        offset += 1;
    }

    if( asset_tag_packed )
    {
        memcpy( pia->tl + offset, asset_tag_packed, asset_tag_size );
        offset += asset_tag_size;
    }
    else
    {
        memcpy( pia->tl + offset, &empty_marker, 1 );
        offset += 1;
    }
    if( fru_file_id_packed )
    {
        memcpy( pia->tl + offset, fru_file_id_packed, fru_file_id_size );
        offset += fru_file_id_size;
    }
    else
    {
        memcpy( pia->tl + offset, &empty_marker, 1 );
        offset += 1;
    }

    if( family_packed )
    {
        memcpy( pia->tl + offset, family_packed, family_size );
        offset += family_size;
    }
    else
    {
        memcpy( pia->tl + offset, &empty_marker, 1 );
        offset += 1;
    }

    if( skuid_packed )
    {
        memcpy( pia->tl + offset, skuid_packed, skuid_size );
        offset += skuid_size;
    }
    else
    {
        memcpy( pia->tl + offset, &empty_marker, 1 );
        offset += 1;
    }

    /* We don't handle FRU File ID for now... */
    //memcpy(pia->tl + offset, &empty_marker, 1);
    // offset += 1;

    /* Second iteration copies packed contents into final buffer */
    for( i = 0; i < num_keys; i++ )
    {
        key = sec_keys[i];
        /* Skip keys we've already accounted for */
        if( !strcmp( key, get_key( PIA, LANGUAGE_CODE ) ) ||
            !strcmp( key, get_key( PIA, MANUFACTURER ) ) ||
            !strcmp( key, get_key( PIA, PRODUCT_NAME ) ) ||
            !strcmp( key, get_key( PIA, PART_NUMBER ) ) ||
            !strcmp( key, get_key( PIA, VERSION ) ) ||
            !strcmp( key, get_key( PIA, SERIAL_NUMBER ) ) ||
            !strcmp( key, get_key( PIA, ASSET_TAG ) ) ||
            !strcmp( key, get_key( PIA, FRU_FILE_ID ) ) ||
            !strcmp( key, get_key( PIA, PRODUCT_FAMILY ) ) ||
            !strcmp( key, get_key( PIA, SKU_ID ) ) ||
            !strcmp( key, get_key( PIA, MANUFACTURER_SIZE ) ) ||
            !strcmp( key, get_key( PIA, PRODUCT_NAME_SIZE ) ) ||
            !strcmp( key, get_key( PIA, PART_NUMBER_SIZE ) ) ||
            !strcmp( key, get_key( PIA, VERSION_SIZE ) ) ||
            !strcmp( key, get_key( PIA, SERIAL_NUMBER_SIZE ) ) ||
            !strcmp( key, get_key( PIA, ASSET_TAG_SIZE ) ) ||
            !strcmp( key, get_key( PIA, FRU_FILE_ID_SIZE ) ) ||
            !strcmp( key, get_key( PIA, FAMILY_SIZE ) ) ||
            !strcmp( key, get_key( PIA, SKU_ID_SIZE ) ) )
        {
            continue;
        }
        str_data = iniparser_getstring( ini, key, NULL );
        if( str_data && strlen( str_data ) )
        {
            packed_size = ( *packer )( str_data, &packed_ascii );
            memcpy( pia->tl + offset, packed_ascii, packed_size );
            offset += packed_size;
        }
    }
    /* write the end marker 'C1' */
    memcpy( pia->tl + offset, &end_marker, 1 );
    /* Calculate checksum of entire PIA */
    cksum = get_zero_cksum( ( uint8_t * )data, size - 1 );
    memcpy( data + size - 1, &cksum, 1 );

    *pia_data = data;

    return pia->area_length;
}

int gen_mia_mar( dictionary * ini, char * * mia_data )
{
    struct management_access_record *mar;
    char *data,
         *uuid_str_data;

    int record_type_id,
        record_format_version,
        sub_record_type,
        size,
        offset;

    uint8_t headercksum, cksum;

    mar = NULL;
    size = offset = cksum = headercksum = 0;

    record_type_id        = iniparser_getint( ini, get_key( MIA_MAR, RECORD_TYPE_ID ), 0 );
    record_format_version = iniparser_getint( ini, get_key( MIA_MAR, RECORD_FORMAT_VERSION ), 0 );
    sub_record_type       = iniparser_getint( ini, get_key( MIA_MAR, SUB_RECORD_TYPE ), 0 );

    uuid_str_data = iniparser_getstring( ini, get_key( MIA_MAR, RECORD_DATA ), NULL );
    if( uuid_str_data == NULL && strlen( uuid_str_data ) != UUID_STR_LENGTH )
    {
        fprintf( stderr, "\nInvalid UUID data\n\n" );
        exit( EXIT_FAILURE );
    }

    // mia_mar struct size
    size = sizeof( struct management_access_record );
    data = ( char * ) calloc( size, 1 );
    mar = ( struct management_access_record * ) data;

    memset( mar->record_data, 0, UUID_BYTE_LENGTH );

    sscanf( uuid_str_data,
            "%2hhx%2hhx%2hhx%2hhx-"
            "%2hhx%2hhx-"
            "%2hhx%2hhx-"
            "%2hhx%2hhx-"
            "%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx",
            &mar->record_data[0], &mar->record_data[1], &mar->record_data[2], &mar->record_data[3],
            &mar->record_data[4], &mar->record_data[5],
            &mar->record_data[6], &mar->record_data[7],
            &mar->record_data[8], &mar->record_data[9],
            &mar->record_data[10], &mar->record_data[11], &mar->record_data[12], &mar->record_data[13],
            &mar->record_data[14], &mar->record_data[15] );

    /* Fill up MAR */
    mar->record_header.type_id = record_type_id;
    mar->record_header.format_version = record_format_version;
    mar->record_header.record_length = size - sizeof( struct multi_record_header ); // multi-record header size is 5
    mar->sub_record_type = sub_record_type;


    offset = sizeof( struct multi_record_header );
    cksum = get_zero_cksum( ( uint8_t * ) data + offset, mar->record_header.record_length );
    mar->record_header.record_checksum = cksum;

    headercksum = get_zero_cksum( ( uint8_t * ) data, sizeof( struct multi_record_header ) - 1 );
    mar->record_header.header_checksum = headercksum;

    *mia_data = data;

    return size;
}

int gen_mia_ver( dictionary * ini, char * * mia_data )
{
    struct oem_vpd_version *oem_ver;
    char *data;

    int record_type_id,
        record_format_version,
        major_version,
        minor_version,
        size,
        offset;

    uint8_t headercksum, cksum;

    oem_ver = NULL;
    size = offset = cksum = headercksum = 0;

    record_type_id        = iniparser_getint( ini, get_key( MIA_VER, RECORD_TYPE_ID ), 0 );
    record_format_version = iniparser_getint( ini, get_key( MIA_VER, RECORD_FORMAT_VERSION ), 0 );
    major_version         = iniparser_getint( ini, get_key( MIA_VER, OEM_MAJOR_VER ), 0 );
    minor_version         = iniparser_getint( ini, get_key( MIA_VER, OEM_MINOR_VER ), 0 );

    size += sizeof( struct oem_vpd_version );
    data = ( char * ) calloc( size, 1 );
    oem_ver = ( struct oem_vpd_version * ) data;

    /* Fill up CIA */
    oem_ver->record_header.type_id = record_type_id;
    oem_ver->record_header.format_version = record_format_version;
    oem_ver->record_header.record_length = size - sizeof( struct multi_record_header );

    oem_ver->major_version = major_version;
    oem_ver->minor_version = minor_version;

    offset = sizeof( struct multi_record_header );
    cksum = get_zero_cksum( ( uint8_t * ) data + offset, oem_ver->record_header.record_length );
    oem_ver->record_header.record_checksum = cksum;

    headercksum = get_zero_cksum( ( uint8_t * ) data, sizeof( struct multi_record_header ) - 1 );
    oem_ver->record_header.header_checksum = headercksum;

    *mia_data = data;

    return size;
}

int gen_mia_mac( dictionary * ini, char * * mia_data )
{
    struct mac_address *mac;
    char *data,
         *host_base_address,
         *bmc_base_address,
         *switch_base_address;

    int record_type_id,
        record_format_version,
        host_mac_count,
        bmc_mac_count,
        switch_mac_count,
        size,
        offset;

    uint8_t headercksum, cksum;

    mac = NULL;
    size = offset = cksum = headercksum = 0;

    record_type_id        = iniparser_getint( ini, get_key( MIA_MAC, RECORD_TYPE_ID ), 0 );
    record_format_version = iniparser_getint( ini, get_key( MIA_MAC, RECORD_FORMAT_VERSION ), 0 );
    host_mac_count        = iniparser_getint( ini, get_key( MIA_MAC, HOST_MAC_COUNT ), 0 );
    bmc_mac_count         = iniparser_getint( ini, get_key( MIA_MAC, BMC_MAC_COUNT ), 0 );
    switch_mac_count      = iniparser_getint( ini, get_key( MIA_MAC, SWITCH_MAC_COUNT ), 0 );

    host_base_address = iniparser_getstring( ini, get_key( MIA_MAC, HOST_BASE_MAC ), NULL );
    if( host_base_address == NULL || strlen( host_base_address ) != MAC_ADDRESS_STR_LENGTH )
    {
        fprintf( stderr, "\nInvalid Host Base MAC Address\n\n" );
        exit( EXIT_FAILURE );
    }

    bmc_base_address = iniparser_getstring( ini, get_key( MIA_MAC, HOST_BASE_MAC ), NULL );
    if( bmc_base_address == NULL || strlen( bmc_base_address ) != MAC_ADDRESS_STR_LENGTH )
    {
        fprintf( stderr, "\nInvalid BMC Base MAC Address\n\n" );
        exit( EXIT_FAILURE );
    }

    switch_base_address = iniparser_getstring( ini, get_key( MIA_MAC, HOST_BASE_MAC ), NULL );
    if( switch_base_address == NULL || strlen( switch_base_address ) != MAC_ADDRESS_STR_LENGTH )
    {
        fprintf( stderr, "\nInvalid Switch Base MAC Address\n\n" );
        exit( EXIT_FAILURE );
    }

    size += sizeof( struct mac_address );
    data = ( char * ) calloc( size, 1 );
    mac = ( struct mac_address * ) data;

    /* Fill up CIA */
    mac->record_header.type_id = record_type_id;
    mac->record_header.format_version = record_format_version;
    /* Length is in multiples of 8 bytes */
    mac->record_header.record_length = size - sizeof( struct multi_record_header );

    mac->host_mac_address_count = host_mac_count;
    sscanf( host_base_address, "%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx",
            &mac->host_base_mac_address[0], &mac->host_base_mac_address[1],
            &mac->host_base_mac_address[2], &mac->host_base_mac_address[3],
            &mac->host_base_mac_address[4], &mac->host_base_mac_address[5] );

    mac->bmc_mac_address_count = bmc_mac_count;
    sscanf( host_base_address, "%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx",
            &mac->bmc_base_mac_address[0], &mac->bmc_base_mac_address[1],
            &mac->bmc_base_mac_address[2], &mac->bmc_base_mac_address[3],
            &mac->bmc_base_mac_address[4], &mac->bmc_base_mac_address[5] );

    mac->switch_mac_address_count = switch_mac_count;
    sscanf( host_base_address, "%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx",
            &mac->switch_base_mac_address[0], &mac->switch_base_mac_address[1],
            &mac->switch_base_mac_address[2], &mac->switch_base_mac_address[3],
            &mac->switch_base_mac_address[4], &mac->switch_base_mac_address[5] );


    offset = sizeof( struct multi_record_header );
    cksum = get_zero_cksum( ( uint8_t * ) data + offset, mac->record_header.record_length );
    mac->record_header.record_checksum = cksum;

    headercksum = get_zero_cksum( ( uint8_t * ) data, sizeof( struct multi_record_header ) - 1 );
    mac->record_header.header_checksum = headercksum;

    *mia_data = data;

    return size;
}

int gen_mia_fan( dictionary * ini, char * * mia_data )
{
    struct fan_speed_control_parameter *fan;
    char *data;

    int record_type_id,
        record_format_version,
        fan_speed,
        fan_airflow,
        size,
        offset;

    uint8_t headercksum, cksum;

    fan = NULL;
    size = offset = cksum = headercksum = 0;

    record_type_id        = iniparser_getint( ini, get_key( MIA_FAN, RECORD_TYPE_ID ), 0 );
    record_format_version = iniparser_getint( ini, get_key( MIA_FAN, RECORD_FORMAT_VERSION ), 0 );
    fan_speed             = iniparser_getint( ini, get_key( MIA_FAN, MAX_FAN_SPEED ), 0 );
    fan_airflow           = iniparser_getint( ini, get_key( MIA_FAN, FAN_AIRFLOW ), 0 );

    size += sizeof( struct fan_speed_control_parameter );
    data = ( char * ) calloc( size, 1 );
    fan = ( struct fan_speed_control_parameter * ) data;

    /* Fill up CIA */
    fan->record_header.type_id = record_type_id;
    fan->record_header.format_version = record_format_version;
    /* Length is in multiples of 8 bytes */
    fan->record_header.record_length = size - sizeof( struct multi_record_header );

    fan->max_fan_speed = fan_speed;
    fan->fan_airflow = fan_airflow;

    offset = sizeof( struct multi_record_header );
    cksum = get_zero_cksum( ( uint8_t * ) data + offset, fan->record_header.record_length );
    fan->record_header.record_checksum = cksum;

    headercksum = get_zero_cksum( ( uint8_t * ) data, sizeof( struct multi_record_header ) - 1 );
    fan->record_header.header_checksum = headercksum;

    *mia_data = data;

    return size;
}

int gen_mia_bci( dictionary * ini, char * * mia_data )
{
    struct board_controller_info *bci;
    char *data,
         *packed_vendor_id,
         *packed_family,
         *packed_type;

    int record_type_id,
        record_format_version,
        size,
        offset;

    uint8_t headercksum, cksum;

    bci = NULL;
    size = offset = cksum = headercksum = 0;

    record_type_id        = iniparser_getint( ini, get_key( MIA_BCI, RECORD_TYPE_ID ), 0 );
    record_format_version = iniparser_getint( ini, get_key( MIA_BCI, RECORD_FORMAT_VERSION ), 0 );

    packed_vendor_id = iniparser_getstring( ini, get_key( MIA_BCI, VENDOR_ID ), NULL );
    if( packed_vendor_id == NULL || strlen( packed_vendor_id ) > CPU_VENDOR_ID_STR_LENGTH )
    {
        fprintf( stderr, "\nInvalid CPU type ID\n\n" );
        exit( EXIT_FAILURE );
    }

    packed_family = iniparser_getstring( ini, get_key( MIA_BCI, FAMILY ), NULL );
    if( packed_family == NULL || strlen( packed_family ) > CPU_FAMILY_STR_LENGTH )
    {
        fprintf( stderr, "\nInvalid CPU type ID\n\n" );
        exit( EXIT_FAILURE );
    }

    packed_type = iniparser_getstring( ini, get_key( MIA_BCI, CONTROLLER_TYPE ), NULL );
    if( packed_type == NULL || strlen( packed_type ) > CPU_TYPE_STR_LENGTH )
    {
        fprintf( stderr, "\nInvalid CPU type ID\n\n" );
        exit( EXIT_FAILURE );
    }

    size += sizeof( struct board_controller_info );
    data = ( char * ) calloc( size, 1 );
    bci = ( struct board_controller_info * ) data;

    /* Fill up CIA */
    bci->record_header.type_id = record_type_id;
    bci->record_header.format_version = record_format_version;
    /* Length is in multiples of 8 bytes */
    bci->record_header.record_length = size - sizeof( struct multi_record_header );

    memset( bci->vendor_id, 0, CPU_VENDOR_ID_STR_LENGTH );
    memcpy( bci->vendor_id, packed_vendor_id, CPU_VENDOR_ID_STR_LENGTH );
    memset( bci->family, 0, CPU_FAMILY_STR_LENGTH );
    memcpy( bci->family, packed_family, CPU_FAMILY_STR_LENGTH );
    memset( bci->type, 0, CPU_TYPE_STR_LENGTH );
    memcpy( bci->type, packed_type, CPU_TYPE_STR_LENGTH );

    offset = sizeof( struct multi_record_header );
    cksum = get_zero_cksum( ( uint8_t * ) data + offset, bci->record_header.record_length );
    bci->record_header.record_checksum = cksum;

    headercksum = get_zero_cksum( ( uint8_t * ) data, sizeof( struct multi_record_header ) - 1 );
    bci->record_header.header_checksum = headercksum;

    *mia_data = data;

    return size;
}

int gen_mia_sysc( dictionary * ini, char * * mia_data )
{
    struct system_configuration *sysc;
    char *data;

    int record_type_id,
        record_format_version,
        customer_id,
        size,
        offset;

    uint8_t headercksum, cksum;

    sysc = NULL;
    size = offset = cksum = headercksum = 0;

    record_type_id        = iniparser_getint( ini, get_key( MIA_SC, RECORD_TYPE_ID ), 0 );
    record_format_version = iniparser_getint( ini, get_key( MIA_SC, RECORD_FORMAT_VERSION ), 0 );
    customer_id           = iniparser_getint( ini, get_key( MIA_SC, CUSTOMER_ID ), 0 );

    size += sizeof( struct system_configuration );
    data = ( char * ) calloc( size, 1 );
    sysc = ( struct system_configuration * ) data;

    /* Fill up CIA */
    sysc->record_header.type_id = record_type_id;
    sysc->record_header.format_version = record_format_version;
    /* Length is in multiples of 8 bytes */
    sysc->record_header.record_length = size - sizeof( struct multi_record_header );
    sysc->customer_id = customer_id;

    offset = sizeof( struct multi_record_header );
    cksum = get_zero_cksum( ( uint8_t * ) data + offset, sysc->record_header.record_length );
    sysc->record_header.record_checksum = cksum;

    headercksum = get_zero_cksum( ( uint8_t * ) data, sizeof( struct multi_record_header ) - 1 );
    sysc->record_header.header_checksum = headercksum;

    *mia_data = data;

    return size;
}

int gen_fru_data( dictionary *ini, char **raw_data )
{
    int total_length,
        offset,
        mia_mar_offset,
        mia_ver_offset,
        mia_mac_offset,
        mia_fan_offset,
        mia_bci_offset,
        mia_sysc_offset,
        mia_mar_len_mul8,
        mia_ver_len_mul8,
        mia_mac_len_mul8,
        mia_fan_len_mul8,
        mia_bci_len_mul8,
        mia_sysc_len_mul8,
        len_mul8,
        iua_len,
        size,
        cksum;

    char *iua, *cia, *bia, *pia, *mia_mar, *mia_ver, *mia_mac, *mia_fan, *mia_bci, *mia_sysc, *data;

    iua = cia = bia = pia = data = NULL;
    total_length = offset = len_mul8 = iua_len = size = cksum = 0;
    mia_mar_offset = mia_ver_offset = mia_mac_offset = mia_fan_offset = mia_bci_offset = mia_sysc_offset = 0;
    mia_mar_len_mul8 = mia_ver_len_mul8 = mia_mac_len_mul8 = mia_fan_len_mul8 = mia_bci_len_mul8 = mia_sysc_len_mul8 = 0;

    /* A common header always exists even if there's no FRU data */
    struct fru_common_header *fch =
        ( struct fru_common_header * ) calloc( sizeof( struct fru_common_header ),
                1 );
    fch->format_version = 0x01;
    total_length += sizeof( struct fru_common_header );
    offset = total_length / 8;

    /* Parse "Internal Use Area" (IUA) section */
    if( iniparser_find_entry( ini, IUA ) )
    {
        iua_len = gen_iua( ini, &iua );
        fch->internal_use_offset = offset;

        offset += ( iua_len / 8 );
        total_length += iua_len;
    }

    /* Parse "Chassis Info Area" (CIA) section */
    if( iniparser_find_entry( ini, CIA ) )
    {
        len_mul8 = gen_cia( ini, &cia );
        fch->chassis_info_offset = offset;

        offset += len_mul8;
        total_length += len_mul8 * 8;
    }

    /* Parse "Board Info Area" (BIA) section */
    if( iniparser_find_entry( ini, BIA ) )
    {
        len_mul8 = gen_bia( ini, &bia );
        fch->board_info_offset = offset;

        offset += len_mul8;
        total_length += len_mul8 * 8;
    }

    /* Parse "Product Info Area" (PIA) section */
    if( iniparser_find_entry( ini, PIA ) )
    {
        len_mul8 = gen_pia( ini, &pia );
        fch->product_info_offset = offset;

        offset += len_mul8;
        total_length += len_mul8 * 8;
    }

    /* The offset changes to actual offset from this line. */

    /* Parse "MultiRecord Info Area" (MIA_MAR) section */
    if( iniparser_find_entry( ini, MIA_MAR ) )
    {
        mia_mar_len_mul8 = len_mul8 = gen_mia_mar( ini, &mia_mar );
        fch->multirecord_info_offset = offset;
        offset *= 8; // actual offset
        mia_mar_offset = offset;

        offset += len_mul8;
        total_length += len_mul8;
    }

    /* Parse "MultiRecord Info Area" (MIA_VER) section */
    if( iniparser_find_entry( ini, MIA_VER ) )
    {
        mia_ver_len_mul8 = len_mul8 = gen_mia_ver( ini, &mia_ver );
        mia_ver_offset = offset;

        offset += len_mul8;
        total_length += len_mul8;
    }

    /* Parse "MultiRecord Info Area" (MIA_MAC) section */
    if( iniparser_find_entry( ini, MIA_MAC ) )
    {
        mia_mac_len_mul8 = len_mul8 = gen_mia_mac( ini, &mia_mac );
        mia_mac_offset = offset;

        offset += len_mul8;
        total_length += len_mul8;
    }

    /* Parse "MultiRecord Info Area" (MIA_FAN) section */
    if( iniparser_find_entry( ini, MIA_FAN ) )
    {
        mia_fan_len_mul8 = len_mul8 = gen_mia_fan( ini, &mia_fan );
        mia_fan_offset += offset;

        offset += len_mul8;
        total_length += len_mul8;
    }

    /* Parse "MultiRecord Info Area" (MIA_BCI) section */
    if( iniparser_find_entry( ini, MIA_BCI ) )
    {
        mia_bci_len_mul8 = len_mul8 = gen_mia_bci( ini, &mia_bci );
        mia_bci_offset = offset;

        offset += len_mul8;
        total_length += len_mul8;
    }

    /* Parse "MultiRecord Info Area" (MIA_SC) section */
    if( iniparser_find_entry( ini, MIA_SC ) )
    {
        mia_sysc_len_mul8 = len_mul8 = gen_mia_sysc( ini, &mia_sysc );
        mia_sysc_offset = offset;

        offset += len_mul8;
        total_length += len_mul8;
    }

    /* calculate header checksum */
    cksum = get_zero_cksum( ( uint8_t * ) fch, sizeof( *fch ) - 1 );
    fch->checksum = cksum;

    //    fprintf( stderr, "\ngen_fru_data get size: %d\n", total_length );
    data = ( char * ) malloc( total_length );

    /* Copy common header first */
    memcpy( data, fch, sizeof( struct fru_common_header ) );

    /* Copy each section's data if any */
    if( iua )
    {
        offset = fch->internal_use_offset * 8;
        size = *( iua + 1 ) * 8;
        memcpy( data + offset, iua, size );
    }

    if( cia )
    {
        offset = fch->chassis_info_offset * 8;
        size = *( cia + 1 ) * 8;
        memcpy( data + offset, cia, size );
    }

    if( bia )
    {
        offset = fch->board_info_offset * 8;
        size = *( bia + 1 ) * 8;
        memcpy( data + offset, bia, size );
    }

    if( pia )
    {
        offset = fch->product_info_offset * 8;
        size = *( pia + 1 ) * 8;
        memcpy( data + offset, pia, size );
    }

    if( mia_mar )
    {
        memcpy( data + mia_mar_offset, mia_mar, mia_mar_len_mul8 );
    }

    if( mia_ver )
    {
        memcpy( data + mia_ver_offset, mia_ver, mia_ver_len_mul8 );
    }

    if( mia_mac )
    {
        memcpy( data + mia_mac_offset, mia_mac, mia_mac_len_mul8 );
    }

    if( mia_fan )
    {
        memcpy( data + mia_fan_offset, mia_fan, mia_fan_len_mul8 );
    }

    if( mia_bci )
    {
        memcpy( data + mia_bci_offset, mia_bci, mia_bci_len_mul8 );
    }
    if( mia_sysc )
    {
        memcpy( data + mia_sysc_offset, mia_sysc, mia_sysc_len_mul8 );
    }
    *raw_data = data;

    return total_length;
}

int write_fru_data( const char*filename, void *data, int length )
{
    int fd, flags;
    mode_t mode;

    fd = -1;
    flags = O_RDWR | O_CREAT | O_TRUNC;
    mode = S_IRWXU | S_IRGRP | S_IROTH;

    if( ( fd = open( filename, flags, mode ) ) == -1 )
    {
        perror( "File open:" );
        return -1;
    }

    write( fd, data, length );
    close( fd );

    return 0;
}
void ShowHelp( char *str )
{
    printf( "*************************************************************\n" );
    fprintf( stdout, "*                FRU BIN GENERATE TOOL V%s                 *\n", TOOL_VERSION );
    printf( "*************************************************************\n" );
    fprintf( stdout, usage, str );
    printf( "\tGenerating a FRU data file using 8-bit ASCII:\n" );
    printf( "\t   ipmi-fru-it -s 2048 -c fru.conf -o FRU.bin -a\n" );
    printf( "\tReading a FRU data file(Not implemented, please use):\n" );
    printf( "\t   ipmi-fru-it -r -i FRU.bin\n" );
}

int main( int argc, char **argv )
{
    char *fru_ini_file, *outfile, *data;
    int c, length, max_size = 0, result;
    dictionary *ini;

    /* supported cmdline options */
    char options[] = "hvri:aws:c:o:";

    fru_ini_file = outfile = data = NULL;
    ini = NULL;
    fprintf( stdout, "*********FRU BIN GENERATE TOOL V%s********* \n", TOOL_VERSION );

    if( argc == 1 )
    {
        ShowHelp( argv[0] );
        exit( EXIT_SUCCESS );
    }

    packer = &pack_ascii6;
    //packerascii = &pack_ascii8_length;

    while( ( c = getopt( argc, argv, options ) ) != -1 )
    {
        switch( c )
        {
            case 'r':
                fprintf( stderr, "\nError! Option not implemented\n\n" );
                exit( EXIT_FAILURE );
            case 'i':
                fprintf( stderr, "\nError! Option not implemented\n\n" );
                exit( EXIT_FAILURE );
            case 's':
                result = sscanf( optarg, "%d", &max_size );
                if( result == 0 || result == EOF )
                {
                    fprintf( stderr, "\nError! Invalid maximum file size (-s %s)\n\n",
                             optarg );
                    exit( EXIT_FAILURE );
                }
                break;
            case 'c':
                fru_ini_file = optarg;
                break;
            case 'o':
                outfile = optarg;
                break;
            case 'a':
                //packer = &pack_ascii8;
                packerascii = &pack_ascii8_length;
                break;

            case 'v':
                fprintf( stdout, "\nipmi-fru-it version %s\n\n", TOOL_VERSION );
                return 0;
            default:
                fprintf( stdout, "\nipmi-fru-it version %s\n", TOOL_VERSION );
                fprintf( stdout, usage, argv[0] );
                return -1;
        }
    }

    if( !fru_ini_file || !outfile )
    {
        fprintf( stderr, usage, argv[0] );
        exit( EXIT_FAILURE );
    }

    ini = iniparser_load( fru_ini_file );
    if( !ini )
    {
        fprintf( stderr, "\nError parsing INI file %s!\n\n", fru_ini_file );
        exit( EXIT_FAILURE );
    }

    length = gen_fru_data( ini, &data );

    if( length < 0 )
    {
        fprintf( stderr, "\nError generating FRU data!\n\n" );
        exit( EXIT_FAILURE );
    }

    // only bother checking max_size if the parameter set it
    if( max_size  && ( length > max_size ) )
    {
        fprintf( stderr, "\nError! FRU data length (%d bytes) exceeds maximum "
                 "file size (%d bytes)\n\n", length, max_size );
        exit( EXIT_FAILURE );
    }

    if( write_fru_data( outfile, data, length ) )
    {
        fprintf( stderr, "\nError writing %s\n\n", outfile );
        exit( EXIT_FAILURE );
    }

    iniparser_freedict( ini );

    fprintf( stdout, "\nFRU file \"%s\" created\n\n", outfile );

    return 0;
}
