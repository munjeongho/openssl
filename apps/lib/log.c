/*
 * Copyright 2020-2022 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <openssl/trace.h>
#include "apps.h"
#include "log.h"

static int verbosity = LOG_INFO;

int log_set_verbosity(const char *prog, int level)
{
    if (level < LOG_EMERG || level > LOG_TRACE) {
        trace_log_message(-1, prog, LOG_ERR,
                          "Invalid verbosity level %d", level);
        return 0;
    }
    verbosity = level;
    return 1;
}

int log_get_verbosity(void)
{
    return verbosity;
}

#ifdef HTTP_DAEMON
static int print_syslog(const char *str, size_t len, void *levPtr)
{
    int level = *(int *)levPtr;
    int ilen = len > MAXERRLEN ? MAXERRLEN : len;

    syslog(level, "%.*s", ilen, str);

    return ilen;
}
#endif

static void log_with_prefix(const char *prog, const char *fmt, va_list ap)
{
    char prefix[80];
    BIO *bio, *pre = BIO_new(BIO_f_prefix());

    (void)snprintf(prefix, sizeof(prefix), "%s: ", prog);
    (void)BIO_set_prefix(pre, prefix);
    bio = BIO_push(pre, bio_err);
    (void)BIO_vprintf(bio, fmt, ap);
    (void)BIO_printf(bio, "\n");
    (void)BIO_flush(bio);
    (void)BIO_pop(pre);
    BIO_free(pre);
}

void trace_log_message(int category,
                       const char *prog, int level, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
#ifdef __STRICT_ANSI__ /* unfortuantely, ANSI does not define va_copy */
    if (verbosity >= level)
        category = -1; /* disabling trace output in addition to logging */
#endif
    if (category >= 0 && OSSL_trace_enabled(category)) {
        BIO *out = OSSL_trace_begin(category);
#ifndef __STRICT_ANSI__
        va_list ap_copy;

        va_copy(ap_copy, ap);
        (void)BIO_vprintf(out, fmt, ap_copy);
        va_end(ap_copy);
#else
        (void)BIO_vprintf(out, fmt, ap);
#endif
        (void)BIO_printf(out, "\n");
        OSSL_trace_end(category, out);
    }
    if (verbosity < level) {
        va_end(ap);
        return;
    }
#ifdef HTTP_DAEMON
    if (n_responders != 0) {
        vsyslog(level, fmt, ap);
        if (level <= LOG_ERR)
            ERR_print_errors_cb(print_syslog, &level);
    } else
#endif
    log_with_prefix(prog, fmt, ap);
    va_end(ap);
}
