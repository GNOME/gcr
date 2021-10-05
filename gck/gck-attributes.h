/*
 * Copyright 2008, Stefan Walter <nielsen@memberwebs.com>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef __GCK_ATTRIBUTES_H__
#define __GCK_ATTRIBUTES_H__

#if !defined (__GCK_INSIDE_HEADER__) && !defined (GCK_COMPILATION)
#error "Only <gck/gck.h> can be included directly."
#endif

#include <glib-object.h>
#include <p11-kit/p11-kit.h>

G_BEGIN_DECLS


#define             GCK_VENDOR_CODE                         0x47434B00 /* GCK */

/* An error code which results from a failure to load the PKCS11 module */
typedef enum {
	GCK_ERROR_MODULE_PROBLEM = (CKR_VENDOR_DEFINED | (GCK_VENDOR_CODE + 1)),
} GckError;

/**
 * GCK_ERROR:
 *
 * The error domain for gck library errors.
 */
#define             GCK_ERROR                               (gck_error_quark ())

GQuark              gck_error_quark                         (void) G_GNUC_CONST;

const gchar*        gck_message_from_rv                     (gulong rv);

gboolean            gck_string_to_chars                     (guchar *data,
                                                             gsize max,
                                                             const gchar *string);

gchar*              gck_string_from_chars                   (const guchar *data,
                                                             gsize max);

typedef gpointer    (*GckAllocator)                         (gpointer data, gsize length);

typedef enum {
	GCK_SESSION_READ_ONLY = 0,
	GCK_SESSION_READ_WRITE = 1 << 1,
	GCK_SESSION_LOGIN_USER =  1 << 2,
	GCK_SESSION_AUTHENTICATE = 1 << 3,
} GckSessionOptions;

typedef struct _GckMechanism GckMechanism;

struct _GckMechanism {
	gulong type;
	gconstpointer parameter;
	gulong n_parameter;
};

typedef struct _GckAttribute GckAttribute;

struct _GckAttribute {
	gulong type;
	guchar *value;
	gulong length;
};

#define GCK_INVALID ((gulong)-1)

gboolean            gck_value_to_ulong                      (const guchar *value,
                                                             gsize length,
                                                             gulong *result);

gboolean            gck_value_to_boolean                    (const guchar *value,
                                                             gsize length,
                                                             gboolean *result);

void                gck_attribute_init                      (GckAttribute *attr,
                                                             gulong attr_type,
                                                             const guchar *value,
                                                             gsize length);

void                gck_attribute_init_invalid              (GckAttribute *attr,
                                                             gulong attr_type);

void                gck_attribute_init_empty                (GckAttribute *attr,
                                                             gulong attr_type);

void                gck_attribute_init_boolean              (GckAttribute *attr,
                                                             gulong attr_type,
                                                             gboolean value);

void                gck_attribute_init_date                 (GckAttribute *attr,
                                                             gulong attr_type,
                                                             const GDate *value);

void                gck_attribute_init_ulong                (GckAttribute *attr,
                                                             gulong attr_type,
                                                             gulong value);

void                gck_attribute_init_string               (GckAttribute *attr,
                                                             gulong attr_type,
                                                             const gchar *value);

void                gck_attribute_init_copy                 (GckAttribute *dest,
                                                             const GckAttribute *src);

#define             GCK_TYPE_ATTRIBUTE                      (gck_attribute_get_type ())

GType               gck_attribute_get_type                  (void) G_GNUC_CONST;

GckAttribute*       gck_attribute_new                       (gulong attr_type,
                                                             const guchar *value,
                                                             gsize length);

GckAttribute*       gck_attribute_new_invalid               (gulong attr_type);

GckAttribute*       gck_attribute_new_empty                 (gulong attr_type);

GckAttribute*       gck_attribute_new_boolean               (gulong attr_type,
                                                             gboolean value);

GckAttribute*       gck_attribute_new_date                  (gulong attr_type,
                                                             const GDate *value);

GckAttribute*       gck_attribute_new_ulong                 (gulong attr_type,
                                                             gulong value);

GckAttribute*       gck_attribute_new_string                (gulong attr_type,
                                                             const gchar *value);

gboolean            gck_attribute_is_invalid                (const GckAttribute *attr);

gboolean            gck_attribute_get_boolean               (const GckAttribute *attr);

gulong              gck_attribute_get_ulong                 (const GckAttribute *attr);

gchar*              gck_attribute_get_string                (const GckAttribute *attr);

void                gck_attribute_get_date                  (const GckAttribute *attr,
                                                             GDate* value);

const guchar *      gck_attribute_get_data                  (const GckAttribute *attr,
                                                             gsize *length);

gboolean            gck_attribute_equal                     (gconstpointer attr1,
                                                             gconstpointer attr2);

guint               gck_attribute_hash                      (gconstpointer attr);

GckAttribute*       gck_attribute_dup                       (const GckAttribute *attr);

void                gck_attribute_clear                     (GckAttribute *attr);

void                gck_attribute_free                      (gpointer attr);

void                gck_attribute_dump                      (const GckAttribute *attr);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GckAttribute, gck_attribute_free);

typedef struct _GckBuilder GckBuilder;

struct _GckBuilder {
	/*< private >*/
	gsize x[16];
};

typedef enum {
	GCK_BUILDER_NONE,
	GCK_BUILDER_SECURE_MEMORY = 1,
} GckBuilderFlags;

typedef struct _GckAttributes GckAttributes;

GckBuilder *         gck_builder_new                        (GckBuilderFlags flags);

#define              GCK_BUILDER_INIT                       { { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 } }

GckBuilder *         gck_builder_ref                        (GckBuilder *builder);

void                 gck_builder_unref                      (GckBuilder *builder);

void                 gck_builder_init                       (GckBuilder *builder);

void                 gck_builder_init_full                  (GckBuilder *builder,
                                                             GckBuilderFlags flags);

#define              GCK_TYPE_BUILDER                       (gck_builder_get_type ())

GType                gck_builder_get_type                   (void) G_GNUC_CONST;

void                 gck_builder_take_data                  (GckBuilder *builder,
                                                             gulong attr_type,
                                                             guchar *value,
                                                             gsize length);

void                 gck_builder_add_data                   (GckBuilder *builder,
                                                             gulong attr_type,
                                                             const guchar *value,
                                                             gsize length);

void                 gck_builder_add_empty                  (GckBuilder *builder,
                                                             gulong attr_type);

void                 gck_builder_add_invalid                (GckBuilder *builder,
                                                             gulong attr_type);

void                 gck_builder_add_ulong                  (GckBuilder *builder,
                                                             gulong attr_type,
                                                             gulong value);

void                 gck_builder_add_boolean                (GckBuilder *builder,
                                                             gulong attr_type,
                                                             gboolean value);

void                 gck_builder_add_date                   (GckBuilder *builder,
                                                             gulong attr_type,
                                                             const GDate *value);

void                 gck_builder_add_string                 (GckBuilder *builder,
                                                             gulong attr_type,
                                                             const gchar *value);

void                 gck_builder_add_attribute              (GckBuilder *builder,
                                                             const GckAttribute *attr);

void                 gck_builder_add_all                    (GckBuilder *builder,
                                                             GckAttributes *attrs);

void                 gck_builder_add_only                   (GckBuilder *builder,
                                                             GckAttributes *attrs,
                                                             gulong only_type,
                                                             ...);

void                 gck_builder_add_onlyv                  (GckBuilder *builder,
                                                             GckAttributes *attrs,
                                                             const gulong *only_types,
                                                             guint n_only_types);

void                 gck_builder_add_except                 (GckBuilder *builder,
                                                             GckAttributes *attrs,
                                                             gulong except_type,
                                                             ...);

void                 gck_builder_add_exceptv                (GckBuilder *builder,
                                                             GckAttributes *attrs,
                                                             const gulong *except_types,
                                                             guint n_except_types);

void                 gck_builder_set_data                   (GckBuilder *builder,
                                                             gulong attr_type,
                                                             const guchar *value,
                                                             gsize length);

void                 gck_builder_set_empty                  (GckBuilder *builder,
                                                             gulong attr_type);

void                 gck_builder_set_invalid                (GckBuilder *builder,
                                                             gulong attr_type);

void                 gck_builder_set_ulong                  (GckBuilder *builder,
                                                             gulong attr_type,
                                                             gulong value);

void                 gck_builder_set_boolean                (GckBuilder *builder,
                                                             gulong attr_type,
                                                             gboolean value);

void                 gck_builder_set_date                   (GckBuilder *builder,
                                                             gulong attr_type,
                                                             const GDate *value);

void                 gck_builder_set_string                 (GckBuilder *builder,
                                                             gulong attr_type,
                                                             const gchar *value);

void                 gck_builder_set_all                    (GckBuilder *builder,
                                                             GckAttributes *attrs);

const GckAttribute * gck_builder_find                       (GckBuilder *builder,
                                                             gulong attr_type);

gboolean             gck_builder_find_boolean               (GckBuilder *builder,
                                                             gulong attr_type,
                                                             gboolean *value);

gboolean             gck_builder_find_ulong                 (GckBuilder *builder,
                                                             gulong attr_type,
                                                             gulong *value);

gboolean             gck_builder_find_string                (GckBuilder *builder,
                                                             gulong attr_type,
                                                             gchar **value);

gboolean             gck_builder_find_date                  (GckBuilder *builder,
                                                             gulong attr_type,
                                                             GDate *value);

GckAttributes *      gck_builder_steal                      (GckBuilder *builder);

GckAttributes *      gck_builder_end                        (GckBuilder *builder);

GckBuilder *         gck_builder_copy                       (GckBuilder *builder);

void                 gck_builder_clear                      (GckBuilder *builder);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GckBuilder, gck_builder_unref);

#define              GCK_TYPE_ATTRIBUTES                    (gck_attributes_get_type ())

GType                gck_attributes_get_type                (void) G_GNUC_CONST;

GckAttributes *      gck_attributes_new                     (gulong reserved);

GckAttributes *      gck_attributes_new_empty               (gulong first_type,
                                                             ...);

const GckAttribute * gck_attributes_at                      (GckAttributes *attrs,
                                                             guint index);

const GckAttribute * gck_attributes_find                    (GckAttributes *attrs,
                                                             gulong attr_type);

gboolean             gck_attributes_find_boolean            (GckAttributes *attrs,
                                                             gulong attr_type,
                                                             gboolean *value);

gboolean             gck_attributes_find_ulong              (GckAttributes *attrs,
                                                             gulong attr_type,
                                                             gulong *value);

gboolean             gck_attributes_find_string             (GckAttributes *attrs,
                                                             gulong attr_type,
                                                             gchar **value);

gboolean             gck_attributes_find_date               (GckAttributes *attrs,
                                                             gulong attr_type,
                                                             GDate *value);

gulong               gck_attributes_count                   (GckAttributes *attrs);

GckAttributes *      gck_attributes_ref                     (GckAttributes *attrs);

GckAttributes *      gck_attributes_ref_sink                (GckAttributes *attrs);

void                 gck_attributes_unref                   (gpointer attrs);

gboolean             gck_attributes_contains                (GckAttributes *attrs,
                                                             const GckAttribute *match);

void                 gck_attributes_dump                    (GckAttributes *attrs);

gchar *              gck_attributes_to_string               (GckAttributes *attrs);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GckAttributes, gck_attributes_unref);

G_END_DECLS

#endif /* __GCK_ATTRIBUTES_H__ */
