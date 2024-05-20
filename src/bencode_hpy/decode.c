#include "hpy.h"

static HPy decode_any(HPyContext *ctx, const char *buf, HPy_ssize_t index)
{
    return ctx->h_None;
}

HPyDef_METH(bdecode, "bdecode", HPyFunc_O) static HPy bdecode_impl(HPyContext *ctx, HPy self, HPy b)
{
    // self is the module object
    if (!HPyBytes_Check(ctx, b))
    {
        HPyErr_SetString(ctx, ctx->h_TypeError, "argument must be bytes");
        return HPy_NULL;
    }

    HPy_ssize_t size = HPyBytes_Size(ctx, b);
    const char *buf = HPyBytes_AsString(ctx, b);

    HPy_Close(ctx, b);

    return decode_any(ctx, buf, size);
};
