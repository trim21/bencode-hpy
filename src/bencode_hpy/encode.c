#include "stdio.h"
#include "string.h"

#include "hpy.h"

#define defaultBufferSize 4096

#define HPyLong_Check(ctx, obj) HPy_TypeCheck(ctx, obj, ctx->h_LongType)
#define returnIfError(o)                                                                                               \
  if (o) {                                                                                                             \
    return o;                                                                                                          \
  }

#define UNSUPPORTED_TYPE_MESSAGE "invalid type '%s' for bencode"

struct buffer;
static int encodeAny(HPyContext *ctx, struct buffer *buf, HPy obj);

static inline void runtimeError(HPyContext *ctx, const char *data) { HPyErr_SetString(ctx, ctx->h_RuntimeError, data); }

static inline void typeError(HPyContext *ctx, const char *data) { HPyErr_SetString(ctx, ctx->h_TypeError, data); }

HPyGlobal BencodeEncodeError;

static inline void bencodeError(HPyContext *ctx, const char *data) {
  HPyErr_SetString(ctx, HPyGlobal_Load(ctx, BencodeEncodeError), data);
}

struct buffer {
  char *buf;
  size_t len;
  size_t cap;
};

static struct buffer *newBuffer() {
  struct buffer *v = malloc(sizeof(struct buffer));

  v->buf = (char *)malloc(defaultBufferSize);
  v->len = 0;
  v->cap = defaultBufferSize;

  return v;
}

static int bufferWrite(HPyContext *ctx, struct buffer *buf, const char *data, HPy_ssize_t size) {
  void *tmp;

  if (size + buf->len >= buf->cap) {
    tmp = realloc(buf->buf, buf->cap * 2 + size);
    if (tmp == NULL) {
      runtimeError(ctx, "realloc failed");
      return 1;
    }
    buf->cap = buf->cap * 2 + size;
    buf->buf = tmp;
  }

  memcpy(buf->buf + buf->len, data, size);

  buf->len = buf->len + size;

  return 0;
}

static int bufferWriteLong(HPyContext *ctx, struct buffer *buf, long long val) {

  int j = snprintf(NULL, 0, "%lld", val) + 1;
  char *s = malloc(sizeof(char) * j);
  snprintf(s, j, "%lld", val);
  int r = bufferWrite(ctx, buf, s, j - 1);
  free(s);
  return r;
}

static void freeBuffer(struct buffer *buf) {
  free(buf->buf);
  free(buf);
}

// obj must be a python dict object.
// TODO: use c native struct and sorting
static int buildDictKeyList(HPyContext *ctx, HPy obj, HPy *list, HPy_ssize_t *count) {
  *count = HPy_Length(ctx, obj);
  if (*count == 0) {
    return 0;
  }

  HPy keys = HPyDict_Keys(ctx, obj);
  if (HPy_IsNull(keys)) {
    runtimeError(ctx, "failed to get dict keys");
    return 1;
  }

  HPyListBuilder b = HPyListBuilder_New(ctx, *count);

  for (HPy_ssize_t i = 0; i < *count; i++) {
    HPy key = HPy_GetItem_i(ctx, keys, i);
    HPy keyAsBytes = key;

    if (HPyUnicode_Check(ctx, key)) {
      keyAsBytes = HPyUnicode_AsUTF8String(ctx, key);
    } else if (!HPyBytes_Check(ctx, key)) {
      bencodeError(ctx, "dict key must be str or bytes");
      HPyListBuilder_Cancel(ctx, b);
      HPy_Close(ctx, key);
      HPy_Close(ctx, keys);
      return 1;
    }

    HPy value = HPy_GetItem(ctx, obj, key);
    if (HPy_IsNull(value)) {
      HPyListBuilder_Cancel(ctx, b);
      HPy_Close(ctx, keys);
      HPy_Close(ctx, keyAsBytes);
      runtimeError(ctx, "can not dict value");
      return 1;
    }

    HPy tu = HPyTuple_Pack(ctx, 2, keyAsBytes, value);
    if (HPy_IsNull(tu)) {
      HPyListBuilder_Cancel(ctx, b);
      HPy_Close(ctx, keys);
      HPy_Close(ctx, value);
      HPy_Close(ctx, keyAsBytes);
      runtimeError(ctx, "can not pack key value pair");
      return 1;
    }

    HPyListBuilder_Set(ctx, b, i, tu);
    HPy_Close(ctx, tu);
    HPy_Close(ctx, value);
    HPy_Close(ctx, key);
    HPy_Close(ctx, keyAsBytes);
  }

  HPy_Close(ctx, keys);

  *list = HPyListBuilder_Build(ctx, b);

  HPy sortName = HPyUnicode_FromString(ctx, "sort");
  if (HPy_IsNull(HPy_CallMethod(ctx, sortName, list, 1, HPy_NULL))) {
    HPy_Close(ctx, sortName);
    return 1;
  }
  HPy_Close(ctx, sortName);

  // check duplicated keys
  const char *currentKey = NULL;
  size_t currentKeylen = 0;
  const char *lastKey = NULL;
  size_t lastKeylen = 0;

  for (HPy_ssize_t i = 0; i < *count; i++) {
    HPy keyValue = HPy_GetItem_i(ctx, *list, i);
    HPy key = HPy_GetItem_i(ctx, keyValue, 0);
    currentKeylen = HPyBytes_GET_SIZE(ctx, key);
    currentKey = HPyBytes_AsString(ctx, key);
    if (lastKey != NULL) {
      if (lastKeylen == currentKeylen) {
        if (strncmp(lastKey, currentKey, lastKeylen) == 0) {
          bencodeError(ctx, "find duplicated keys with str and bytes in dict");
          HPy_Close(ctx, keyValue);
          HPy_Close(ctx, key);
          return 1;
        }
      }
    }

    lastKey = currentKey;
    lastKeylen = currentKeylen;
    HPy_Close(ctx, keyValue);
    HPy_Close(ctx, key);
  }

  return 0;
}
static int encodeBytes(HPyContext *ctx, struct buffer *buf, HPy obj) {
  HPy_ssize_t size = HPyBytes_Size(ctx, obj);
  const char *data = HPyBytes_AsString(ctx, obj);

  returnIfError(bufferWriteLong(ctx, buf, size));
  returnIfError(bufferWrite(ctx, buf, ":", 1));
  return bufferWrite(ctx, buf, data, size);
}

static int encodeDict(HPyContext *ctx, struct buffer *buf, HPy obj) {
  returnIfError(bufferWrite(ctx, buf, "d", 1));

  HPy list = HPy_NULL;
  HPy_ssize_t count = 0;
  if (buildDictKeyList(ctx, obj, &list, &count)) {
    if (!HPy_IsNull(list)) {
      HPy_Close(ctx, list);
    }

    return 1;
  }

  if (count == 0) {
    return bufferWrite(ctx, buf, "e", 1);
  }

  for (HPy_ssize_t i = 0; i < count; i++) {
    HPy keyValue = HPy_GetItem_i(ctx, list, i); // tuple[bytes, Any]
    if (HPy_IsNull(keyValue)) {
      HPy_Close(ctx, list);
      runtimeError(ctx, "failed to get key/value tuple from list");
      return 1;
    }

    HPy key = HPy_GetItem_i(ctx, keyValue, 0);
    if (HPy_IsNull(key)) {
      HPy_Close(ctx, keyValue);
      HPy_Close(ctx, list);
      runtimeError(ctx, "can't get key from key,value tuple");
      return 1;
    }
    if (encodeBytes(ctx, buf, key)) {
      HPy_Close(ctx, keyValue);
      HPy_Close(ctx, list);
      HPy_Close(ctx, key);
      return 1;
    }

    HPy_Close(ctx, key);

    HPy value = HPy_GetItem_i(ctx, keyValue, 1);
    if (HPy_IsNull(value)) {
      HPy_Close(ctx, keyValue);
      HPy_Close(ctx, list);
      runtimeError(ctx, "can't get value");
      return 1;
    }
    if (encodeAny(ctx, buf, value)) {
      HPy_Close(ctx, keyValue);
      HPy_Close(ctx, list);
      HPy_Close(ctx, value);
      return 1;
    }

    HPy_Close(ctx, keyValue);
    HPy_Close(ctx, value);
  }

  HPy_Close(ctx, list);

  return bufferWrite(ctx, buf, "e", 1);
}

static int encodeAny(HPyContext *ctx, struct buffer *buf, HPy obj) {
  if (HPy_Is(ctx, ctx->h_True, obj)) {
    return bufferWrite(ctx, buf, "i1e", 3);
  } else if (HPy_Is(ctx, ctx->h_False, obj)) {
    return bufferWrite(ctx, buf, "i0e", 3);
  } else if (HPyBytes_Check(ctx, obj)) {
    return encodeBytes(ctx, buf, obj);
  } else if (HPyUnicode_Check(ctx, obj)) {
    HPy_ssize_t size;
    const char *data = HPyUnicode_AsUTF8AndSize(ctx, obj, &size);

    returnIfError(bufferWriteLong(ctx, buf, size));
    returnIfError(bufferWrite(ctx, buf, ":", 1));
    return bufferWrite(ctx, buf, data, size);
  } else if (HPyLong_Check(ctx, obj)) {
    long long val = HPyLong_AsLongLong(ctx, obj);

    // python int may overflow c long long
    if (HPyErr_Occurred(ctx)) {
      return 1;
    }

    returnIfError(bufferWrite(ctx, buf, "i", 1));
    returnIfError(bufferWriteLong(ctx, buf, val));
    return bufferWrite(ctx, buf, "e", 1);
  } else if (HPyList_Check(ctx, obj) || HPyTuple_Check(ctx, obj)) {
    HPy_ssize_t len = HPy_Length(ctx, obj);
    returnIfError(bufferWrite(ctx, buf, "l", 1));

    for (HPy_ssize_t i = 0; i < len; i++) {
      HPy o = HPy_GetItem_i(ctx, obj, i);
      if (HPy_IsNull(o)) {
        runtimeError(ctx, "failed to get item from list/tuple");
        return 1;
      }
      if (encodeAny(ctx, buf, o)) {
        HPy_Close(ctx, o);
        return 1;
      }
      HPy_Close(ctx, o);
    }

    return bufferWrite(ctx, buf, "e", 1);
  } else if (HPyDict_Check(ctx, obj)) {
    return encodeDict(ctx, buf, obj);
  }

  HPy typ = HPy_Type(ctx, obj);

  // TODO: `None` become 'NoneType'
  const char *typName = HPyType_GetName(ctx, typ);

  int j = snprintf(NULL, 0, UNSUPPORTED_TYPE_MESSAGE, typName) + 1;

  char *s = malloc(sizeof(char) * j);

  snprintf(s, j, UNSUPPORTED_TYPE_MESSAGE, typName);

  HPyErr_SetString(ctx, HPyGlobal_Load(ctx, BencodeEncodeError), s);

  HPy_Close(ctx, typ);

  free(s);

  return 1;
}

HPyDef_METH(bencode, "bencode", HPyFunc_O) static HPy bencode_impl(HPyContext *ctx, HPy self, HPy obj) {
  // self is the module object

  struct buffer *buf = newBuffer();

  if (encodeAny(ctx, buf, obj)) {
    freeBuffer(buf);
    // error when encoding
    return HPy_NULL;
  }

  HPy res = HPyBytes_FromStringAndSize(ctx, buf->buf, buf->len);

  freeBuffer(buf);

  return res;
};
