
#include <cstdio>
#include <cstdlib>

#include "io.h"

int main(int argc, char* argv[]) {
    if (argc != 3) {
        fprintf(stderr, "invalid arguments!\n");
        exit(1);
    }
    bool ipv6;
    if (atoi(argv[1]) == 6) ipv6 = true;
    else if (atoi(argv[1]) == 4) ipv6 = false;
    else {
        fprintf(stderr, "invalid arguments!\n");
        exit(1);
    }

    char* data;
    printf("\n");
    int init = TCP_win32_init();
    if (init != 0) {
        fprintf(stderr, "Windows initialize function failed!\n");
        exit(1);
    }

    socket_t listensock = TCP_listen2(argv[2], ipv6, true);
    if (listensock == INV_SOCKET) {
        fprintf(stderr, "listen function failed!\n");
        exit(1);
    }

    socket_t sock = TCP_accept2(listensock, true);
    if (sock == INV_SOCKET) {
        TCP_close(listensock);
        fprintf(stderr, "accept function failed!\n");
        exit(1);
    }

    int size = TCP_recv2(sock, &data, true);
    printf("\n");
    if (size == -1) {
        TCP_close(listensock);
        fprintf(stderr, "receive function failed!\n");
        exit(1);
    }
    printf("receive size: %d\n", size);

    char* retdat = (char*)malloc(6220800); // 1 frame at HD = ~6MB
    if (TCP_send2(sock, retdat, 6220800, true) != 6220800) {
        TCP_close(listensock);
        free(retdat);
        free(data);
        fprintf(stderr, "send function failed!\n");
        exit(1);
    }

    TCP_close(sock);
    TCP_close(listensock);
    free(retdat);
    free(data);

    return 0;
}