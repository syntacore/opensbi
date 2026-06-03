#include <sbi_utils/ud/ud_helper.h>

#include <sbi_utils/ud/ud_extension_mapping.h>

#include <sbi/riscv_asm.h>
#include <sbi/sbi_error.h>
#include <sbi/sbi_heap.h>
#include <sbi/sbi_string.h>

#include "generated/DiscoveryData.h"

static DiscoveryData_t *unified_discovery_data = NULL;

int init_unified_discovery() {
	if (unified_discovery_data != NULL) {
		return SBI_OK;
	}
	unsigned long blob_addr = csr_read(CSR_MCONFIGPTR);
	if (!blob_addr) {
		return SBI_ENOTSUPP;
	}
	asn_codec_ctx_t codec = {0};
	basic_allocator_init();
	asn_dec_rval_t discovery_data_rval = asn_decode(
		&codec,
		ATS_DER,
		&asn_DEF_DiscoveryData,
		(void*) &unified_discovery_data,
		(void*) blob_addr,
		256
	);

	if (discovery_data_rval.code != RC_OK || discovery_data_rval.consumed == 0) {
		return SBI_ENOTSUPP;
	}
	return SBI_OK;
}

int get_unified_discovery_ext_num(size_t *ret) {
	if (unified_discovery_data == NULL) {
		return SBI_EINVALID_STATE;
	}
	*ret = unified_discovery_data->ext->list.count;
	return SBI_OK;
}

int get_unified_discovery_ext_by_index(DiscoveryAdditonalData_t **ret, size_t index) {
	if (unified_discovery_data == NULL) {
		return SBI_EINVALID_STATE;
	}
	if (index >= unified_discovery_data->ext->list.count) {
		return SBI_EINVAL;
	}
	*ret = unified_discovery_data->ext->list.array[index];
	return SBI_OK;
}

int get_unified_discovery_ext_by_url(DiscoveryAdditonalData_t **ret, const char* url) {
	*ret = NULL;
	for (size_t ext_index = 0; ext_index < unified_discovery_data->ext->list.count; ++ext_index) {
		DiscoveryAdditonalData_t* ext = unified_discovery_data->ext->list.array[ext_index];
		if (ext->contentType.present != contentType_PR_url) {
			continue;
		}   
		if (sbi_strcmp((const char*) ext->contentType.choice.url.buf, url) == 0) {
			*ret = ext;
			break;
		}
	}
	if (ret == NULL) {
		return SBI_EINVAL;
	}
	return SBI_OK;
}

bool unified_discovery_check_extension_present(struct ud_extension_mapping_entry entry) {
	void **ext_ptr = (void **)((char *)(unified_discovery_data) + entry.offset);
	return *ext_ptr != NULL;
}

int unified_discovery_get_extensions_str(char** res, size_t* len) {
	if (res == NULL || len == NULL) {
		return SBI_EINVAL;
	}
	struct ud_extension_mapping mapping = get_unified_discovery_global_extension_mapping();
	size_t *bytes_to_take = sbi_malloc(mapping.size * sizeof(size_t));
	if (!bytes_to_take) {
		return SBI_ENOMEM;
	}
	size_t res_string_size = 2; // initial size to write "i" support
	for (size_t i = 0; i < mapping.size; ++i) {
		struct ud_extension_mapping_entry entry = mapping.array[i];
		volatile bool present = unified_discovery_check_extension_present(entry);
		if (present) {
			const size_t extension_name_len = sbi_strlen(entry.name); 
			res_string_size += extension_name_len + 1;
			bytes_to_take[i] = extension_name_len + 1;
		} else {
			bytes_to_take[i] = 0;
		}
	}
	*len = res_string_size;
	*res = sbi_malloc(res_string_size);
	if (!(*res)) {
		sbi_free(bytes_to_take);
		return SBI_ENOMEM;
	}
	char* next_name_ptr = *res;
	next_name_ptr[0] = 'i';
	next_name_ptr[1] = 0;
	next_name_ptr += 2;
	for (size_t i = 0; i < mapping.size; ++i) {
		struct ud_extension_mapping_entry entry = mapping.array[i];
		sbi_memcpy(next_name_ptr, entry.name, bytes_to_take[i]);
		next_name_ptr += bytes_to_take[i];
	}
	sbi_free(bytes_to_take);
	return SBI_OK;
}
