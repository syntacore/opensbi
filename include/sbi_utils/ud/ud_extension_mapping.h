#ifndef __UD_EXTENSION_MAPPING_H__
#define __UD_EXTENSION_MAPPING_H__

#include <DiscoveryData.h>

struct ud_extension_mapping_entry {
    char* name;
    size_t offset;
};

struct ud_extension_mapping {
    size_t size;
    struct ud_extension_mapping_entry *array;
};

struct ud_extension_mapping get_unified_discovery_global_extension_mapping();

#endif
