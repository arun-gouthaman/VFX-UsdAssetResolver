#include "debugCodes.h"

#include <pxr/base/tf/registryManager.h>

PXR_NAMESPACE_OPEN_SCOPE

TF_REGISTRY_FUNCTION(TfDebug)
{
    TF_DEBUG_ENVIRONMENT_SYMBOL(FILERESOLVER_RESOLVER, "Print debug output during path resolution");
    TF_DEBUG_ENVIRONMENT_SYMBOL(FILERESOLVER_RESOLVER_CONTEXT, "Print debug output during path resolution context");
    TF_DEBUG_ENVIRONMENT_SYMBOL(FILERESOLVER_TEST_DEBUG, "Test Debug");
}

PXR_NAMESPACE_CLOSE_SCOPE
