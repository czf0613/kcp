#include "ikcp.h"
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <pthread.h>

// socket fds
static int fd1, fd2;
static struct sockaddr_in addr1 = {0}, addr2 = {0};
static uint8_t *read_buf1, *read_buf2;
static ikcpcb *session1 = NULL, *session2 = NULL;
static pthread_t thread1, thread2;

static uint32_t iclock()
{
    struct timeval time;
    gettimeofday(&time, NULL);
    uint32_t milliseconds = (uint32_t)(time.tv_sec * 1000 + time.tv_usec / 1000);
    return milliseconds;
}

static void set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static int udp_output1(const uint8_t *buf, int len, struct IKCPCB *kcp, void *user)
{
    sendto(fd1, buf, len, 0, (const struct sockaddr *)&addr2, sizeof(addr2));
    return 0;
}

static int udp_output2(const uint8_t *buf, int len, struct IKCPCB *kcp, void *user)
{
    sendto(fd2, buf, len, 0, (const struct sockaddr *)&addr1, sizeof(addr1));
    return 0;
}

volatile bool stop_flag = false;
void handle_signal(int signal)
{
    if (signal == SIGINT)
    {
        stop_flag = true;
    }
}

static void *job1(void *args)
{
    uint64_t time = 0;
    printf("thread1 started\n");

    while (!stop_flag)
    {
        int n_data = recvfrom(fd1, read_buf1, 1048576, 0, NULL, NULL);
        if (n_data > 0)
        {
            ikcp_input(session1, read_buf1, n_data);
        }

        ikcp_update(session1, iclock());

        n_data = ikcp_recv(session1, read_buf1, 1048576);
        if (n_data > 0)
        {
            char *content = calloc(n_data + 1, 1);
            memcpy(content, read_buf1, n_data);
            printf("session1 recv: %s\n", content);
            free(content);
        }

        usleep(10000);

        if (time % 100 == 0)
        {
            char msg[128] = {0};
            sprintf(msg, "[%lld] Hello, World2!", time / 100);
            ikcp_send(session1, (const uint8_t *)msg, strlen(msg));
        }
        time++;
    }

    return NULL;
}

static void *job2(void *args)
{
    uint64_t time = 0;
    printf("thread2 started\n");

    while (!stop_flag)
    {
        int n_data = recvfrom(fd2, read_buf2, 1048576, 0, NULL, NULL);
        if (n_data > 0)
        {
            ikcp_input(session2, read_buf2, n_data);
        }

        ikcp_update(session2, iclock());

        n_data = ikcp_recv(session2, read_buf2, 1048576);
        if (n_data > 0)
        {
            char *content = calloc(n_data + 1, 1);
            memcpy(content, read_buf2, n_data);
            printf("session2 recv: %s\n", content);
            free(content);
        }

        usleep(10000);

        if (time % 100 == 0)
        {
            char msg[128] = {0};
            sprintf(msg, "[%lld] Hello, World1!", time / 100);
            ikcp_send(session2, (const uint8_t *)msg, strlen(msg));
        }
        time++;
    }

    return NULL;
}

// 只在macOS/Linux上可用该测试样例
int main()
{
    fd1 = socket(AF_INET, SOCK_DGRAM, 0), fd2 = socket(AF_INET, SOCK_DGRAM, 0);
    read_buf1 = malloc(1048576), read_buf2 = malloc(1048576);
    printf("socket created, fd1=%d, fd2=%d\n", fd1, fd2);

    addr1.sin_family = AF_INET;
    addr1.sin_port = htons(12345);
    addr1.sin_addr.s_addr = INADDR_ANY;
    addr2.sin_family = AF_INET;
    addr2.sin_port = htons(12346);
    addr2.sin_addr.s_addr = INADDR_ANY;

    bind(fd1, (const struct sockaddr *)&addr1, sizeof(addr1));
    set_nonblocking(fd1);
    session1 = ikcp_create(0x114514, NULL);
    session1->output = udp_output1;
    printf("ikcp created, session1=%p\n", session1);

    bind(fd2, (const struct sockaddr *)&addr2, sizeof(addr2));
    set_nonblocking(fd2);
    session2 = ikcp_create(0x114514, NULL);
    session2->output = udp_output2;
    printf("ikcp created, session2=%p\n", session2);

    signal(SIGINT, handle_signal);
    pthread_create(&thread1, NULL, job1, NULL);
    pthread_create(&thread2, NULL, job2, NULL);

    while (!stop_flag)
    {
        usleep(10000);
    }
    printf("cleaning up...\n");
    sleep(2);

    close(fd1);
    ikcp_release(session1);
    free(read_buf1);
    pthread_kill(thread1, SIGINT);

    close(fd2);
    ikcp_release(session2);
    free(read_buf2);
    pthread_kill(thread2, SIGINT);
}