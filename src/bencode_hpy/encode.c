#include "stdio.h"

#include "string.h"

#include "hpy.h"

#define defaultBufferSize 4096

#define HPyLong_Check(ctx, obj) HPy_TypeCheck(ctx, obj, ctx->h_LongType)
#define returnIfError(o)                                                                                               \
    if (o)                                                                                                             \
    {                                                                                                                  \
        return o;                                                                                                      \
    }

static inline void runtimeError(HPyContext *ctx, const char *data)
{
    HPyErr_SetString(ctx, ctx->h_RuntimeError, data);
}

static inline void typeError(HPyContext *ctx, const char *data)
{
    HPyErr_SetString(ctx, ctx->h_TypeError, data);
}

HPyGlobal BencodeEncodeError;

static inline void bencodeError(HPyContext *ctx, const char *data)
{
    HPyErr_SetString(ctx, HPyGlobal_Load(ctx, BencodeEncodeError), data);
}

struct str
{
    char *buf;
    size_t len;
};

struct buffer
{
    char *buf;
    HPy_ssize_t len;
    HPy_ssize_t cap;
};

static struct buffer *newBuffer()
{
    struct buffer *v = malloc(sizeof(struct buffer));

    v->buf = (char *)malloc(defaultBufferSize);
    v->len = 0;
    v->cap = defaultBufferSize;

    return v;
}

static int writeBuffer(HPyContext *ctx, struct buffer *buf, const char *data, HPy_ssize_t size)
{
    void *tmp;

    if (size + buf->len >= buf->cap)
    {
        tmp = realloc(buf->buf, buf->cap * 2);
        if (tmp == NULL)
        {
            runtimeError(ctx, "realloc failed");
            return 1;
        }
        buf->cap = buf->cap * 2;
    }

    memcpy(buf->buf + buf->len, data, size);

    buf->len = buf->len + size;

    return 0;
}

static int bufferWriteLong(HPyContext *ctx, struct buffer *buf, long long val)
{

    int j = snprintf(NULL, 0, "%lld", val) + 1;
    char *s = malloc(sizeof(char) * j);
    snprintf(s, j, "%lld", val);
    int r = writeBuffer(ctx, buf, s, j - 1);
    free(s);
    return r;
}

static void freeBuffer(struct buffer *buf)
{
    free(buf->buf);
    free(buf);
}

// obj must be a python dict object.
// TODO: use c native struct and sorting
static int buildDictKeyList(HPyContext *ctx, HPy obj, HPy *list, HPy_ssize_t *count)
{
    HPy keys = HPyDict_Keys(ctx, obj);
    if (HPy_IsNull(keys))
    {
        runtimeError(ctx, "failed to get dict keys");
        return 1;
    }

    *count = HPy_Length(ctx, keys);

    printf("count %ld\n", *count);

    HPyListBuilder b = HPyListBuilder_New(ctx, *count);

    for (HPy_ssize_t i = 0; i < *count; i++)
    {
        printf("loop %ld\n", i);
        HPy key = HPy_GetItem_i(ctx, keys, i);
        HPy value = HPy_GetItem(ctx, obj, key);
        HPy keyAsBytes = key;
        if (HPyUnicode_Check(ctx, key))
        {
            keyAsBytes = HPyUnicode_AsUTF8String(ctx, key);
        }
        else if (!HPyBytes_Check(ctx, key))
        {
            bencodeError(ctx, "dict key must be str or bytes");
            HPy_Close(ctx, key);
            return 1;
        }

        HPy tu = HPyTuple_Pack(ctx, 2, key, value);
        if (HPy_IsNull(tu))
        {
            runtimeError(ctx, "can not pack key value pair");
            return 1;
        }

        HPyListBuilder_Set(ctx, b, i, tu);
    }

    printf("buidl list\n");

    *list = HPyListBuilder_Build(ctx, b);

    HPy sortName = HPyUnicode_FromString(ctx, "sort");
    if (HPy_IsNull(HPy_CallMethod(ctx, sortName, list, 1, HPy_NULL)))
    {
        return 1;
    }

    HPy_Close(ctx, sortName);
    HPy_Close(ctx, keys);

    // check duplicated keys
    const char *currentKey = NULL;
    size_t currentKeylen = 0;
    const char *lastKey = NULL;
    size_t lastKeylen = 0;

    for (HPy_ssize_t i = 0; i < *count; i++)
    {
        HPy keyValue = HPy_GetItem_i(ctx, *list, i);
        HPy key = HPy_GetItem_i(ctx, keyValue, 0);
        currentKeylen = HPyBytes_GET_SIZE(ctx, key);
        currentKey = HPyBytes_AsString(ctx, key);
        if (lastKey != NULL)
        {
            if (lastKeylen == currentKeylen)
            {
                if (strncmp(lastKey, currentKey, lastKeylen) == 0)
                {
                    bencodeError(ctx, "find duplicated keys with str and bytes in dict");
                    return 1;
                }
            }
        }

        lastKey = currentKey;
        lastKeylen = currentKeylen;
    }

    return 0;
}
static int encodeBytes(HPyContext *ctx, struct buffer *buf, HPy obj)
{
    HPy_ssize_t size = HPyBytes_Size(ctx, obj);
    const char *data = HPyBytes_AsString(ctx, obj);

    returnIfError(bufferWriteLong(ctx, buf, size));
    returnIfError(writeBuffer(ctx, buf, ":", 1));
    return writeBuffer(ctx, buf, data, size);
}

static int encodeAny(HPyContext *ctx, struct buffer *buf, HPy obj)
{
    printf("encode value\n");
    if (HPy_Is(ctx, ctx->h_True, obj))
    {
        return writeBuffer(ctx, buf, "i1e", 3);
    }
    else if (HPy_Is(ctx, ctx->h_False, obj))
    {
        return writeBuffer(ctx, buf, "i0e", 3);
    }
    else if (HPyBytes_Check(ctx, obj))
    {
        return encodeBytes(ctx, buf, obj);
    }
    else if (HPyUnicode_Check(ctx, obj))
    {
        HPy_ssize_t size;
        const char *data = HPyUnicode_AsUTF8AndSize(ctx, obj, &size);

        returnIfError(bufferWriteLong(ctx, buf, size));
        returnIfError(writeBuffer(ctx, buf, ":", 1));
        return writeBuffer(ctx, buf, data, size);
    }
    else if (HPyLong_Check(ctx, obj))
    {
        printf("encode long\n");
        HPy_ssize_t size = HPyLong_AsSize_t(ctx, obj);

        returnIfError(writeBuffer(ctx, buf, "i", 1));
        returnIfError(bufferWriteLong(ctx, buf, size));
        return writeBuffer(ctx, buf, "e", 1);
    }
    else if (HPyList_Check(ctx, obj) || HPyTuple_Check(ctx, obj))
    {
        HPy_ssize_t len = HPy_Length(ctx, obj);
        returnIfError(writeBuffer(ctx, buf, "l", 1));

        for (HPy_ssize_t i = 0; i < len; i++)
        {
            HPy o = HPy_GetItem_i(ctx, obj, i);
            returnIfError(encodeAny(ctx, buf, o));
        }
        return writeBuffer(ctx, buf, "e", 1);
    }
    else if (HPyDict_Check(ctx, obj))
    {
        printf("encode dict\n");
        returnIfError(writeBuffer(ctx, buf, "d", 1));
        HPy list = HPy_NULL;
        HPy_ssize_t count = 0;
        if (buildDictKeyList(ctx, obj, &list, &count))
        {
            return 1;
        }

        printf("count %ld\n", count);

        for (HPy_ssize_t i = 0; i < count; i++)
        {
            printf("451\n");
            HPy keyValue = HPy_GetItem_i(ctx, list, i); // tuple[bytes, Any]
            if (HPy_IsNull(keyValue))
            {
                runtimeError(ctx, "why???!!!");
                return 1;
            }
            printf("452\n");
            HPy key = HPy_GetItem_i(ctx, keyValue, 0);
            HPy value = HPy_GetItem_i(ctx, keyValue, 1);
            if (HPy_IsNull(key))
            {
                runtimeError(ctx, "can't get key from key,value tuple");
                return 1;
            }
            if (HPy_IsNull(value))
            {
                runtimeError(ctx, "can't get value");
                return 1;
            }
            printf("453\n");
            if (encodeBytes(ctx, buf, key))
            {
                printf("5\n");
                HPy_Close(ctx, list);
                return 1;
            }
            printf("454\n");
            if (encodeAny(ctx, buf, value))
            {
                printf("6\n");
                HPy_Close(ctx, list);
                return 1;
            }

            printf("7\n");
            HPy_Close(ctx, key);
            HPy_Close(ctx, value);
        }

        printf("8\n");

        HPy_Close(ctx, list);
        return writeBuffer(ctx, buf, "e", 1);
    }

    HPy typ = HPy_Type(ctx, obj);

    // TODO: `None` become
    const char *typName = HPyType_GetName(ctx, typ);

    int j = snprintf(NULL, 0, "invalid type '%s'", typName) + 1;

    char *s = malloc(sizeof(char) * j);

    snprintf(s, j, "invalid type '%s'", typName);

    HPyErr_SetString(ctx, HPyGlobal_Load(ctx, BencodeEncodeError), s);

    free(s);

    return 1;
}

HPyDef_METH(bencode, "bencode", HPyFunc_O) static HPy bencode_impl(HPyContext *ctx, HPy self, HPy obj)
{
    // self is the module object

    struct buffer *buf = newBuffer();

    if (encodeAny(ctx, buf, obj))
    {
        freeBuffer(buf);
        // error when encoding
        return HPy_NULL;
    }

    HPy res = HPyBytes_FromStringAndSize(ctx, buf->buf, buf->len);

    freeBuffer(buf);

    return res;
};
