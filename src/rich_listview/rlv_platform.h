#ifndef RLV_PLATFORM_H
#define RLV_PLATFORM_H

/**
 * Amiga-only target policy for RichListview.
 *
 * Included by the public draw header so applications and library units
 * fail early on non-Amiga toolchains. Not an application-facing API of
 * its own — prefer rich_listview.h / backends/*.h.
 *
 * Distinct from rlv_platform_internal.h (allocation shim; private).
 *
 * OS Kickstart/library versions are not inferred here. Prefer runtime
 * library-version checks where behaviour depends on OS revision.
 */

#if defined(RLV_PLATFORM_AMIGA) || defined(__AMIGA__)

#ifndef RLV_PLATFORM_AMIGA
#define RLV_PLATFORM_AMIGA 1
#endif

#ifndef RLV_TARGET_AMIGAOS
#define RLV_TARGET_AMIGAOS 1
#endif

#else

#error "RichListview targets AmigaOS and requires an Amiga compiler/SDK (define RLV_PLATFORM_AMIGA or __AMIGA__)"

#endif

#endif /* RLV_PLATFORM_H */

