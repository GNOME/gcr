/*** BEGIN file-header ***/

#include <glib-object.h>

G_BEGIN_DECLS
/*** END file-header ***/

/*** BEGIN file-production ***/

/* enumerations from "@filename@" */
/*** END file-production ***/

/*** BEGIN value-header ***/
#ifndef @ENUMPREFIX@_TYPE_@ENUMSHORT@
GType @enum_name@_get_type (void) G_GNUC_CONST;
#define @ENUMPREFIX@_TYPE_@ENUMSHORT@ (@enum_name@_get_type ())
#endif

/*** END value-header ***/

/*** BEGIN file-tail ***/
G_END_DECLS

/*** END file-tail ***/
