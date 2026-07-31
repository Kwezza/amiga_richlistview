#ifndef CLV_PLATFORM_H
#define CLV_PLATFORM_H

/**
 * Amiga-only target policy for the Custom ListView C library.
 *
 * The library targets classic AmigaOS with an Amiga SDK, Amiga data model,
 * and Amiga ABI. Host-native (Windows/Unix) library builds are unsupported.
 * Host scripts may still validate, package, or analyse Amiga artefacts.
 *
 * Distinct from clv_platform_internal.h (internal allocation shim).
 *
 * OS Kickstart/library versions are not inferred here. Prefer runtime
 * library-version checks where behaviour depends on OS revision.
 */

#if defined(CLV_PLATFORM_AMIGA) || defined(__AMIGA__)

#ifndef CLV_PLATFORM_AMIGA
#define CLV_PLATFORM_AMIGA 1
#endif

#ifndef CLV_TARGET_AMIGAOS
#define CLV_TARGET_AMIGAOS 1
#endif

#else

#error "Custom ListView targets AmigaOS and requires an Amiga compiler/SDK (define CLV_PLATFORM_AMIGA or __AMIGA__)"

#endif

#endif /* CLV_PLATFORM_H */

