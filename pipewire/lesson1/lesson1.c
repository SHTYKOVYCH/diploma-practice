#include <pipewire/pipewire.h>

int main(int argc, char* argv[]) {
    pw_init(&argc, &argv);

    fprintf(stdout, "Compiled with version %s\nLinked with version %s\n", pw_get_headers_version(), pw_get_library_version());

    return 0;
}
