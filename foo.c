#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <grp.h>
#include <linux/capability.h>

// Direct syscall wrappers since we're not using libcap
static int capset(cap_user_header_t header, const cap_user_data_t data) {
    return syscall(SYS_capset, header, data);
}

static int capget(cap_user_header_t header, cap_user_data_t data) {
    return syscall(SYS_capget, header, data);
}

void print_capabilities(const char *label) {
    struct __user_cap_header_struct header;
    struct __user_cap_data_struct data[2];

    header.version = _LINUX_CAPABILITY_VERSION_3;
    header.pid = 0;

    if (capget(&header, data) < 0) {
        perror("capget");
        return;
    }

    printf("%s:\n", label);
    printf("  Permitted:   0x%08x%08x\n", data[1].permitted, data[0].permitted);
    printf("  Effective:   0x%08x%08x\n", data[1].effective, data[0].effective);
    printf("  Inheritable: 0x%08x%08x\n", data[1].inheritable, data[0].inheritable);

    // Check if CAP_DAC_READ_SEARCH is set
    int cap_bit = CAP_DAC_READ_SEARCH;
    int word = cap_bit / 32;
    int bit = cap_bit % 32;

    if (data[word].effective & (1 << bit)) {
        printf("  CAP_DAC_READ_SEARCH is EFFECTIVE\n");
    } else {
        printf("  CAP_DAC_READ_SEARCH is NOT effective\n");
    }
}

void print_ids(const char *label) {
    printf("%s: uid=%d euid=%d gid=%d egid=%d\n",
           label, getuid(), geteuid(), getgid(), getegid());
}

int setup_capabilities() {
    struct __user_cap_header_struct header;
    struct __user_cap_data_struct data[2];

    printf("\n=== Setting up capabilities ===\n");

    // Initialize header
    header.version = _LINUX_CAPABILITY_VERSION_3;
    header.pid = 0;

    // Get current capabilities
    if (capget(&header, data) < 0) {
        perror("capget");
        return -1;
    }

    print_capabilities("Before setup");

    // Calculate bit position for CAP_DAC_READ_SEARCH
    int cap_bit = CAP_DAC_READ_SEARCH;
    int word = cap_bit / 32;  // Which 32-bit word (0 or 1)
    int bit = cap_bit % 32;   // Which bit in that word

    // Set CAP_DAC_READ_SEARCH in permitted, effective, and inheritable
    data[word].permitted |= (1 << bit);
    data[word].effective |= (1 << bit);
    data[word].inheritable |= (1 << bit);

    // Apply the capability set
    if (capset(&header, data) < 0) {
        perror("capset");
        return -1;
    }

    print_capabilities("After capset");

    // Enable ambient capabilities (survives UID changes)
    // PR_CAP_AMBIENT = 47, PR_CAP_AMBIENT_RAISE = 2
    if (prctl(47, 2, CAP_DAC_READ_SEARCH, 0, 0) < 0) {
        perror("prctl PR_CAP_AMBIENT_RAISE");
        printf("Warning: Ambient capabilities not supported (kernel < 4.3?)\n");
    } else {
        printf("Ambient capabilities set successfully\n");
    }

    // Keep capabilities across setuid
    if (prctl(PR_SET_KEEPCAPS, 1, 0, 0, 0) < 0) {
        perror("prctl PR_SET_KEEPCAPS");
        return -1;
    }

    printf("PR_SET_KEEPCAPS enabled\n");

    return 0;
}

int drop_privileges(uid_t target_uid, gid_t target_gid) {
    struct __user_cap_header_struct header;
    struct __user_cap_data_struct data[2];

    printf("\n=== Dropping privileges ===\n");
    print_ids("Before drop");

    // Set GID first (must be done while still root)
    if (setgid(target_gid) < 0) {
        perror("setgid");
        return -1;
    }

    // Drop supplementary groups
    if (setgroups(0, NULL) < 0) {
        perror("setgroups");
        return -1;
    }

    // Set UID
    if (setuid(target_uid) < 0) {
        perror("setuid");
        return -1;
    }

    print_ids("After drop");
    print_capabilities("After UID change (before re-apply)");

    // Re-apply capabilities after UID change
    header.version = _LINUX_CAPABILITY_VERSION_3;
    header.pid = 0;

    // Get current capabilities
    if (capget(&header, data) < 0) {
        perror("capget after setuid");
        return -1;
    }

    // Re-enable CAP_DAC_READ_SEARCH in effective set
    int cap_bit = CAP_DAC_READ_SEARCH;
    int word = cap_bit / 32;
    int bit = cap_bit % 32;

    data[word].effective |= (1 << bit);

    if (capset(&header, data) < 0) {
        perror("capset after setuid");
        return -1;
    }

    print_capabilities("Final capabilities");

    return 0;
}

void test_file_access(const char *path) {
    printf("\n=== Testing access to: %s ===\n", path);

    struct stat st;
    if (stat(path, &st) < 0) {
        printf("stat() failed: %s\n", strerror(errno));
    } else {
        printf("stat() succeeded! Mode: %o, Owner: %d:%d\n",
               st.st_mode & 0777, st.st_uid, st.st_gid);
    }

    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        printf("open() failed: %s\n", strerror(errno));
    } else {
        printf("open() succeeded! fd=%d\n", fd);

        char buf[128];
        ssize_t n = read(fd, buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = '\0';
            printf("Read %zd bytes: %.50s%s\n", n, buf, n > 50 ? "..." : "");
        }
        close(fd);
    }
}

int main(int argc, char *argv[]) {
    uid_t target_uid = 1000;
    gid_t target_gid = 1000;

    if (argc > 1) {
        target_uid = atoi(argv[1]);
    }
    if (argc > 2) {
        target_gid = atoi(argv[2]);
    }

    printf("CAP_DAC_READ_SEARCH Test Program (without libcap)\n");
    printf("=================================================\n");

    if (geteuid() != 0) {
        fprintf(stderr, "Error: This program must be run as root\n");
        fprintf(stderr, "Usage: sudo %s [target_uid] [target_gid]\n", argv[0]);
        return 1;
    }

    print_ids("Initial state");
    print_capabilities("Initial capabilities");

    if (setup_capabilities() != 0) {
        fprintf(stderr, "Failed to setup capabilities\n");
        return 1;
    }

    if (drop_privileges(target_uid, target_gid) != 0) {
        fprintf(stderr, "Failed to drop privileges\n");
        return 1;
    }

    printf("\n=== Testing File Access ===\n");
    printf("Now running as uid=%d with CAP_DAC_READ_SEARCH\n", getuid());

    // Test accessing a root-owned file
    printf("\nTrying to access /etc/shadow (normally requires root):\n");
    test_file_access("/etc/shadow");

    // Test accessing /root (usually 0700)
    printf("\nTrying to access /root directory:\n");
    test_file_access("/root");

    // Provide instructions for custom test
    printf("\n=== Custom Test Instructions ===\n");
    printf("To test with your own restricted directory:\n");
    printf("1. Create a test: sudo mkdir -p /tmp/test_restricted\n");
    printf("2. Restrict it: sudo chmod 700 /tmp/test_restricted\n");
    printf("3. Add a file: sudo sh -c 'echo test > /tmp/test_restricted/file.txt'\n");
    printf("4. The program should be able to read it despite permissions\n");

    if (argc > 3) {
        printf("\nTesting custom path: %s\n", argv[3]);
        test_file_access(argv[3]);
    }

    return 0;
}

/*
 * Compilation (no libcap needed):
 * gcc -o cap_test cap_test.c
 *
 * Usage:
 * sudo ./cap_test [target_uid] [target_gid] [optional_test_path]
 *
 * Example:
 * sudo ./cap_test 1000 1000
 * sudo ./cap_test 1000 1000 /tmp/test_restricted/file.txt
 */
