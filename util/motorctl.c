/*
 * oko-motor - host utility for the OKO STM8S103F3P6 focus/zoom controller.
 *
 * Bus 3 and address 0x60 are the OKO defaults.  The utility understands the
 * application protocol (0x01..0x07, 0xFF) and the I2C bootloader
 * (0x10..0x14, see docs/BOOTLOADER.md).
 *
 * Exit codes: 0 success, 1 transfer/flash error, 2 usage or blocked.
 */
#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <unistd.h>

#include "i2c_host.h"
#include "oko_boot.h"
#include "oko_crc.h"
#include "i2c_proto.h"

#define IMAGE_BASE   OKO_HDR_BASE
#define IMAGE_SIZE   (OKO_APP_END - OKO_HDR_BASE)
#define MAX_IMAGE    IMAGE_SIZE

#define APP_TIMEOUT_US  2000u
#define BOOT_TIMEOUT_US 5000u
#define APP_RETRIES     3u
#define BOOT_RETRIES    4u

/* Erase/write/finish are executed synchronously by the polling bootloader,
 * so the status read must wait for the operation instead of racing it. */
#define ERASE_SETTLE_US  500000u
#define WRITE_SETTLE_US   40000u
#define FINISH_SETTLE_US 150000u

static const char *device_path = "/dev/i2c-3";
static uint8_t slave_address = 0x60;
static int lock_fd = -1;

static uint8_t fw_image[MAX_IMAGE] __attribute__((aligned(4)));
static struct oko_app_header fw_header;

static void usage(void)
{
    fprintf(stderr,
        "usage: oko-motor [-b BUS] [-a ADDR] [-d DEV] COMMAND [ARGS]\n"
        "\n"
        "  info                 application version, status and positions\n"
        "  version              application version (3 bytes)\n"
        "  status MOTOR         motor status (0 focus, 1 zoom)\n"
        "  position MOTOR       motor position in steps\n"
        "  speed MOTOR VALUE    set speed in steps/s (1..5000)\n"
        "  move MOTOR STEPS     relative move\n"
        "  stop MOTOR           stop one motor\n"
        "  reset                software reset of the application\n"
        "  enter-boot           ask the application for the bootloader\n"
        "  boot-reset           leave the bootloader and start the application\n"
        "  check FILE [X.Y.Z]   validate FILE offline (no I2C), exit 0/2;\n"
        "                       with X.Y.Z also compare the header version\n"
        "  flash FILE           flash app.ihx/.hex or app.bin (with header)\n"
        "  verify FILE          compare the flashed CRC with FILE\n"
        "\n"
        "FILE is the image produced by tools/mkapp.py: a 16-byte header at\n"
        "0x8A00 followed by the application at 0x8A10.\n");
}

static int lock_take(int exclusive)
{
    const char *path = getenv("OKO_MOTOR_LOCK");
    int flags = exclusive ? LOCK_EX : LOCK_SH;

    if (!path || !*path)
        path = "/run/oko-motor.lock";
    lock_fd = open(path, O_RDWR | O_CREAT, 0666);
    if (lock_fd < 0 && errno == EACCES)
    {
        path = "/tmp/oko-motor.lock";
        lock_fd = open(path, O_RDWR | O_CREAT, 0666);
    }
    if (lock_fd < 0)
        return 0;              /* not fatal: the bus is still usable */
    if (flock(lock_fd, flags) < 0)
    {
        close(lock_fd);
        lock_fd = -1;
    }
    return 0;
}

static void lock_release(void)
{
    if (lock_fd >= 0)
    {
        flock(lock_fd, LOCK_UN);
        close(lock_fd);
        lock_fd = -1;
    }
}

static int parse_motor(const char *text, uint8_t *motor)
{
    long v = strtol(text, NULL, 0);
    if (v != 0 && v != 1)
        return -1;
    *motor = (uint8_t)v;
    return 0;
}

/* ---------------------------------------------------------------- app I2C */

static int app_read(int fd, uint8_t cmd, uint8_t *out, size_t len)
{
    uint8_t req[2];

    req[0] = cmd;
    req[1] = 0x00;
    return i2c_host_transfer(fd, req, 1, out, len, APP_TIMEOUT_US,
                             APP_RETRIES) < 0 ? -1 : 0;
}

static int app_write(int fd, const uint8_t *buf, size_t len)
{
    return i2c_host_write(fd, buf, len);
}

/* Role and version in one read: [role][major][minor][patch]. */
static int read_state(int fd, uint8_t *role, uint8_t version[3])
{
    uint8_t resp[4];

    if (app_read(fd, OKO_CMD_GET_STATE, resp, sizeof(resp)) < 0)
        return -1;
    if (role)
        *role = resp[0];
    if (version)
    {
        version[0] = resp[1];
        version[1] = resp[2];
        version[2] = resp[3];
    }
    return 0;
}

static int read_app_version(int fd, uint8_t version[3])
{
    uint8_t role = 0xFF;

    if (read_state(fd, &role, version) < 0 || role != OKO_ROLE_APP)
        return -1;
    return 0;
}

static int read_motor_status(int fd, uint8_t motor, uint8_t *status)
{
    uint8_t req[2] = { I2C_CMD_GET_STATUS, motor };
    return i2c_host_transfer(fd, req, 2, status, 1, APP_TIMEOUT_US,
                             APP_RETRIES) < 0 ? -1 : 0;
}

static int read_motor_position(int fd, uint8_t motor, int32_t *position)
{
    uint8_t req[2] = { I2C_CMD_GET_POS, motor };
    uint8_t raw[4];

    if (i2c_host_transfer(fd, req, 2, raw, 4, APP_TIMEOUT_US, APP_RETRIES) < 0)
        return -1;
    *position = (int32_t)oko_get_be32(raw);
    return 0;
}

/* --------------------------------------------------------------- boot I2C */

static int boot_info(int fd, uint8_t info[8])
{
    uint8_t cmd = BOOT_CMD_INFO;
    return i2c_host_transfer(fd, &cmd, 1, info, 8, BOOT_TIMEOUT_US,
                             BOOT_RETRIES) < 0 ? -1 : 0;
}

static int boot_wait(int fd)
{
    uint8_t info[8];
    int attempt;

    for (attempt = 0; attempt < 20; attempt++)
    {
        if (boot_info(fd, info) == 0 && info[6] == BOOT_WRITE_MAX &&
            info[7] == BOOT_STATE_BOOT)
            return 0;
        usleep(100000);
    }
    return -1;
}

static int boot_status_command(int fd, const uint8_t *buf, size_t len,
                               uint8_t *status, unsigned settle_us)
{
    if (i2c_host_transfer(fd, buf, len, status, 1, settle_us,
                          BOOT_RETRIES) < 0)
        return -1;
    return 0;
}

static int boot_erase(int fd, uint16_t start, uint16_t length)
{
    uint8_t frame[9];
    uint8_t status = 0xFF;
    size_t len = oko_boot_frame_erase(frame, start, length);

    if (len == 0)
        return -1;
    if (boot_status_command(fd, frame, len, &status, ERASE_SETTLE_US) < 0)
        return -1;
    if (status != BOOT_ST_OK)
    {
        fprintf(stderr, "boot erase failed: status %u\n", status);
        return -1;
    }
    return 0;
}

static int boot_write(int fd, uint16_t address, const uint8_t *data,
                      uint8_t length)
{
    uint8_t frame[8 + BOOT_WRITE_MAX];
    uint8_t status = 0xFF;
    size_t len = oko_boot_frame_write(frame, address, data, length);

    if (len == 0)
        return -1;
    if (boot_status_command(fd, frame, len, &status, WRITE_SETTLE_US) < 0)
        return -1;
    if (status != BOOT_ST_OK)
    {
        fprintf(stderr, "boot write 0x%04X failed: status %u\n", address,
                status);
        return -1;
    }
    return 0;
}

static int boot_finish(int fd, uint16_t length, uint32_t crc)
{
    uint8_t frame[9];
    uint8_t status = 0xFF;
    size_t len = oko_boot_frame_finish(frame, length, crc);

    if (len == 0)
        return -1;
    if (boot_status_command(fd, frame, len, &status, FINISH_SETTLE_US) < 0)
        return -1;
    if (status != BOOT_ST_OK)
    {
        fprintf(stderr, "boot finish failed: status %u\n", status);
        return -1;
    }
    return 0;
}

static int boot_reset(int fd)
{
    uint8_t cmd = BOOT_CMD_RESET;

    i2c_host_write(fd, &cmd, 1);
    usleep(200000);
    return 0;
}

/* --------------------------------------------------------- firmware files */

static int hex_nibble(int c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int load_hex(const char *path, uint8_t *image, size_t size)
{
    FILE *file = fopen(path, "r");
    char line[600];

    if (!file)
    {
        fprintf(stderr, "cannot open %s: %s\n", path, strerror(errno));
        return -1;
    }

    while (fgets(line, sizeof(line), file))
    {
        uint8_t raw[300];
        size_t len, i;
        uint16_t address;
        int type;

        if (line[0] != ':')
            continue;
        len = strlen(line);
        while (len && (line[len - 1] == '\n' || line[len - 1] == '\r'))
            line[--len] = '\0';
        if (len < 11)
            break;
        for (i = 0; i < (len - 1) / 2 && i < sizeof(raw); i++)
        {
            int hi = hex_nibble(line[1 + 2 * i]);
            int lo = hex_nibble(line[2 + 2 * i]);
            if (hi < 0 || lo < 0)
            {
                fclose(file);
                fprintf(stderr, "%s: bad hex digit\n", path);
                return -1;
            }
            raw[i] = (uint8_t)((hi << 4) | lo);
        }
        if (raw[0] + 5 > (int)i)
            continue;
        {
            uint8_t sum = 0;
            for (size_t k = 0; k < (size_t)raw[0] + 5; k++)
                sum = (uint8_t)(sum + raw[k]);
            if (sum != 0)
            {
                fclose(file);
                fprintf(stderr, "%s: Intel HEX checksum error\n", path);
                return -1;
            }
        }
        address = (uint16_t)(((uint16_t)raw[1] << 8) | raw[2]);
        type = raw[3];
        if (type == 0x01)
            break;
        if (type != 0x00)
            continue;
        if (address < IMAGE_BASE || (size_t)(address - IMAGE_BASE) + raw[0] > size)
        {
            fclose(file);
            fprintf(stderr, "%s: record 0x%04X outside the application image\n",
                    path, address);
            return -1;
        }
        memcpy(image + (address - IMAGE_BASE), &raw[4], raw[0]);
    }
    fclose(file);
    return 0;
}

static int load_firmware(const char *path)
{
    const char *ext = strrchr(path, '.');
    uint16_t length;
    uint32_t crc;
    size_t n;

    memset(fw_image, 0xFF, sizeof(fw_image));

    if (ext && (!strcasecmp(ext, ".ihx") || !strcasecmp(ext, ".hex")))
    {
        if (load_hex(path, fw_image, sizeof(fw_image)))
            return -1;
    }
    else
    {
        FILE *file = fopen(path, "rb");
        long size;

        if (!file)
        {
            fprintf(stderr, "cannot open %s: %s\n", path, strerror(errno));
            return -1;
        }
        if (fseek(file, 0, SEEK_END) != 0 || (size = ftell(file)) < 0 ||
            fseek(file, 0, SEEK_SET) != 0)
        {
            fclose(file);
            fprintf(stderr, "%s: cannot determine the file size\n", path);
            return -1;
        }
        if (size < (long)OKO_HDR_SIZE || (size_t)size > sizeof(fw_image))
        {
            fclose(file);
            fprintf(stderr, "%s: not an OKO application image\n", path);
            return -1;
        }
        n = fread(fw_image, 1, (size_t)size, file);
        fclose(file);
        if (n != (size_t)size)
        {
            fprintf(stderr, "%s: short read\n", path);
            return -1;
        }
    }

    if (oko_header_unpack(fw_image, &fw_header) ||
        !oko_header_valid(&fw_header))
    {
        fprintf(stderr, "%s: invalid OKO8 header\n", path);
        return -1;
    }
    length = (uint16_t)fw_header.length;
    crc = oko_crc32(fw_image + (OKO_APP_BASE - IMAGE_BASE), length);
    if (crc != fw_header.crc32)
    {
        fprintf(stderr, "%s: CRC32 mismatch (file 0x%08X, header 0x%08X)\n",
                path, crc, fw_header.crc32);
        return -1;
    }
    return 0;
}

/* Probes the application and enters the bootloader. */
static int enter_boot(int fd)
{
    uint8_t cmd = I2C_CMD_ENTER_BOOT;
    uint8_t role = 0xFF;

    /* The role query is a normal application command, so a running app is
     * never sent 0x10 or any other bootloader-only code. */
    if (read_state(fd, &role, NULL) < 0)
    {
        fprintf(stderr, "no response at address 0x%02X\n", slave_address);
        return -1;
    }
    if (role == OKO_ROLE_BOOT)
        return 0;
    if (role != OKO_ROLE_APP)
    {
        fprintf(stderr, "unknown device role 0x%02X\n", role);
        return -1;
    }

    if (app_write(fd, &cmd, 1) < 0)
        return -1;
    /* The application stores the boot magic and stops reloading the IWDG. */
    usleep(300000);
    if (boot_wait(fd) == 0)
        return 0;
    fprintf(stderr, "the device did not enter the bootloader\n");
    return -1;
}

static int flash_firmware(int fd, int verify_only)
{
    uint16_t length = (uint16_t)fw_header.length;
    uint16_t total = (uint16_t)(OKO_HDR_SIZE + length);
    uint16_t erase = (uint16_t)((total + OKO_BLOCK_SIZE - 1) &
                                ~(OKO_BLOCK_SIZE - 1));
    uint16_t address;
    uint8_t info[8];

    if (enter_boot(fd))
        return 1;

    if (boot_info(fd, info) < 0)
    {
        fprintf(stderr, "bootloader does not answer BOOT_INFO\n");
        return 1;
    }
    printf("boot %u.%u.%u, app %u.%u.%u, block %u\n",
           info[0], info[1], info[2], info[3], info[4], info[5], info[6]);

    if (verify_only)
    {
        printf("PROGRESS 50\n");
        if (boot_finish(fd, length, fw_header.crc32))
            return 1;
        boot_reset(fd);
        printf("verified: CRC32 0x%08X\n", fw_header.crc32);
        return 0;
    }

    if (boot_erase(fd, IMAGE_BASE, erase))
        return 1;
    printf("PROGRESS 5\n");

    for (address = 0; address < total; address += BOOT_WRITE_MAX)
    {
        uint16_t remaining = (uint16_t)(total - address);
        uint8_t block = (uint8_t)(remaining > BOOT_WRITE_MAX ?
                                  BOOT_WRITE_MAX : remaining);
        uint8_t i;

        /* BOOT_WRITE requires a whole number of 4-byte words. */
        block = (uint8_t)((block + 3) & ~3);
        if (address + block > sizeof(fw_image))
            break;

        if (boot_write(fd, (uint16_t)(IMAGE_BASE + address),
                       fw_image + address, block))
            return 1;

        for (i = 0; i < block; i++)
            if (address + i >= total)
                fw_image[address + i] = 0xFF;

        printf("PROGRESS %u\n",
               (unsigned)(5u + (unsigned)(address + block) * 90u / total));
        fflush(stdout);
    }

    if (boot_finish(fd, length, fw_header.crc32))
        return 1;
    boot_reset(fd);
    printf("PROGRESS 100\n");
    return 0;
}

/* --------------------------------------------------------------- commands */

static int cmd_info(int fd)
{
    uint8_t version[3] = {0, 0, 0};
    uint8_t boot[8];
    uint8_t role = 0xFF;
    int motor;

    /* 0x08 reports the role, so a running application never receives the
     * bootloader-only 0x10 command. */
    if (read_state(fd, &role, version) == 0 && role == OKO_ROLE_BOOT)
    {
        if (boot_info(fd, boot) == 0 && boot[6] == BOOT_WRITE_MAX &&
            boot[7] == BOOT_STATE_BOOT)
        {
            printf("bootloader: %u.%u.%u, application header: %u.%u.%u\n",
                   boot[0], boot[1], boot[2], boot[3], boot[4], boot[5]);
            return 0;
        }
        fprintf(stderr, "bootloader role reported, but BOOT_INFO failed\n");
        return 1;
    }

    if (role == OKO_ROLE_APP)
    {
        printf("version: %u.%u.%u\n", version[0], version[1], version[2]);
        for (motor = 0; motor < 2; motor++)
        {
            uint8_t status = 0xFF;
            int32_t position = 0;

            if (read_motor_status(fd, (uint8_t)motor, &status) == 0 &&
                read_motor_position(fd, (uint8_t)motor, &position) == 0)
                printf("%s: status %u, position %ld\n",
                       motor == 0 ? "focus" : "zoom", status, (long)position);
            else
                printf("%s: no response\n", motor == 0 ? "focus" : "zoom");
        }
        return 0;
    }

    fprintf(stderr, "no application response at 0x%02X\n", slave_address);
    return 1;
}

static int cmd_version(int fd)
{
    uint8_t version[3];
    uint8_t role = 0xFF;

    if (read_state(fd, &role, version) < 0)
    {
        fprintf(stderr, "no response at 0x%02X\n", slave_address);
        return 1;
    }
    if (role == OKO_ROLE_BOOT)
    {
        fprintf(stderr, "the device is in the bootloader; use info or "
                        "boot-reset\n");
        return 1;
    }
    if (role != OKO_ROLE_APP)
    {
        fprintf(stderr, "unknown device role 0x%02X\n", role);
        return 1;
    }
    printf("%u.%u.%u\n", version[0], version[1], version[2]);
    return 0;
}

static int cmd_status(int fd, uint8_t motor)
{
    uint8_t status = 0xFF;

    if (read_motor_status(fd, motor, &status) < 0)
    {
        fprintf(stderr, "no application response\n");
        return 1;
    }
    printf("%u\n", status);
    return 0;
}

static int cmd_position(int fd, uint8_t motor)
{
    int32_t position;

    if (read_motor_position(fd, motor, &position) < 0)
    {
        fprintf(stderr, "no application response\n");
        return 1;
    }
    printf("%ld\n", (long)position);
    return 0;
}

static int app_u16_command(int fd, uint8_t cmd, uint8_t motor, uint16_t value)
{
    uint8_t frame[4];

    frame[0] = cmd;
    frame[1] = motor;
    frame[2] = (uint8_t)(value >> 8);
    frame[3] = (uint8_t)value;
    return app_write(fd, frame, sizeof(frame));
}

static int cmd_speed(int fd, uint8_t motor, uint16_t speed)
{
    if (speed < 1 || speed > 5000)
    {
        fprintf(stderr, "speed must be 1..5000\n");
        return 2;
    }
    return app_u16_command(fd, I2C_CMD_SET_SPEED, motor, speed) ? 1 : 0;
}

static int cmd_move(int fd, uint8_t motor, int16_t steps)
{
    return app_u16_command(fd, I2C_CMD_MOVE, motor, (uint16_t)steps) ? 1 : 0;
}

static int cmd_stop(int fd, uint8_t motor)
{
    uint8_t frame[2] = { I2C_CMD_STOP, motor };
    return app_write(fd, frame, sizeof(frame)) ? 1 : 0;
}

static int cmd_reset(int fd)
{
    uint8_t frame[1] = { I2C_CMD_RESET };
    return app_write(fd, frame, 1) ? 1 : 0;
}

/* BOOT_RESET (0x14) is the only command that leaves the bootloader; the
 * application-level reset (0x06) is ignored there. */
static int cmd_boot_reset(int fd)
{
    uint8_t cmd = BOOT_CMD_RESET;
    uint8_t version[3];

    if (i2c_host_write(fd, &cmd, 1) < 0)
    {
        fprintf(stderr, "cannot send BOOT_RESET\n");
        return 1;
    }
    usleep(1200000);
    if (read_app_version(fd, version) == 0)
    {
        printf("app %u.%u.%u\n", version[0], version[1], version[2]);
        return 0;
    }
    fprintf(stderr, "BOOT_RESET sent, but the application did not answer\n");
    return 1;
}

static int cmd_flash(int fd, int verify_only)
{
    int rc = flash_firmware(fd, verify_only);

    if (rc == 0 && !verify_only)
    {
        uint8_t version[3];
        usleep(1200000);
        if (read_app_version(fd, version) == 0)
        {
            printf("app %u.%u.%u\n", version[0], version[1], version[2]);
            if (version[0] != fw_header.major ||
                version[1] != fw_header.minor ||
                version[2] != fw_header.patch)
                fprintf(stderr, "warning: reported version differs from file\n");
        }
        else
            fprintf(stderr, "warning: application did not start after reset\n");
    }
    return rc;
}

static int parse_addr(const char *text, uint8_t *address)
{
    char *end = NULL;
    unsigned long v = strtoul(text, &end, 0);

    if (!end || *end || v > 0x7F)
        return -1;
    *address = (uint8_t)v;
    return 0;
}

/* The WebUI derives the expected application version from the file name;
 * this parser mirrors that format strictly: three decimal bytes. */
static int parse_version_text(const char *text, uint8_t version[3])
{
    unsigned major, minor, patch;
    char tail;

    if (sscanf(text, "%u.%u.%u%c", &major, &minor, &patch, &tail) != 3)
        return -1;
    if (major > 255 || minor > 255 || patch > 255)
        return -1;
    version[0] = (uint8_t)major;
    version[1] = (uint8_t)minor;
    version[2] = (uint8_t)patch;
    return 0;
}

int main(int argc, char **argv)
{
    int opt;
    int fd;
    int rc = 0;
    const char *command;

    while ((opt = getopt(argc, argv, "+b:a:d:h")) != -1)
    {
        switch (opt)
        {
            case 'b':
            {
                static char path[32];
                snprintf(path, sizeof(path), "/dev/i2c-%s", optarg);
                device_path = path;
                break;
            }
            case 'a':
                if (parse_addr(optarg, &slave_address))
                {
                    fprintf(stderr, "invalid address: %s\n", optarg);
                    return 2;
                }
                break;
            case 'd':
                device_path = optarg;
                break;
            case 'h':
                usage();
                return 0;
            default:
                usage();
                return 2;
        }
    }

    if (optind >= argc)
    {
        usage();
        return 2;
    }
    command = argv[optind++];

    /* Offline validation: no device, no lock, no I2C. */
    if (!strcmp(command, "check"))
    {
        const char *file = (optind < argc) ? argv[optind] : NULL;
        uint8_t expected[3];

        if (!file)
        {
            usage();
            return 2;
        }
        if (load_firmware(file) != 0)
            return 2;
        if (optind + 1 < argc)
        {
            if (parse_version_text(argv[optind + 1], expected))
            {
                fprintf(stderr, "invalid version: %s (expected X.Y.Z)\n",
                        argv[optind + 1]);
                return 2;
            }
            if (expected[0] != fw_header.major ||
                expected[1] != fw_header.minor ||
                expected[2] != fw_header.patch)
            {
                fprintf(stderr,
                        "%s: header version %u.%u.%u does not match %u.%u.%u\n",
                        file, fw_header.major, fw_header.minor,
                        fw_header.patch, expected[0], expected[1],
                        expected[2]);
                return 2;
            }
        }
        return 0;
    }

    if (!strcmp(command, "info") || !strcmp(command, "version") ||
        !strcmp(command, "status") || !strcmp(command, "position"))
        lock_take(0);
    else
        lock_take(1);

    fd = i2c_host_open(device_path, slave_address);
    if (fd < 0)
    {
        lock_release();
        return 1;
    }

    if (!strcmp(command, "info"))
        rc = cmd_info(fd);
    else if (!strcmp(command, "version"))
        rc = cmd_version(fd);
    else if (!strcmp(command, "status"))
    {
        uint8_t motor;
        if (optind >= argc || parse_motor(argv[optind], &motor))
        {
            usage();
            rc = 2;
        }
        else
            rc = cmd_status(fd, motor);
    }
    else if (!strcmp(command, "position"))
    {
        uint8_t motor;
        if (optind >= argc || parse_motor(argv[optind], &motor))
        {
            usage();
            rc = 2;
        }
        else
            rc = cmd_position(fd, motor);
    }
    else if (!strcmp(command, "speed"))
    {
        uint8_t motor;
        long speed;
        if (optind + 1 >= argc || parse_motor(argv[optind], &motor))
            rc = 2;
        else
        {
            speed = strtol(argv[optind + 1], NULL, 0);
            rc = cmd_speed(fd, motor, (uint16_t)speed);
        }
    }
    else if (!strcmp(command, "move"))
    {
        uint8_t motor;
        long steps;
        if (optind + 1 >= argc || parse_motor(argv[optind], &motor))
            rc = 2;
        else
        {
            steps = strtol(argv[optind + 1], NULL, 0);
            if (steps < -32768 || steps > 32767)
            {
                fprintf(stderr, "steps must be -32768..32767\n");
                rc = 2;
            }
            else
                rc = cmd_move(fd, motor, (int16_t)steps);
        }
    }
    else if (!strcmp(command, "stop"))
    {
        uint8_t motor;
        if (optind >= argc || parse_motor(argv[optind], &motor))
            rc = 2;
        else
            rc = cmd_stop(fd, motor);
    }
    else if (!strcmp(command, "reset"))
        rc = cmd_reset(fd);
    else if (!strcmp(command, "enter-boot"))
        rc = enter_boot(fd) ? 1 : 0;
    else if (!strcmp(command, "boot-reset"))
        rc = cmd_boot_reset(fd);
    else if (!strcmp(command, "flash") || !strcmp(command, "verify"))
    {
        const char *file = (optind < argc) ? argv[optind] : NULL;
        int verify_only = !strcmp(command, "verify");

        if (!file)
            rc = 2;
        else if (load_firmware(file))
            rc = 2;
        else
            rc = cmd_flash(fd, verify_only);
    }
    else
    {
        usage();
        rc = 2;
    }

    i2c_host_close(fd);
    lock_release();
    return rc;
}