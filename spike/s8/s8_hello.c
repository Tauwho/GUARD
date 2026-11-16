/* Spike S8 (#35): a stand-in GUARD binary started at boot from post_start.custom.
 * It records that it ran, and as which uid, then exits. */
#include <stdio.h>
#include <unistd.h>

int main(int argc, char **argv) {
    const char *path = argc > 1 ? argv[1] : "/data/var/tmp/s8_hello.txt";
    FILE *f = fopen(path, "w");
    if (f == NULL) {
        perror("s8_hello: fopen");
        return 1;
    }
    fprintf(f, "s8_hello started at boot: uid=%d euid=%d\n", (int)getuid(), (int)geteuid());
    fclose(f);
    return 0;
}
