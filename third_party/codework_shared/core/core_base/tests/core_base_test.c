#include "core_base.h"

#include <assert.h>
#include <string.h>

int main(void) {
    CoreStr a = core_str_from_cstr("abc");
    CoreStr b = core_str_from_cstr("abc");
    CoreStr c = core_str_from_cstr("abcd");

    assert(core_str_eq(a, b));
    assert(!core_str_eq(a, c));

    const char *path = "dir/file.pack";
    assert(strcmp(core_path_basename(path), "file.pack") == 0);
    assert(strcmp(core_path_ext(path), ".pack") == 0);

    const char payload[] = "hash-me";
    assert(core_hash64_fnv1a(payload, sizeof(payload) - 1) != 0);
    assert(core_hash32_fnv1a(payload, sizeof(payload) - 1) != 0);

    void *p = core_alloc(64);
    assert(p != NULL);
    p = core_realloc(p, 128);
    assert(p != NULL);
    core_free(p);

    return 0;
}
