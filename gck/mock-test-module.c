#include "config.h"

#include "gck-mock.h"
#include <p11-kit/pkcs11.h>

CK_RV
C_GetFunctionList (CK_FUNCTION_LIST_PTR_PTR list)
{
	return gck_mock_C_GetFunctionList (list);
}
