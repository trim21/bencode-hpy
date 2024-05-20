#include "hpy.h"
#include "version.h"

extern HPyDef decode;
extern HPyDef encode;

HPyDef_SLOT(bencode_exec, HPy_mod_exec) static int bencode_exec_impl(HPyContext *ctx, HPy module)
{
    HPy version_string;

    version_string = HPyUnicode_FromString(ctx, BENCODE_VERSION);
    if (HPy_IsNull(version_string))
    {
        return 1;
    }
    HPy_SetAttr_s(ctx, module, "__version__", version_string);
    return 0;
}

static HPyDef *module_defines[] = {&bencode_exec, &decode, &encode, NULL};

static HPyModuleDef moduledef = {
    .defines = module_defines,
};

HPy_MODINIT(_bencode, moduledef)
