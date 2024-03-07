#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define return_defer(res) \
    do {                  \
        result = res;     \
        goto defer;       \
    } while (0)

int main(int argc, char* argv[]) {
    int result = 0;

    char* battery_directory = "/sys/class/power_supply/BAT0";
    int alarm_threshold = 10;
    int check_interval = 10;

    int opt;
    while ((opt = getopt(argc, argv, "hb:a:i:")) != -1) {
        switch (opt) {
            case 'b':
                battery_directory = strdup(optarg);
                break;
            case 'a':
                alarm_threshold = atoi(optarg);
                break;
            case 'i':
                check_interval = atoi(optarg);
                break;
            case 'h':
            default:
                fprintf(opt == 'h' ? stdout : stderr,
                        "Usage: \n"
                        "%s\n"
                        "  [-b path_to_battery_sys_dir]\n"
                        "  [-a battery_capacity_alarm_threshold]\n"
                        "  [-i check_interval_secs]\n",
                        argv[0]);
                return_defer(opt != 'h');
        }
    }

    char capacity_file_name[64] = {0};
    sprintf(capacity_file_name, "%s/%s", battery_directory, "capacity");
    char status_file_name[64] = {0};
    sprintf(status_file_name, "%s/%s", battery_directory, "status");

    FILE* capacity_file = NULL;
    capacity_file = fopen(capacity_file_name, "r");
    if (!capacity_file) {
        perror("Failed to open capacity file");
        return_defer(1);
    }
    FILE* status_file = NULL;
    status_file = fopen(status_file_name, "r");
    if (!status_file) {
        perror("Failed to open capacity file");
        return_defer(1);
    }

    char alarm_notification_id[16] = "0";

    while (1) {
        int capacity = 0;
        fscanf(capacity_file, "%d", &capacity);
        fflush(capacity_file);
        rewind(capacity_file);

        if (capacity <= alarm_threshold) {
            char status[32] = {0};
            fscanf(status_file, "%s", status);
            fflush(status_file);
            rewind(status_file);
            if (strcmp(status, "Charging") != 0) {
                int pipe_fds[2];
                pipe(pipe_fds);
                char expire_time[16] = {0};
                sprintf(expire_time, "%d", check_interval * 1000 + 100);
                char summary[32] = {0};
                sprintf(summary, "Battery low! (%d%%)", capacity);
                char* args[] = {
                    "notify-send",
                    "--app-name=batkeep",
                    "--urgency=critical",
                    "-t",
                    expire_time,
                    "-r",
                    alarm_notification_id,
                    "--print-id",
                    summary,
                    "Please plug in the charger",
                    NULL,
                };
                if (fork() == 0) {
                    close(pipe_fds[0]);
                    dup2(pipe_fds[1], STDOUT_FILENO);
                    execvp(args[0], args);
                }
                close(pipe_fds[1]);
                wait(NULL);
                char buffer[16] = {0};
                if (read(pipe_fds[0], buffer, sizeof(buffer)) > 0) {
                    sscanf(buffer, "%s", alarm_notification_id);
                }
            }
        }

        sleep(check_interval);
    }

defer:
    if (capacity_file) fclose(capacity_file);
    if (status_file) fclose(status_file);
    return result;
}
