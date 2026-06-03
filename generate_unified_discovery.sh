#! /bin/bash
COMMON_GENERATED_DIR=./lib/utils/ud/generated
rm ${COMMON_GENERATED_DIR}/*
${ASN1C_PATH} \
    -no-gen-example \
    -fno-constraints \
    -no-gen-XER \
    -no-gen-OER \
    -no-gen-APER \
    -no-gen-JER \
    -no-gen-CBOR \
    -no-gen-print \
    -no-gen-random-fill \
    ${ASN1_SCHEMA} -D ${COMMON_GENERATED_DIR}

SYSTEM_FILE=${COMMON_GENERATED_DIR}/asn_system.h
sed -i '/^#include <netinet\/in.h>.*/d' ${SYSTEM_FILE}
sed -i '/#include <inttypes.h>/a #include <sbi/sbi_types.h>'  $SYSTEM_FILE

INTERNAL_FILE=${COMMON_GENERATED_DIR}/asn_internal.h
sed -i '/#include "asn_application.h"/a #include <sbi_utils/ud/basic_allocator.h>'  $INTERNAL_FILE
sed -i '/#include "asn_application.h"/a #include <sbi/sbi_console.h>'  $INTERNAL_FILE
sed -i 's|#define[[:space:]]\+CALLOC(nmemb, size)[[:space:]]\+calloc(nmemb, size)|#define	CALLOC(nmemb, size)	basic_calloc(nmemb, size)|' $INTERNAL_FILE
sed -i 's|#define[[:space:]]\+MALLOC(size)[[:space:]]\+malloc(size)|#define	MALLOC(size)	basic_malloc(size)|' $INTERNAL_FILE
sed -i 's|#define[[:space:]]\+REALLOC(oldptr, size)[[:space:]]\+realloc(oldptr, size)|#define	REALLOC(oldptr, size)	basic_realloc(oldptr, size)|' $INTERNAL_FILE
sed -i 's|#define[[:space:]]\+FREEMEM(ptr)[[:space:]]\+free(ptr)|#define	FREEMEM(ptr)	basic_free(ptr)|' $INTERNAL_FILE

sed -i 's|^#include[[:space:]]\+<assert\.h>.*|#define assert(cond)|'  $INTERNAL_FILE

sed -i 's|^#include <errno.h>.*|#include <sbi_utils/ud/errno.h>|'  $COMMON_GENERATED_DIR/*

sed -i '/#define	FREEMEM(ptr).*/a #define snprintf'  $INTERNAL_FILE


