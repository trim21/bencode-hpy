#include "stdio.h"

#include "hpy.h"

struct str
{
    char *buf;
    size_t len;
};

char *itoa(int num, char *buffer, int base);

struct buffer
{
    char *buf;
    HPy_ssize_t len;
    HPy_ssize_t cap;
};

static struct buffer *newBuffer()
{
    struct buffer *v = malloc(sizeof(struct buffer));

    v->buf = (char *)malloc(1024);
    v->len = 0;
    v->cap = 1024;

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
            HPyErr_SetString(ctx, ctx->h_TypeError, "argument must be bytes");
            return 1;
        }
        buf->cap = buf->cap * 2;
    }

    memcpy(buf->buf + buf->len, data, size);

    buf->len = buf->len + size;

    return 0;
}

static void freeBuffer(struct buffer *buf)
{
    free(buf->buf);
    free(buf);
}

static int encode_any(HPyContext *ctx, struct buffer *buf, HPy obj)
{
    if (HPyBytes_Check(ctx, obj))
    {

        HPy_ssize_t size = HPyBytes_Size(ctx, obj);
        const char *data = HPyBytes_AsString(ctx, obj);

        l = itoa(size);

        return writeBuffer(ctx, buf, data, size);
    }
    return 0;
}

HPyDef_METH(encode, "encode", HPyFunc_O) static HPy encode_impl(HPyContext *ctx, HPy self, HPy obj)
{
    // self is the module object

    struct buffer *buf = newBuffer();

    if (encode_any(ctx, buf, obj))
    {
        freeBuffer(buf);
        // error when encoding
        return HPy_NULL;
    }

    HPy res = HPyBytes_FromStringAndSize(ctx, buf->buf, buf->len);

    freeBuffer(buf);

    return res;
};

void reverse(char str[], int length)
{
    int start = 0;
    int end = length - 1;
    while (start < end)
    {
        char temp = str[start];
        str[start] = str[end];
        str[end] = temp;
        end--;
        start++;
    }
}

// Implementation of itoa()
char *itoa(int num, char *str, int base)
{
    int i = 0;
    bool isNegative = false;

    /* Handle 0 explicitly, otherwise empty string is
     * printed for 0 */
    if (num == 0)
    {
        str[i++] = '0';
        str[i] = '\0';
        return str;
    }

    // In standard itoa(), negative numbers are handled
    // only with base 10. Otherwise numbers are
    // considered unsigned.
    if (num < 0 && base == 10)
    {
        isNegative = true;
        num = -num;
    }

    // Process individual digits
    while (num != 0)
    {
        int rem = num % base;
        str[i++] = (rem > 9) ? (rem - 10) + 'a' : rem + '0';
        num = num / base;
    }

    // If number is negative, append '-'
    if (isNegative)
        str[i++] = '-';

    str[i] = '\0'; // Append string terminator

    // Reverse the string
    reverse(str, i);

    return str;
}
