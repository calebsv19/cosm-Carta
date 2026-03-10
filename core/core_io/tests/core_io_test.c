#include "core_io.h"

#include <assert.h>
#include <string.h>

int main(void) {
    const char *path = "/tmp/core_io_test.bin";
    const char *missing = "/tmp/core_io_test_missing.bin";
    const char payload[] = "core-io-roundtrip";

    CoreResult w = core_io_write_all(path, payload, sizeof(payload));
    assert(w.code == CORE_OK);
    assert(core_io_path_exists(path));
    assert(!core_io_path_exists(missing));

    CoreBuffer buf = {0};
    CoreResult r = core_io_read_all(path, &buf);
    assert(r.code == CORE_OK);
    assert(buf.size == sizeof(payload));
    assert(memcmp(buf.data, payload, sizeof(payload)) == 0);

    core_io_buffer_free(&buf);
    return 0;
}
