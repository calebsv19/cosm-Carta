#include "core_base.h"

#include <assert.h>
#include <string.h>

static void test_core_str_behavior(void) {
    CoreStr a = core_str_from_cstr("abc");
    CoreStr b = core_str_from_cstr("abc");
    CoreStr c = core_str_from_cstr("abcd");
    CoreStr empty = core_str_from_cstr("");
    CoreStr null_str = core_str_from_cstr(NULL);
    CoreStr manual_null_empty = {NULL, 0};
    CoreStr manual_null_nonempty = {NULL, 1};

    assert(core_str_eq(a, b));
    assert(!core_str_eq(a, c));

    assert(empty.len == 0u);
    assert(empty.data != NULL);
    assert(null_str.len == 0u);
    assert(null_str.data == NULL);
    assert(core_str_eq(empty, null_str));
    assert(core_str_eq(null_str, manual_null_empty));
    assert(!core_str_eq(manual_null_nonempty, a));
}

static void test_path_helpers(void) {
    const char *path = "dir/file.pack";
    const char *trailing = "dir/subdir/";
    const char *dotfile = ".gitignore";
    const char *no_ext = "README";
    const char *empty_ext = "file.";
    const char *multi_dot = "archive.tar.gz";
    const char *windows_path = "C:\\dir\\file.txt";
    const char *windows_trailing = "C:\\dir\\";
    const char *rootish = "/";
    const char *empty = "";

    assert(strcmp(core_path_basename(path), "file.pack") == 0);
    assert(strcmp(core_path_ext(path), ".pack") == 0);

    assert(strcmp(core_path_basename(trailing), "") == 0);
    assert(core_path_ext(trailing) == NULL);

    assert(strcmp(core_path_basename(dotfile), ".gitignore") == 0);
    assert(strcmp(core_path_ext(dotfile), ".gitignore") == 0);

    assert(strcmp(core_path_basename(no_ext), "README") == 0);
    assert(core_path_ext(no_ext) == NULL);

    assert(strcmp(core_path_basename(empty_ext), "file.") == 0);
    assert(strcmp(core_path_ext(empty_ext), ".") == 0);

    assert(strcmp(core_path_basename(multi_dot), "archive.tar.gz") == 0);
    assert(strcmp(core_path_ext(multi_dot), ".gz") == 0);

    assert(strcmp(core_path_basename(windows_path), "file.txt") == 0);
    assert(strcmp(core_path_ext(windows_path), ".txt") == 0);

    assert(strcmp(core_path_basename(windows_trailing), "") == 0);
    assert(core_path_ext(windows_trailing) == NULL);

    assert(strcmp(core_path_basename(rootish), "") == 0);
    assert(core_path_ext(rootish) == NULL);

    assert(core_path_basename(NULL) == NULL);
    assert(core_path_basename(empty) == empty);
    assert(core_path_ext(NULL) == NULL);
    assert(core_path_ext(empty) == NULL);
}

static void test_hash_behavior(void) {
    const char payload[] = "hash-me";
    uint64_t h64_payload = core_hash64_fnv1a(payload, sizeof(payload) - 1u);
    uint32_t h32_payload = core_hash32_fnv1a(payload, sizeof(payload) - 1u);
    uint64_t h64_empty = core_hash64_fnv1a(NULL, 0u);
    uint32_t h32_empty = core_hash32_fnv1a(NULL, 0u);

    assert(h64_payload != 0u);
    assert(h32_payload != 0u);
    assert(core_hash64_fnv1a(payload, 0u) == h64_empty);
    assert(core_hash32_fnv1a(payload, 0u) == h32_empty);
    assert(core_hash64_fnv1a(NULL, 3u) == 0u);
    assert(core_hash32_fnv1a(NULL, 3u) == 0u);
}

static void test_allocation_wrappers(void) {
    void *p = core_alloc(64u);
    assert(p != NULL);
    p = core_realloc(p, 128u);
    assert(p != NULL);
    p = core_realloc(p, 0u);
    assert(p == NULL);

    assert(core_alloc(0u) == NULL);
    assert(core_calloc(0u, 8u) == NULL);
    assert(core_calloc(8u, 0u) == NULL);
    assert(core_realloc(NULL, 0u) == NULL);

    p = core_calloc(4u, sizeof(int));
    assert(p != NULL);
    core_free(p);
    core_free(NULL);
}

int main(void) {
    test_core_str_behavior();
    test_path_helpers();
    test_hash_behavior();
    test_allocation_wrappers();
    return 0;
}
