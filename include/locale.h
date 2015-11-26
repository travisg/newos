/*
** Copyright 2004, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#ifndef __newos__libc_locale__hh__
#define __newos__libc_locale__hh__

#include <stddef.h>
#include <sys/types.h>
#include <sys/cdefs.h>

#include <stdarg.h>

#ifdef __cplusplus
namespace std {
extern "C"
{
#endif

struct lconv {
    char    *decimal_point;
    char    *thousands_sep;
    char    *grouping;
    char    *int_curr_symbol;
    char    *currency_symbol;
    char    *mon_decimal_point;
    char    *mon_thousands_sep;
    char    *mon_grouping;
    char    *positive_sign;
    char    *negative_sign;
    char    int_frac_digits;
    char    frac_digits;
    char    p_cs_precedes;
    char    p_sep_by_space;
    char    n_cs_precedes;
    char    n_sep_by_space;
    char    p_sign_posn;
    char    n_sign_posn;
};

#define LC_ALL          0
#define LC_COLLATE      1
#define LC_CTYPE        2
#define LC_MONETARY     3
#define LC_NUMERIC      4
#define LC_TIME         5
#define LC_MESSAGES     6
#define _LC_LAST        7

struct lconv *localeconv(void);
char         *setlocale(int category, const char *locale);

#ifdef __cplusplus
}
}
#endif

#endif

#if defined(__cplusplus) && !defined(_NEWOS_NO_LIBC_COMPAT)
using ::std::localeconv;
using ::std::setlocale;
using ::std::lconv;
#endif
