/* mk_html.h — CommonMark-compatible HTML serializer for mk_parser events
 *
 * Usage:
 *   MkHtmlState *s = mk_html_new();
 *   MkCallbacks  cb = mk_html_callbacks(s);
 *   // ... mk_feed / mk_finish ...
 *   const char *html = mk_html_result(s, &len);
 *   mk_html_free(s);
 */
#ifndef MK_HTML_H
#define MK_HTML_H

#include "../../include/mk_parser.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MkHtmlState MkHtmlState;

/** Allocate a new HTML serializer state. */
MkHtmlState    *mk_html_new(void);

/** Free the serializer state (does NOT free the parser). */
void            mk_html_free(MkHtmlState *s);

/** Get the MkCallbacks struct that feeds events into this serializer. */
MkCallbacks     mk_html_callbacks(MkHtmlState *s);

/** Return the accumulated HTML (NUL-terminated, valid until mk_html_free). */
const char     *mk_html_result(MkHtmlState *s, size_t *out_len);

/** Reset the state so the same MkHtmlState can be reused for another parse. */
void            mk_html_reset(MkHtmlState *s);

/** Convenience: parse markdown → HTML in one call.
 *  Returns heap-allocated NUL-terminated HTML; caller must free(). */
char           *mk_html_parse(const char *markdown, size_t md_len,
                              size_t *out_len);

#ifdef __cplusplus
}
#endif
#endif /* MK_HTML_H */
