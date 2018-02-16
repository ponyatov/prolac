/* Process this file with autoheader to produce config.h.in */
#ifndef CONFIG_H
#define CONFIG_H

/* Package and version. */
#define PACKAGE "prolac"
#define VERSION "97"

/* Define if you have the strerror function. */
#undef HAVE_STRERROR 

/* Define to 1 since we have PermStrings. */
#define HAVE_PERMSTRING 1

/* Define if have access to Linux kernel sources. */
#define HAVE_LINUX 1

@TOP@
@BOTTOM@

#ifdef __cplusplus
/* Get rid of a possible inline macro under C++. */
# undef inline
#endif
#endif
