/**
 * motorctl - I2C utility for STM8S103 motor controller
 * 
 * This utility communicates with the STM8S103 motor controller over I2C
 * to control focus and zoom motors.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <stdint.h>
#include <getopt.h>

#define DEFAULT_BUS 7
#define DEFAULT_ADDR 0x60

/* Slave address used by the raw I2C_RDWR transfers. Set from -a in main(). */
static int g_slave_addr = DEFAULT_ADDR;

// Motor IDs
#define MOTOR_FOCUS 0x00
#define MOTOR_ZOOM 0x01

// Command IDs
#define CMD_MOVE 0x01
#define CMD_SET_SPEED 0x02
#define CMD_STOP 0x03
#define CMD_GET_POSITION 0x04
#define CMD_GET_STATUS 0x05
#define CMD_RESET 0x06
#define CMD_GET_VERSION 0xFF

// Status codes
#define STATUS_IDLE 0x00
#define STATUS_RUNNING 0x01
#define STATUS_ERROR 0x02

// Error handling
#define MAX_SPEED 5000

// Function prototypes
void usage(const char *program_name);
int open_i2c_device(int bus, int addr);
int send_command(int fd, uint8_t cmd, uint8_t motor, const uint8_t *write_data, int write_len, uint8_t *read_data, int read_len);
int send_command_with_read(int fd, uint8_t cmd, uint8_t motor, const uint8_t *write_data, int write_len, uint8_t *read_data, int read_len);
int set_speed(int fd, uint8_t motor, uint16_t speed);
int move_motor(int fd, uint8_t motor, int16_t steps);
int get_position(int fd, uint8_t motor, int32_t *position);
int get_status(int fd, uint8_t motor, uint8_t *status);
int reset_controller(int fd);
int get_version(int fd, uint8_t *version);

int main(int argc, char *argv[]) {
    int bus = DEFAULT_BUS;
    int addr = DEFAULT_ADDR;
    int fd;
    int opt;
    int ret = 0;
    int a;
    
    // Parse command line options. The leading '+' puts getopt into POSIX mode
    // so it stops at the first non-option argument, which lets negative values
    // such as "-200" reach the command arguments instead of being parsed as
    // options.
    while ((opt = getopt(argc, argv, "+b:a:h")) != -1) {
        switch (opt) {
            case 'b':
                bus = atoi(optarg);
                break;
            case 'a':
                addr = strtol(optarg, NULL, 16);
                break;
            case 'h':
                usage(argv[0]);
                return 0;
            default:
                usage(argv[0]);
                return 1;
        }
    }
    a = optind;
    g_slave_addr = addr;
    
    // Open I2C device
    fd = open_i2c_device(bus, addr);
    if (fd < 0) {
        fprintf(stderr, "Failed to open I2C device: %s\n", strerror(errno));
        return 1;
    }
    
    // Handle commands. All command arguments are read relative to optind so
    // that leading -b/-a options do not shift them.
    if (argc - a < 1) {
        usage(argv[0]);
        ret = 1;
    } else if (strcmp(argv[a], "version") == 0) {
        uint8_t version[3];
        if (get_version(fd, version) == 0) {
            printf("Firmware version: %d.%d.%d\n", version[0], version[1], version[2]);
        } else {
            fprintf(stderr, "Failed to get version\n");
            ret = 1;
        }
    } else if (strcmp(argv[a], "speed") == 0) {
        if (argc - a < 3) {
            fprintf(stderr, "Usage: %s speed <motor> <speed>\n", argv[0]);
            ret = 1;
        } else {
            uint8_t motor = (strcmp(argv[a+1], "focus") == 0) ? MOTOR_FOCUS : 
                          (strcmp(argv[a+1], "zoom") == 0) ? MOTOR_ZOOM : 0xFF;
            uint16_t speed = atoi(argv[a+2]);
            
            if (motor == 0xFF) {
                fprintf(stderr, "Invalid motor specified\n");
                ret = 1;
            } else if (speed > MAX_SPEED) {
                fprintf(stderr, "Speed must be <= %d\n", MAX_SPEED);
                ret = 1;
            } else {
                if (set_speed(fd, motor, speed) == 0) {
                    printf("Speed set to %d for %s motor\n", speed, (motor == MOTOR_FOCUS) ? "focus" : "zoom");
                } else {
                    fprintf(stderr, "Failed to set speed\n");
                    ret = 1;
                }
            }
        }
    } else if (strcmp(argv[a], "move") == 0) {
        if (argc - a < 3) {
            fprintf(stderr, "Usage: %s move <motor> <steps>\n", argv[0]);
            ret = 1;
        } else {
            uint8_t motor = (strcmp(argv[a+1], "focus") == 0) ? MOTOR_FOCUS : 
                         (strcmp(argv[a+1], "zoom") == 0) ? MOTOR_ZOOM : 0xFF;
            int16_t steps = atoi(argv[a+2]);
            
            if (motor == 0xFF) {
                fprintf(stderr, "Invalid motor specified\n");
                ret = 1;
            } else {
                if (move_motor(fd, motor, steps) == 0) {
                    printf("Moved %s motor by %d steps\n", (motor == MOTOR_FOCUS) ? "focus" : "zoom", steps);
                } else {
                    fprintf(stderr, "Failed to move motor\n");
                    ret = 1;
                }
            }
        }
    } else if (strcmp(argv[a], "position") == 0) {
        if (argc - a < 2) {
            fprintf(stderr, "Usage: %s position <motor>\n", argv[0]);
            ret = 1;
        } else {
            uint8_t motor = (strcmp(argv[a+1], "focus") == 0) ? MOTOR_FOCUS : 
                            (strcmp(argv[a+1], "zoom") == 0) ? MOTOR_ZOOM : 0xFF;
            int32_t position;
            
            if (motor == 0xFF) {
                fprintf(stderr, "Invalid motor specified\n");
                ret = 1;
            } else {
                if (get_position(fd, motor, &position) == 0) {
                    printf("%s motor position: %d\n", (motor == MOTOR_FOCUS) ? "focus" : "zoom", position);
                } else {
                    fprintf(stderr, "Failed to get position\n");
                    ret = 1;
                }
            }
        }
    } else if (strcmp(argv[a], "status") == 0) {
        if (argc - a < 2) {
            fprintf(stderr, "Usage: %s status <motor>\n", argv[0]);
            ret = 1;
        } else {
            uint8_t motor = (strcmp(argv[a+1], "focus") == 0) ? MOTOR_FOCUS : 
                       (strcmp(argv[a+1], "zoom") == 0) ? MOTOR_ZOOM : 0xFF;
            uint8_t status;
            
            if (motor == 0xFF) {
                fprintf(stderr, "Invalid motor specified\n");
                ret = 1;
            } else {
                if (get_status(fd, motor, &status) == 0) {
                    const char* status_str;
                    switch (status) {
                        case STATUS_IDLE: status_str = "IDLE"; break;
                        case STATUS_RUNNING: status_str = "RUNNING"; break;
                        case STATUS_ERROR: status_str = "ERROR"; break;
                        default: status_str = "UNKNOWN"; break;
                    }
                    printf("%s motor status: %s (%d)\n", (motor == MOTOR_FOCUS) ? "focus" : "zoom", status_str, status);
                } else {
                    fprintf(stderr, "Failed to get status\n");
                    ret = 1;
                }
            }
        }
    } else if (strcmp(argv[a], "stop") == 0) {
        if (argc - a < 2) {
            fprintf(stderr, "Usage: %s stop <motor>\n", argv[0]);
            ret = 1;
        } else {
            uint8_t motor = (strcmp(argv[a+1], "focus") == 0) ? MOTOR_FOCUS : 
                         (strcmp(argv[a+1], "zoom") == 0) ? MOTOR_ZOOM : 0xFF;
            
            if (motor == 0xFF) {
                fprintf(stderr, "Invalid motor specified\n");
                ret = 1;
            } else {
                if (send_command(fd, CMD_STOP, motor, NULL, 0, NULL, 0) == 0) {
                    printf("Stopped %s motor\n", (motor == MOTOR_FOCUS) ? "focus" : "zoom");
                } else {
                    fprintf(stderr, "Failed to stop motor\n");
                    ret = 1;
                }
            }
        }
    } else if (strcmp(argv[a], "reset") == 0) {
        if (reset_controller(fd) == 0) {
            printf("Controller reset successful\n");
        } else {
            fprintf(stderr, "Failed to reset controller\n");
            ret = 1;
        }
    } else {
        fprintf(stderr, "Unknown command: %s\n", argv[a]);
        usage(argv[0]);
        ret = 1;
    }
    
    close(fd);
    return ret;
}

void usage(const char *program_name) {
    fprintf(stderr, "Usage: %s [OPTIONS] COMMAND [ARGS]\n", program_name);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -b BUS    I2C bus number (default: %d)\n", DEFAULT_BUS);
    fprintf(stderr, "  -a ADDR  I2C address (default: 0x%02X)\n", DEFAULT_ADDR);
    fprintf(stderr, "  -h        Show this help message\n");
    fprintf(stderr, "\nCommands:\n");
    fprintf(stderr, "  version        Get firmware version\n");
    fprintf(stderr, "  speed <motor> <speed>  Set motor speed\n");
    fprintf(stderr, "  move <motor> <steps>  Move motor by steps\n");
    fprintf(stderr, "  position <motor>    Get motor position\n");
    fprintf(stderr, "  status <motor>  Get motor status\n");
    fprintf(stderr, "  stop <motor>    Stop motor\n");
    fprintf(stderr, "  reset          Reset controller\n");
    fprintf(stderr, "\nMotor names: focus, zoom\n");
}

int open_i2c_device(int bus, int addr) {
    char filename[20];
    int fd;
    
    snprintf(filename, sizeof(filename), "/dev/i2c-%d", bus);
    fd = open(filename, O_RDWR);
    if (fd < 0) {
        return -1;
    }
    
    if (ioctl(fd, I2C_SLAVE, addr) < 0) {
        close(fd);
        return -1;
    }
    
    return fd;
}

int send_command(int fd, uint8_t cmd, uint8_t motor, const uint8_t *write_data, int write_len, uint8_t *read_data, int read_len) {
    struct i2c_msg msgs[2];
    struct i2c_rdwr_ioctl_data ioctl_data;
    uint8_t write_buf[10];
    int write_len_total = 0;
    
    // Prepare write buffer
    write_buf[write_len_total++] = cmd;
    write_buf[write_len_total++] = motor;
    
    if (write_data != NULL && write_len > 0) {
        memcpy(write_buf + write_len_total, write_data, write_len);
        write_len_total += write_len;
    }
    
    // For commands that require read, use I2C_RDWR
    if (read_len > 0) {
        return send_command_with_read(fd, cmd, motor, write_data, write_len, read_data, read_len);
    }
    
    // Write-only command
    msgs[0].addr = g_slave_addr; // Slave address
    msgs[0].flags = 0;   // Write
    msgs[0].len = write_len_total;
    msgs[0].buf = write_buf;
    
    ioctl_data.msgs = msgs;
    ioctl_data.nmsgs = 1;
    
    if (ioctl(fd, I2C_RDWR, &ioctl_data) < 0) {
        return -1;
    }
    
    return 0;
}

int send_command_with_read(int fd, uint8_t cmd, uint8_t motor, const uint8_t *write_data, int write_len, uint8_t *read_data, int read_len) {
    struct i2c_msg msgs[2];
    struct i2c_rdwr_ioctl_data ioctl_data;
    uint8_t write_buf[10];
    int write_len_total = 0;
    
    // Prepare write buffer
    write_buf[write_len_total++] = cmd;
    write_buf[write_len_total++] = motor;
    
    if (write_data != NULL && write_len > 0) {
        memcpy(write_buf + write_len_total, write_data, write_len);
        write_len_total += write_len;
    }
    
    // Write then read
    msgs[0].addr = g_slave_addr;
    msgs[0].flags = 0; // Write
    msgs[0].len = write_len_total;
    msgs[0].buf = write_buf;
    
    msgs[1].addr = g_slave_addr;
    msgs[1].flags = I2C_M_RD; // Read
    msgs[1].len = read_len;
    msgs[1].buf = read_data;
    
    ioctl_data.msgs = msgs;
    ioctl_data.nmsgs = 2;
    
    if (ioctl(fd, I2C_RDWR, &ioctl_data) < 0) {
        return -1;
    }
    
    return 0;
}

int set_speed(int fd, uint8_t motor, uint16_t speed) {
    uint8_t speed_data[2];
    speed_data[0] = (speed >> 8) & 0xFF; // MSB
    speed_data[1] = speed & 0xFF;          // LSB
    
    return send_command(fd, CMD_SET_SPEED, motor, speed_data, 2, NULL, 0);
}

int move_motor(int fd, uint8_t motor, int16_t steps) {
    uint8_t steps_data[2];
    steps_data[0] = (steps >> 8) & 0xFF; // MSB
    steps_data[1] = steps & 0xFF;        // LSB
    
    return send_command(fd, CMD_MOVE, motor, steps_data, 2, NULL, 0);
}

int get_position(int fd, uint8_t motor, int32_t *position) {
    uint8_t read_data[4];
    if (send_command_with_read(fd, CMD_GET_POSITION, motor, NULL, 0, read_data, 4) != 0) {
        return -1;
    }
    
    *position = (read_data[0] << 24) | (read_data[1] << 16) | (read_data[2] << 8) | read_data[3];
    return 0;
}

int get_status(int fd, uint8_t motor, uint8_t *status) {
    uint8_t read_data[1];
    if (send_command_with_read(fd, CMD_GET_STATUS, motor, NULL, 0, read_data, 1) != 0) {
        return -1;
    }
    
    *status = read_data[0];
    return 0;
}

int reset_controller(int fd) {
    return send_command(fd, CMD_RESET, 0, NULL, 0, NULL, 0);
}

int get_version(int fd, uint8_t *version) {
    return send_command_with_read(fd, CMD_GET_VERSION, 0, NULL, 0, version, 3);
}