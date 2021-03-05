#include <inttypes.h>

/*
 * Platform Management FRU Information Storage Definition
 *
 * v1.0
 *
 * Refer: http://www.intel.com/content/dam/www/public/us/en/documents/product-briefs/platform-management-fru-document-rev-1-2-feb-2013.pdf
 *
 */

/* FRU type/length, inspired from Linux kernel's include/linux/ipmi-fru.h */
struct fru_type_length
{
    uint8_t     type_length;
    uint8_t     data[];
};

/* 8. Common Header Format */
struct fru_common_header
{
    uint8_t     format_version;
    uint8_t     internal_use_offset;
    uint8_t     chassis_info_offset;
    uint8_t     board_info_offset;
    uint8_t     product_info_offset;
    uint8_t     multirecord_info_offset;
    uint8_t     pad;
    uint8_t     checksum;
};

/* 9. Internal Use Area Format */
struct internal_use_area
{
    uint8_t     format_version;
    uint8_t     area_length;
    uint8_t     reserved[36];
    uint8_t     end;
    uint8_t     checksum;
};

/* 10. Chassis Info Area Format
 * tl - Type/Length
 */
struct __attribute__( ( __packed__ ) ) chassis_info_area
{
    uint8_t     format_version;
    uint8_t     area_length;
    uint8_t     chassis_type;
    struct fru_type_length tl[];    //Chassis Part Number
    //struct fru_type_length t2[];  //Chassis Serial Number
    //struct fru_type_length t3[];  //Chassis Name
    //struct fru_type_length t4[];  //Chassis SKU ID
    //struct fru_type_length t5[];  //Chassis Manufacturer
    //struct fru_type_length t6[];  //Chassis Version
    //struct fru_type_length t7[];  //Chassis Asset Tag
};

/* 11. Board Info Area Format */
struct __attribute__( ( __packed__ ) ) board_info_area
{
    uint8_t     format_version;
    uint8_t     area_length;
    uint8_t     language_code;
    uint8_t     mfg_date[3];
    struct fru_type_length tl[];    //Board Manufacturer
    //struct fru_type_length t2[];  //Board Product Name
    //struct fru_type_length t3[];  //Board Serial Number
    //struct fru_type_length t4[];  //Board Part Number
    //struct fru_type_length t5[];  //FRU ID File
    //struct fru_type_length t6[];  //Board Version
    //struct fru_type_length t7[];  //Board Asset Tag

};

/* 12. Product Info Area Format */
struct __attribute__( ( __packed__ ) ) product_info_area
{
    uint8_t     format_version;
    uint8_t     area_length;
    uint8_t     language_code;
    struct fru_type_length tl[];    //Manufacturer Name
    //struct fru_type_length t2[];  //Product Name
    //struct fru_type_length t3[];  //Product Part/Model
    //struct fru_type_length t4[];  //Product Version
    //struct fru_type_length t5[];  //Product Serial Number
    //struct fru_type_length t6[];  //Asset Tag
    //struct fru_type_length t7[];  //FRU ID File
    //struct fru_type_length t6[];  //Product Family
    //struct fru_type_length t7[];  //Product SKU ID
};

/* 13. MultiRecord Info Area - Common Record Header */
struct __attribute__( ( __packed__ ) ) multi_record_header
{
    uint8_t     type_id;
    uint8_t     format_version;
    uint8_t     record_length;
    uint8_t     record_checksum;
    uint8_t     header_checksum;
} multi_record_header;

#define UUID_BYTE_LENGTH     16
#define UUID_STR_LENGTH      49

/* 13. MultiRecord Info Area - Management Access Record Format */
struct __attribute__( ( __packed__ ) ) management_access_record
{
    struct multi_record_header record_header;
    uint8_t     sub_record_type;
    uint8_t     record_data[UUID_BYTE_LENGTH];
    uint8_t     pad[2];
};

/* 14. MultiRecord Info Area - OEM VPD Version Format */
struct __attribute__( ( __packed__ ) ) oem_vpd_version
{
    struct multi_record_header record_header;
    uint8_t     major_version;
    uint8_t     minor_version;
    uint8_t     pad[1];
};

#define MAC_ADDRESS_STR_LENGTH    12
#define MAC_ADDRESS_BYTE_LENGTH   6


/* 15. MultiRecord Info Area - MAC Address Format */
struct __attribute__( ( __packed__ ) ) mac_address
{
    struct multi_record_header record_header;
    uint8_t     host_mac_address_count;
    uint8_t     host_base_mac_address[MAC_ADDRESS_BYTE_LENGTH];
    uint8_t     bmc_mac_address_count;
    uint8_t     bmc_base_mac_address[MAC_ADDRESS_BYTE_LENGTH];
    uint16_t    switch_mac_address_count;
    uint8_t     switch_base_mac_address[MAC_ADDRESS_BYTE_LENGTH];
    uint8_t     pad[5];
};

/* 16. MultiRecord Info Area - Fan Speed Control Parameter Format */
struct __attribute__( ( __packed__ ) ) fan_speed_control_parameter
{
    struct multi_record_header record_header;
    uint16_t    max_fan_speed;
    uint8_t     fan_airflow;
    uint8_t     pad[0];
};

#define CPU_VENDOR_ID_STR_LENGTH 16
#define CPU_FAMILY_STR_LENGTH    16
#define CPU_TYPE_STR_LENGTH      16

/* 17. MultiRecord Info Area - Board Controller Info Format */
struct __attribute__( ( __packed__ ) ) board_controller_info
{
    struct multi_record_header record_header;
    uint8_t     vendor_id[CPU_VENDOR_ID_STR_LENGTH];
    uint8_t     family[CPU_FAMILY_STR_LENGTH];
    uint8_t     type[CPU_TYPE_STR_LENGTH];
    uint8_t     pad[3];
};

/* 18. MultiRecord Info Area - System Configuration Format */
struct __attribute__( ( __packed__ ) ) system_configuration
{
    struct multi_record_header record_header;
    uint32_t    customer_id;
    uint8_t     pad[7];
};

/* 19. MultiRecord Info Area - Management Access Record Format */
struct __attribute__( ( __packed__ ) ) multirecord_int_record
{
    struct multi_record_header record_header;
    uint8_t     record_data[];
};

enum fru_multi_record_id
{
    MULTI_RECORD_ID_MAR  = 0x03,
    MULTI_RECORD_ID_VER  = 0xC0,
    MULTI_RECORD_ID_MAC  = 0xC1,
    MULTI_RECORD_ID_FAN  = 0xC2,
    MULTI_RECORD_ID_BCI  = 0xC3,
    MULTI_RECORD_ID_SC   = 0xC4,
};

/* Type code */
enum fru_type_code
{
    TYPE_CODE_BINARY    = 0x00,
    TYPE_CODE_BCDPLUS   = 0x40,
    TYPE_CODE_ASCII6    = 0x80,
    TYPE_CODE_UNILATIN  = 0xc0,
};
