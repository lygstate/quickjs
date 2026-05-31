#include "quickjs-pal.h"

static void *js_pal_calloc(void *opaque, size_t count, size_t size)
{
    return pal_mallocz(count * size);
}

static void *js_pal_malloc(void *opaque, size_t size)
{
    return pal_malloc(size);
}

static void js_pal_free(void *opaque, void *ptr)
{
    pal_free(ptr);
}

static void *js_pal_realloc(void *opaque, void *ptr, size_t size)
{
    return pal_realloc(ptr, size);
}

static const JSMallocFunctions js_pal_malloc_funcs = {
    js_pal_calloc,
    js_pal_malloc,
    js_pal_free,
    js_pal_realloc,
    pal_malloc_usable_size,
};

JSRuntime *JS_NewRuntimePAL(void)
{
    return JS_NewRuntime2(&js_pal_malloc_funcs, NULL);
}
