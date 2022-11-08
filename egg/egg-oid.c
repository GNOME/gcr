/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* egg-oid.c - OID helper routines

   Copyright (C) 2007 Stefan Walter

   The Gnome Keyring Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Keyring Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   see <http://www.gnu.org/licenses/>.

   Author: Stef Walter <stef@memberwebs.com>
*/

#include "config.h"

#include "egg-oid.h"

#include <string.h>

#include <glib/gi18n-lib.h>

typedef struct _OidInfo {
	GQuark oid;
	const gchar *oidstr;
	const gchar *attr;
	const gchar *description;
	guint flags;
} OidInfo;

static OidInfo oid_info[] = {
	{ 0, "0.9.2342.19200300.100.1.25", "DC", N_("Domain Component"),
		EGG_OID_PRINTABLE },
	{ 0, "0.9.2342.19200300.100.1.1", "UID", N_("User ID"),
		EGG_OID_PRINTABLE | EGG_OID_IS_CHOICE },

	{ 0, "1.2.840.113549.1.9.1", "EMAIL", N_("Email Address"),
		EGG_OID_PRINTABLE },
	{ 0, "1.2.840.113549.1.9.7", NULL, NULL,
		EGG_OID_PRINTABLE | EGG_OID_IS_CHOICE },

	{ 0, "1.2.840.113549.1.9.20", NULL, NULL,
		EGG_OID_PRINTABLE },

	{ 0, "1.3.6.1.5.5.7.9.1", "dateOfBirth", N_("Date of Birth"),
		EGG_OID_PRINTABLE },
	{ 0, "1.3.6.1.5.5.7.9.2", "placeOfBirth", N_("Place of Birth"),
		EGG_OID_PRINTABLE },
	{ 0, "1.3.6.1.5.5.7.9.3", "gender", N_("Gender"),
		EGG_OID_PRINTABLE },
	{ 0, "1.3.6.1.5.5.7.9.4", "countryOfCitizenship", N_("Country of Citizenship"),
		EGG_OID_PRINTABLE },
	{ 0, "1.3.6.1.5.5.7.9.5", "countryOfResidence", N_("Country of Residence"),
		EGG_OID_PRINTABLE },

	{ 0, "2.5.4.3", "CN", N_("Common Name"),
		EGG_OID_PRINTABLE | EGG_OID_IS_CHOICE },
	{ 0, "2.5.4.4", "surName", N_("Surname"),
		EGG_OID_PRINTABLE | EGG_OID_IS_CHOICE },
	{ 0, "2.5.4.5", "serialNumber", N_("Serial Number"),
		EGG_OID_PRINTABLE },
	{ 0, "2.5.4.6", "C", N_("Country"),
		EGG_OID_PRINTABLE, },
	{ 0, "2.5.4.7", "L", N_("Locality"),
		EGG_OID_PRINTABLE | EGG_OID_IS_CHOICE },
	{ 0, "2.5.4.8", "ST", N_("State"),
		EGG_OID_PRINTABLE | EGG_OID_IS_CHOICE },
	{ 0, "2.5.4.9", "STREET", N_("Street"),
		EGG_OID_PRINTABLE | EGG_OID_IS_CHOICE },
	{ 0, "2.5.4.10", "O", N_("Organization"),
		EGG_OID_PRINTABLE | EGG_OID_IS_CHOICE },
	{ 0, "2.5.4.11", "OU", N_("Organizational Unit"),
		EGG_OID_PRINTABLE | EGG_OID_IS_CHOICE },
	{ 0, "2.5.4.12", "T", N_("Title"),
		EGG_OID_PRINTABLE | EGG_OID_IS_CHOICE },
	{ 0, "2.5.4.20", "telephoneNumber", N_("Telephone Number"),
		EGG_OID_PRINTABLE },
	{ 0, "2.5.4.42", "givenName", N_("Given Name"),
		EGG_OID_PRINTABLE | EGG_OID_IS_CHOICE },
	{ 0, "2.5.4.43", "initials", N_("Initials"),
		EGG_OID_PRINTABLE | EGG_OID_IS_CHOICE },
	{ 0, "2.5.4.44", "generationQualifier", N_("Generation Qualifier"),
		EGG_OID_PRINTABLE | EGG_OID_IS_CHOICE },
	{ 0, "2.5.4.46", "dnQualifier", N_("DN Qualifier"),
		EGG_OID_PRINTABLE },
	{ 0, "2.5.4.65", "pseudonym", N_("Pseudonym"),
		EGG_OID_PRINTABLE | EGG_OID_IS_CHOICE },

	/* Translators: Russian: Main state registration number */
	{ 0, "1.2.643.100.1", "OGRN", N_("OGRN"),
		EGG_OID_PRINTABLE },
	/* Translators: Russian: Individual insurance account number */
	{ 0, "1.2.643.100.3", "SNILS", N_("SNILS"),
		EGG_OID_PRINTABLE },
	/* Translators: Russian: Main state registration number for individual enterpreneurs */
	{ 0, "1.2.643.100.5", "OGRNIP", N_("OGRNIP"),
		EGG_OID_PRINTABLE },
	/* Translators: Russian: Individual taxpayer number */
	{ 0, "1.2.643.3.131.1.1", "INN", N_("INN"),
		EGG_OID_PRINTABLE },

	{ 0, "1.2.840.113549.1.1.1", "rsaEncryption", N_("RSA"), 0 },
	{ 0, "1.2.840.113549.1.1.2", "md2WithRSAEncryption", N_("MD2 with RSA"), 0 },
	{ 0, "1.2.840.113549.1.1.4", "md5WithRSAEncryption", N_("MD5 with RSA"), 0 },
	{ 0, "1.2.840.113549.1.1.5", "sha1WithRSAEncryption", N_("SHA1 with RSA"), 0 },
	{ 0, "1.2.840.113549.1.1.7", "id-RSAES-OAEP", N_("RSA with OAEP padding"), 0 },
	{ 0, "1.2.840.113549.1.1.8", "id-mgf1", N_("RSA with MGF1"), 0 },
	{ 0, "1.2.840.113549.1.1.10", "rsassa-pss", N_("RSA signature with RSA-PSS"), 0 },
	{ 0, "1.2.840.113549.1.1.11", "sha256WithRSAEncryption", N_("SHA256 with RSA encryption"), 0 },
	{ 0, "1.2.840.113549.1.1.12", "sha384WithRSAEncryption", N_("SHA384 with RSA encryption"), 0 },
	{ 0, "1.2.840.113549.1.1.13", "sha512WithRSAEncryption", N_("SHA512 with RSA encryption"), 0 },
	{ 0, "1.2.840.113549.1.1.14", "sha224WithRSAEncryption", N_("SHA224 with RSA encryption"), 0 },

	{ 0, "1.2.840.10040.4.1", "dsa", N_("DSA"), 0 },
	{ 0, "1.2.840.10040.4.3", "sha1WithDSA", N_("SHA1 with DSA"), 0 },

	{ 0, "1.2.840.10045.2.1", "ec", N_("Elliptic Curve"), 0, },
	{ 0, "1.2.840.10045.4.1", "sha1WithECDSA", N_("SHA1 with ECDSA"), 0 },
	{ 0, "1.2.840.10045.4.3.1", "sha224WithECDSA", N_("SHA224 with ECDSA"), 0 },
	{ 0, "1.2.840.10045.4.3.2", "sha256WithECDSA", N_("SHA256 with ECDSA"), 0 },
	{ 0, "1.2.840.10045.4.3.3", "sha384WithECDSA", N_("SHA384 with ECDSA"), 0 },
	{ 0, "1.2.840.10045.4.3.4", "sha512WithECDSA", N_("SHA512 with ECDSA"), 0 },

	{ 0, "1.2.643.2.2.3", "gostR3411-94-with-gostR3410-2001", N_("GOST R 34.11-94 with GOST R 34.10-2001"), 0 },
	{ 0, "1.2.643.2.2.19", "gostr3410-2001", N_("GOST R 34.10-2001"), 0 },
	{ 0, "1.2.643.7.1.1.1.1", "gost-3410-2012-256", N_("GOST R 34.10-2012 256-bit curve"), 0 },
	{ 0, "1.2.643.7.1.1.1.2", "gost-3410-2012-512", N_("GOST R 34.10-2012 512-bit curve"), 0 },
	{ 0, "1.2.643.7.1.1.3.2", "signwithdigest-gost-3410-2012-256", N_("GOST R 34.11-2012/256 with GOST R 34.10-2012 256-bit curve"), 0 },
	{ 0, "1.2.643.7.1.1.3.3", "signwithdigest-gost-3410-2012-512", N_("GOST R 34.11-2012/512 with GOST R 34.10-2012 512-bit curve"), 0 },

	/* Extended Key Usages */
	{ 0, "1.3.6.1.5.5.7.3.1", NULL, N_("Server Authentication"), 0 },
	{ 0, "1.3.6.1.5.5.7.3.2", NULL, N_("Client Authentication"), 0 },
	{ 0, "1.3.6.1.5.5.7.3.3", NULL, N_("Code Signing"), 0 },
	{ 0, "1.3.6.1.5.5.7.3.4", NULL, N_("Email Protection"), 0 },
	{ 0, "1.3.6.1.5.5.7.3.8", NULL, N_("Time Stamping"), 0 },

	/* Extended certificate attributes */
	{ 0, "1.3.6.1.5.5.7.1.1", "authorityInfoAccess", N_("Certificate Authority Information Access"), 0 },
	{ 0, "2.5.29.9", "subjectDirectoryAttributes", N_("Subject directory attributes certificate extension"), 0 },
	{ 0, "2.5.29.14", "subjectKeyIdentifier", N_("Subject key identifier"), 0 },
	{ 0, "2.5.29.15", "keyUsage", N_("Key usage"), 0 },
	{ 0, "2.5.29.16", "privateKeyUsagePeriod", N_("Private key usage period"), 0 },
	{ 0, "2.5.29.17", "subjectAltName", N_("Subject alternative name"), 0 },
	{ 0, "2.5.29.18", "issuerAltName", N_("Issuer alternative name"), 0 },
	{ 0, "2.5.29.19", "basicConstraints", N_("Basic constraints"), 0 },
	{ 0, "2.5.29.20", "cRLNumber", N_("CRL number"), 0 },
	{ 0, "2.5.29.21", "reasonCode", N_("Reason code"), 0 },
	{ 0, "2.5.29.23", "instructionCode", N_("Hold instruction code"), 0 },
	{ 0, "2.5.29.24", "invalidityDate", N_("Invalidity date"), 0 },
	{ 0, "2.5.29.27", "deltaCRLIndicator", N_("Certificate Revocation List indicator"), 0 },
	{ 0, "2.5.29.28", "issuingDistributionPoint", N_("Issuing distribution point"), 0 },
	{ 0, "2.5.29.29", "certificateIssuer", N_("Certificate issuer"), 0 },
	{ 0, "2.5.29.30", "nameConstraints", N_("Name constraints"), 0 },
	{ 0, "2.5.29.31", "cRLDistributionPoints", N_("Certificate Revocation List distribution points"), 0 },
	{ 0, "2.5.29.32", "certificatePolicies", N_("Certificate policies"), 0 },
	{ 0, "2.5.29.33", "policyMappings", N_("Policy mappings"), 0 },
	{ 0, "2.5.29.34", "policyConstraints", N_("Policy constraints"), 0 },
	{ 0, "2.5.29.35", "authorityKeyIdentifier", N_("Authority key identifier"), 0 },

	{ 0, NULL, NULL, NULL, FALSE }
};

static OidInfo*
find_oid_info (GQuark oid)
{
	static size_t inited_oids = 0;
	int i;

	g_return_val_if_fail (oid != 0, NULL);

	/* Initialize first time around */
	if (g_once_init_enter (&inited_oids)) {
		for (i = 0; oid_info[i].oidstr != NULL; ++i)
			oid_info[i].oid = g_quark_from_static_string (oid_info[i].oidstr);
		g_once_init_leave (&inited_oids, 1);
	}

	for (i = 0; oid_info[i].oidstr != NULL; ++i) {
		if (oid_info[i].oid == oid)
			return &oid_info[i];
	}

	return NULL;
}

const gchar*
egg_oid_get_name (GQuark oid)
{
	OidInfo *info;

	g_return_val_if_fail (oid, NULL);

	info = find_oid_info (oid);
	if (info == NULL)
		return g_quark_to_string (oid);

	return info->attr;
}

const gchar*
egg_oid_get_description (GQuark oid)
{
	OidInfo *info;

	g_return_val_if_fail (oid, NULL);

	info = find_oid_info (oid);
	if (info == NULL)
		return g_quark_to_string (oid);

	return _(info->description);
}

guint
egg_oid_get_flags (GQuark oid)
{
	OidInfo *info;

	g_return_val_if_fail (oid, 0);

	info = find_oid_info (oid);
	if (info == NULL)
		return 0;

	return info->flags;
}
