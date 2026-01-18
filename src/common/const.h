#pragma once

#define DEFAULT_RPAK_NAME "new"

#define PF_KEEP_DEV 1 << 0 // whether or not to keep debugging information
#define PF_KEEP_SERVER 1 << 1 // whether or not to keep server only data
#define PF_KEEP_CLIENT 1 << 2 // whether or not to keep client only data

#define PAK_HEADER_SIZE_V8 0x80
#define PAK_HEADER_SIZE_V6 0x58
