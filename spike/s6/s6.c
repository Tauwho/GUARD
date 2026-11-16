/* Spike S6 (#32): send N JSON lines from the target to the host on TCP 7700.
 * Use this only if `nc` is missing on the image.
 *   ./s6 <host-ip> [count]   default count 1000 */
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

int main(int argc, char **argv) {
    struct sockaddr_in a = {0};
    a.sin_family = AF_INET;
    a.sin_port = htons(7700);
    if (argc < 2 || inet_pton(AF_INET, argv[1], &a.sin_addr) != 1) {
        fprintf(stderr, "usage: %s <host-ip> [count]\n", argv[0]);
        return 2;
    }
    int count = argc > 2 ? atoi(argv[2]) : 1000;
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0 || connect(s, (struct sockaddr *)&a, sizeof a) != 0) {
        perror("connect");
        return 1;
    }
    char b[32];
    for (int i = 0; i < count; i++) {
        int n = snprintf(b, sizeof b, "{\"seq\":%d}\n", i);
        if (write(s, b, (size_t)n) != n) {
            perror("write");
            return 1;
        }
    }
    close(s);
    printf("s6: sent %d lines\n", count);
    return 0;
}
