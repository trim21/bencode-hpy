#include <stdio.h>

#include "hpy.h"

extern HPyDef bdecode;
extern HPyDef bencode;

extern HPyGlobal BencodeDecodeError;
extern HPyGlobal BencodeEncodeError;

static bool init_exception(HPyContext *ctx, HPy mod, HPyGlobal *global, const char *name, const char *attr_name)
{
    HPy h = HPyErr_NewException(ctx, name, HPy_NULL, HPy_NULL);
    if (HPy_IsNull(h))
    {
        return false;
    }
    HPyGlobal_Store(ctx, global, h);
    int set_attr_failed = HPy_SetAttr_s(ctx, mod, attr_name, h);
    HPy_Close(ctx, h);
    return !set_attr_failed;
}

bool init_exceptions(HPyContext *ctx, HPy mod)
{
    init_exception(ctx, mod, &BencodeDecodeError, "bencode_hpy.BencodeDecodeError", "BencodeDecodeError");
    return init_exception(ctx, mod, &BencodeEncodeError, "bencode_hpy.BencodeEncodeError", "BencodeEncodeError");
    // return true;
}

HPyDef_SLOT(bencode_exec, HPy_mod_exec) static int bencode_exec_impl(HPyContext *ctx, HPy module)
{
    if (!init_exceptions(ctx, module))
    {
        HPyErr_SetString(ctx, ctx->h_RuntimeError, "failed to init exceptions");
        return 1;
    }
    return 0;
}

static HPyDef *module_defines[] = {&bencode_exec, &bdecode, &bencode, NULL};

static HPyModuleDef moduledef = {
    .defines = module_defines,
};

HPy_MODINIT(_bencode, moduledef)
