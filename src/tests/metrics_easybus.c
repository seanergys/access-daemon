#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <libraries/access_easybus.h>

int main(int argc, char *argv[])
{
    metric_t *metrics = NULL;
    int metrics_count = 0;
    int m, v;

    if (!easybus_connect(NULL)) {
        printf("Could not connect to SeanerBUS: %s\n", strerror(errno));
        return 0;
    }
    //easybus_get_available_metrics(NULL, NULL);
    if (!easybus_request(Met, Freq, Cpu, Get, All, NULL, &metrics, &metrics_count)) {
        printf("Error during the request: %s\n", strerror(errno));
        return 0;
    }
    for (m = 0; m < metrics_count; m++) {
        for (v = 0; v < metrics[m].values_count; ++v) {
            printf("%lu\n", metrics[m].values[v]);
        }
    }
    return 0;
}