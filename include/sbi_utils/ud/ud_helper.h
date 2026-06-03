#ifndef __UD_HELPER_H__
#define __UD_HELPER_H__

#include <DiscoveryData.h>

int init_unified_discovery();
int get_unified_discovery_ext_num(size_t *ret);
int get_unified_discovery_ext_by_index(DiscoveryAdditonalData_t **ret, size_t index);
int get_unified_discovery_ext_by_url(DiscoveryAdditonalData_t **ret, const char* url);
int unified_discovery_get_extensions_str(char** res, size_t *len);

#endif
