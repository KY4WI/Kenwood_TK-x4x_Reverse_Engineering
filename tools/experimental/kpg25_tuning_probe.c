/*
 * Kenwood TK-x4x stock tuning-mode read-only probe.
 * Copyright (c) 2026 Dave, KY4WI
 * GPL-3.0-only; see the RE repository LICENSE.
 *
 * Experimental research tool, not a calibration backup/restore utility.
 * The only transmitted data are ASCII "TUNING ", command 05, optional
 * command 2C, ACK 06, and exit 45. No EEPROM write or erase commands.
 */
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#define ACK 0x06
#define MAX_REPLY 32

static volatile sig_atomic_t interrupted;
static void on_signal(int sig) { (void)sig; interrupted = 1; }

static int configure(int fd, speed_t baud)
{
    struct termios t;
    if (tcgetattr(fd, &t) < 0) return -1;
    t.c_iflag = 0;
    t.c_oflag = 0;
    t.c_lflag = 0;
    t.c_cflag = (t.c_cflag & ~(CSIZE | PARENB | PARODD)) | CS8 | CSTOPB | CLOCAL | CREAD;
    t.c_iflag &= ~(IXON | IXOFF | IXANY | ISTRIP);
#ifdef CRTSCTS
    t.c_cflag &= ~CRTSCTS;
#endif
    t.c_cc[VMIN] = 0;
    t.c_cc[VTIME] = 0;
    if (cfsetispeed(&t, baud) < 0 || cfsetospeed(&t, baud) < 0) return -1;
    if (tcsetattr(fd, TCSANOW, &t) < 0) return -1;
    return 0;
}

static int send_bytes(int fd, const uint8_t *bytes, size_t n)
{
    size_t off = 0;
    while (off < n && !interrupted) {
        ssize_t r = write(fd, bytes + off, n - off);
        if (r > 0) off += (size_t)r;
        else if (r < 0 && errno == EINTR) continue;
        else if (r < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            struct pollfd p = {fd, POLLOUT, 0};
            if (poll(&p, 1, 2000) <= 0) return -1;
        } else return -1;
    }
    return !interrupted && tcdrain(fd) == 0 ? 0 : -1;
}

static int send_byte(int fd, uint8_t b) { return send_bytes(fd, &b, 1); }

/* Timeout in milliseconds. 0 = silence timeout, -1 = communication error. */
static int recv_byte(int fd, int timeout_ms)
{
    struct pollfd p = {fd, POLLIN, 0};
    for (;;) {
        int rv;
        if (interrupted) return -1;
        rv = poll(&p, 1, timeout_ms);
        if (rv == 0) return -2;
        if (rv < 0) { if (errno == EINTR) continue; return -1; }
        if (p.revents & (POLLERR | POLLHUP | POLLNVAL)) return -1;
        if (p.revents & POLLIN) {
            uint8_t byte;
            ssize_t n = read(fd, &byte, 1);
            if (n == 1) return byte;
            if (n < 0 && (errno == EINTR || errno == EAGAIN)) continue;
            return -1;
        }
    }
}

static int read_reply(int fd, uint8_t *dst, size_t cap, int first_timeout_ms)
{
    size_t n = 0;
    while (n < cap && !interrupted) {
        int b = recv_byte(fd, n ? 350 : first_timeout_ms);
        if (b == -2 && n) break;
        if (b < 0) return -1;
        dst[n++] = (uint8_t)b;
    }
    return (int)n;
}

static void show_report(FILE *out, const uint8_t *p, int n,
                        const uint8_t *extra, int n_extra)
{
    static const char *const labels_94x[8] = {
        "Squelch", "RF power", "Maximum deviation", "DQT balance",
        "QT fine deviation", "DQT fine deviation", "LTR fine deviation",
        "Frequency adjustment"
    };
    static const uint8_t addresses_94x[8] = {0xf8,0xf2,0xf4,0xf6,0xf9,0xfa,0xfb,0xf0};
    int i;
    fprintf(out, "Kenwood TK-x4x experimental tuning readback\n");
    fprintf(out, "Command 05: %d bytes\n", n);
    for (i = 0; i < n; i++) fprintf(out, "  [%02d] %02X\n", i, p[i]);
    if (n == 10) {
        fprintf(out, "\nTK-940/941 AB00 interpretation (verify model/revision):\n");
        for (i = 0; i < 8; i++)
            fprintf(out, "  EEPROM[%02X] %-22s %02X (shown as %u)\n",
                    addresses_94x[i],labels_94x[i],p[i],(unsigned)p[i]+1);
        fprintf(out, "  EEPROM[FD] selection index %u (original value may be clamped)\n",(unsigned)p[8]);
        fprintf(out, "  EEPROM[FE] item index      %u (original value may be clamped)\n",(unsigned)p[9]);
    } else if (n == 16 || n == 17) {
        static const char *const keys[] = {
            "Squelch", "RF power: test (computed)", "RF power: low",
            "RF power: center", "RF power: high", "Maximum deviation",
            "DQT balance", "QT fine deviation", "DQT fine deviation",
            "LTR fine deviation", "RX sensitivity: test (computed)",
            "RX sensitivity: low", "RX sensitivity: center",
            "RX sensitivity: high"
        };
        static const int eeaddr[] = {
            0xf8, -1, 0xf0, 0xf1, 0xf2, 0xf4, 0xf6,
            0xf9, 0xfa, 0xfb, -1, 0xed, 0xee, 0xef
        };
        fprintf(out, "\nTK-840 8355 response layout (other revisions not verified):\n");
        for (i = 0; i < 14; i++) {
            if (eeaddr[i] >= 0)
                fprintf(out, "  [%02d] EEPROM[%02X] %-29s %02X\n",
                        i,eeaddr[i],keys[i],p[i]);
            else
                fprintf(out, "  [%02d] computed   %-29s %02X\n",
                        i,keys[i],p[i]);
        }
        if (n == 17)
            fprintf(out, "  [14] EEPROM[EC] Frequency adjustment          %02X\n",p[14]);
        else
            fprintf(out, "  16-byte variant: EC value absent from this reply.\n");
        fprintf(out, "  [%02d] EEPROM[FD] selection (transformed)  %02X\n",n-2,p[n-2]);
        fprintf(out, "  [%02d] EEPROM[FE] item (transformed)       %02X\n",n-1,p[n-1]);
    } else fprintf(out, "\nUnexpected reply length: preserve raw data; interpretation unknown.\n");
    if (n_extra > 0) {
        fprintf(out, "\nCommand 2C: %d bytes (TK-940/941 EEPROM E9 down to E0):\n", n_extra);
        for (i = 0; i < n_extra; i++) fprintf(out, "  EEPROM[%02X] = %02X\n",0xe9-i,extra[i]);
    }
    fprintf(out, "\nNot a complete or verified calibration backup. NO EEPROM writes sent.\n");
}

int main(int argc, char **argv)
{
    int fd = -1, status = 1, n = -1, n_extra = 0, entered = 0;
    int extra_requested = 0, i, b;
    const char *path = NULL, *save_path = NULL;
    FILE *file = NULL;
    uint8_t reply[MAX_REPLY] = {0}, extra[MAX_REPLY] = {0};
    static const uint8_t trigger[] = {'T','U','N','I','N','G',' '};
    if (argc < 2) {
        fprintf(stderr,"Usage: %s /dev/ttyUSB0 [--read-e0] [--output FILE]\n",argv[0]);
        return 2;
    }
    path = argv[1];
    for (i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--read-e0") == 0) extra_requested = 1;
        else if (strcmp(argv[i], "--output") == 0 && i + 1 < argc) save_path = argv[++i];
        else { fprintf(stderr,"Unknown or incomplete argument: %s\n",argv[i]); return 2; }
    }
    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);
    fd = open(path, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) { perror(path); return 1; }
#ifdef TIOCEXCL
    if (ioctl(fd, TIOCEXCL) < 0) { perror("TIOCEXCL"); goto done; }
#endif
    if (configure(fd, B1200) < 0) { perror("configure 1200"); goto done; }
    tcflush(fd, TCIFLUSH);
    fprintf(stderr,"Sending stock DOS TUNING handshake (1200 8N2)...\n");
    if (send_bytes(fd, trigger, sizeof(trigger)) < 0) { perror("handshake write"); goto done; }
    b = recv_byte(fd, 4000);
    if (b != ACK) {
        fprintf(stderr,"No tuning ACK (received %d). No commands issued.\n",b);
        goto done;
    }
    entered = 1;
    if (configure(fd, B9600) < 0) { perror("configure 9600"); goto done; }
    if (send_byte(fd, 0x05) < 0) { perror("05 send"); goto done; }
    n = read_reply(fd, reply, sizeof(reply), 4000);
    if (n < 0) { fprintf(stderr,"Tuning reply missing/errored.\n"); goto done; }
    if (send_byte(fd, ACK) < 0) { perror("05 ACK write"); goto done; }
    b = recv_byte(fd, 2500);
    if (b != ACK) { fprintf(stderr,"05 trailing ACK missing (got %d).\n",b); goto done; }
    if (n != 10 && n != 16 && n != 17) {
        fprintf(stderr,"Unexpected 05 reply length %d. Not issuing further requests.\n",n);
        goto done;
    }
    if (extra_requested) {
        if (n != 10) {
            fprintf(stderr,"--read-e0 supported only after a 10-byte TK-940/941-style response.\n");
        } else {
            if (send_byte(fd, 0x2c) < 0) { perror("2C send"); goto done; }
            n_extra = read_reply(fd, extra, 10, 4000);
            if (n_extra != 10) { fprintf(stderr,"Unexpected 2C reply length %d.\n",n_extra); goto done; }
            if (send_byte(fd, ACK) < 0) { perror("2C ACK send"); goto done; }
            b = recv_byte(fd, 2500);
            if (b != ACK) { fprintf(stderr,"2C trailing ACK missing (got %d).\n",b); goto done; }
        }
    }
    show_report(stdout, reply, n, extra, n_extra);
    if (save_path) {
        int outfd = open(save_path, O_WRONLY | O_CREAT | O_EXCL, 0644);
        if (outfd < 0) { perror("output open (existing files not overwritten)"); goto done; }
        file = fdopen(outfd, "w");
        if (!file) { perror("fdopen"); close(outfd); goto done; }
        show_report(file, reply, n, extra, n_extra);
        if (fclose(file) != 0) { perror("output write"); file=NULL; goto done; }
        file=NULL;
        printf("Saved %s\n",save_path);
    }
    status = 0;
done:
    if (entered && fd >= 0 && !interrupted) {
        /* Official tune exit; unlike mask ROM's exit this command is 45h. */
        if (send_byte(fd, 0x45) != 0) fprintf(stderr,"Warning: unable to send tuning exit; power-cycle radio normally.\n");
        else (void)recv_byte(fd, 1300);
    }
    if (fd >= 0) close(fd);
    return status;
}
