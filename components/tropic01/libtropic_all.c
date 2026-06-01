// libtropic_all.c — Unity build wrapper for libtropic C library.
// ESPHome copies this to the build tree. Includes all libtropic C sources
// so they compile as C (avoiding C++ strict type-checking issues with
// enum/int conversions in the library code).
//
// Include paths resolved via -I flags set in __init__.py:
//   -I<component>/src/libtropic/include
//   -I<component>/src/libtropic/src
//   -I<component>/src/libtropic/cal/mbedtls_v4

// === libtropic library C sources ===
#include "libtropic.c"
#include "libtropic_default_sh0_keys.c"
#include "libtropic_l2.c"
#include "libtropic_l3.c"
#include "lt_asn1_der.c"
#include "lt_crc16.c"
#include "lt_hkdf.c"
#include "lt_l1.c"
#include "lt_l1_port_wrap.c"
#include "lt_l2_frame_check.c"
#include "lt_l3_process.c"
#include "lt_random.c"
#include "lt_secure_memzero.c"
#include "lt_tr01_attrs.c"

// === Crypto Abstraction Layer (mbedTLS v4) ===
#include "lt_mbedtls_v4_aesgcm.c"
#include "lt_mbedtls_v4_common.c"
#include "lt_mbedtls_v4_hmac_sha256.c"
#include "lt_mbedtls_v4_sha256.c"
#include "lt_mbedtls_v4_x25519.c"
