#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "hpy.h"
#include "hpy/hpydef.h"
#include "hpy/hpyfunc.h"
#include "hpy/inline_helpers.h"
#include "hpy/runtime/format.h"

HPyGlobal BencodeDecodeError;

static inline HPy decodeError(HPyContext *ctx, const char *fmt, ...)
{
    return HPyErr_SetObject(ctx, HPyGlobal_Load(ctx, BencodeDecodeError), HPyUnicode_FromFormat(ctx, fmt));
}

// int strcom(const char *s1, const char *s2);

static HPy decode_any(HPyContext *ctx, const char *buf, HPy_ssize_t *index, HPy_ssize_t size);
static HPy decode_int(HPyContext *ctx, const char *buf, HPy_ssize_t *index, HPy_ssize_t size);
static HPy decode_str(HPyContext *ctx, const char *buf, HPy_ssize_t *index, HPy_ssize_t size);
static HPy decode_list(HPyContext *ctx, const char *buf, HPy_ssize_t *index, HPy_ssize_t size);

static HPy decode_int(HPyContext *ctx, const char *buf, HPy_ssize_t *index, HPy_ssize_t size)
{
    HPy_ssize_t index_e = 0;

    for (HPy_ssize_t i = *index; i < size; i++)
    {
        if (buf[i] == 'e')
        {
            index_e = i;
            break;
        }
    }

    if (index_e == 0)
    {
        return decodeError(ctx, "invalid int, missing 'e': %d", *index);
    }

    long long sign = 1;
    *index = *index + 1;
    if (buf[*index] == '-')
    {
        if (buf[*index + 1] == '0')
        {
            return decodeError(ctx, "invalid int, '-0' found at %d", *index);
        }

        sign = -1;
        *index = *index + 1;
    }
    else if (buf[*index] == '0')
    {
        if (*index + 1 != index_e)
        {
            return decodeError(ctx, "invalid int, non-zero int should not start with '0'. found at %d", *index);
        }
    }

    long long val = 0;
    for (HPy_ssize_t i = *index; i < index_e; i++)
    {
        if (buf[i] > '9' || buf[i] < '0')
        {
            return decodeError(ctx, "invalid int, '%c' found at %d", buf[i], i);
        }
        val = val * 10 + (buf[i] - '0');
    }

    val = val * sign;

    *index = index_e + 1;
    return HPyLong_FromLongLong(ctx, val);
}

// there is no bytes/str in bencode, they only have 1 type for both of them.
static HPy decode_str(HPyContext *ctx, const char *buf, HPy_ssize_t *index, HPy_ssize_t size)
{
    HPy_ssize_t index_sep = 0;
    for (HPy_ssize_t i = *index; i < size; i++)
    {
        if (buf[i] == ':')
        {
            index_sep = i;
            break;
        }
    }

    if (index_sep == 0)
    {
        return decodeError(ctx, "invalid string, missing length: index %d", *index);
    }

    if (buf[*index] == '0' && *index + 1 != index_sep)
    {
        return decodeError(ctx, "invalid bytes length, found at %d", *index);
    }

    HPy_ssize_t len = 0;
    for (HPy_ssize_t i = *index; i < index_sep; i++)
    {
        if (buf[i] < '0' || buf[i] > '9')
        {
            return decodeError(ctx, "invalid bytes length, found '%c' at %d", buf[i], i);
        }
        len = len * 10 + (buf[i] - '0');
    }

    if (index_sep + len >= size)
    {
        return decodeError(ctx, "bytes length overflow, index %d", *index);
    }

    *index = index_sep + len + 1;

    return HPyBytes_FromStringAndSize(ctx, &buf[index_sep + 1], len);
}

static HPy decode_list(HPyContext *ctx, const char *buf, HPy_ssize_t *index, HPy_ssize_t size)
{
    *index = *index + 1;

    // HPy *objects = malloc(sizeof(HPy) * 8);

    HPy l = HPyList_New(ctx, 0);

    while (buf[*index] != 'e')
    {
        HPy obj = decode_any(ctx, buf, index, size);
        if (HPy_IsNull(obj))
        {
            return HPy_NULL;
        }

        HPyList_Append(ctx, l, obj);
    }

    *index = *index + 1;

    return l;
}

static int strCompare(const char *s1, size_t len1, const char *s2, size_t len2)
{
    size_t min_len = (len1 < len2) ? len1 : len2;
    int result = strncmp(s1, s2, min_len);

    if (result == 0)
    {
        // 前 min_len 个字符相等,比较字符串长度
        if (len1 < len2)
        {
            return -1;
        }
        else if (len1 > len2)
        {
            return 1;
        }
        else
        {
            return 0;
        }
    }

    return result;
}

static HPy decode_dict(HPyContext *ctx, const char *buf, HPy_ssize_t *index, HPy_ssize_t size)
{
    *index = *index + 1;

    HPy d = HPyDict_New(ctx);

    const char *lastKey = NULL;
    size_t lastKeyLen = 0;

    const char *currentKey;
    size_t currentKeyLen;

    while (buf[*index] != 'e')
    {
        HPy key = decode_str(ctx, buf, index, size);
        if (HPy_IsNull(key))
        {
            return HPy_NULL;
        }

        HPy obj = decode_any(ctx, buf, index, size);
        if (HPy_IsNull(obj))
        {
            return HPy_NULL;
        }

        currentKeyLen = HPyBytes_Size(ctx, key);
        currentKey = HPyBytes_AsString(ctx, key);

        // skip first key
        if (lastKey != NULL)
        {
            if (strCompare(currentKey, currentKeyLen, lastKey, lastKeyLen) < 0)
            {
                return decodeError(ctx, "invalid dict, key not sorted. index %d", *index);
            }
        }

        lastKey = currentKey;
        lastKeyLen = currentKeyLen;

        HPy_SetItem(ctx, d, key, obj);
    }

    *index = *index + 1;

    return d;
}

static HPy decode_any(HPyContext *ctx, const char *buf, HPy_ssize_t *index, HPy_ssize_t size)
{
    // int
    if (buf[*index] == 'i')
    {
        return decode_int(ctx, buf, index, size);
    }

    if (buf[*index] >= '0' && buf[*index] <= '9')
    {
        return decode_str(ctx, buf, index, size);
    }

    // list
    if (buf[*index] == 'l')
    {
        return decode_list(ctx, buf, index, size);
    }

    if (buf[*index] == 'd')
    {
        return decode_dict(ctx, buf, index, size);
    }

    return decodeError(ctx, "invalid bencode data, index %d", *index);
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

    HPy_ssize_t index = 0;

    HPy r = decode_any(ctx, buf, &index, size);

    if (index != size)
    {
        HPy_Close(ctx, r);
        return decodeError(ctx, "invalid bencode data, index %d", index);
    }

    return r;
};
