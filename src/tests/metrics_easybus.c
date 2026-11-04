#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <libraries/access_easybus.h>

int main(int argc, char *argv[])
{
    metric_t *metrics_own = NULL;
    metric_t *metrics = NULL;
    int metrics_own_count = 0;
    int metrics_count = 0;
    int m, v;

    #define print_metrics(mets, mets_count) \
    for (m = 0; m < mets_count; m++) { \
        printf("%s_%s: %d vals (%d: %s)\n", \
            mets[m].domain_label, mets[m].metric_label, mets[m].values_count, \
            mets[m].state, mets[m].error_msg); \
        for (v = 0; v < mets[m].values_count && mets[m].values != NULL; ++v) { \
            printf("    %lu\n", mets[m].values[v]); \
        } \
    }
    if (!easybus_connect(NULL)) {
        printf("Could not connect to SeanerBUS: %s\n", strerror(errno));
        return 0;
    }
    if (!easybus_search_available_metrics(Met, Freq, Cpu, &metrics)) {
        printf("Metric is not available: %s\n", strerror(errno));
        return 0;
    }
    print_metrics(metrics, 1);
    metrics = NULL;
    printf("Easybus is connected? %d\n", easybus_is_connected());
    if (!easybus_metrics_alloc(&metrics_own)) {
        printf("Error during the allocation: %s\n", strerror(errno));
    }
    //easybus_get_available_metrics(NULL, NULL);
    if (!easybus_request_get(Met, Freq, Cpu, All, &metrics, &metrics_count)) {
        printf("Error during the get request: %s\n", strerror(errno));
        return 0;
    }
    print_metrics(metrics, metrics_count);
    if (!easybus_request_get(Met, Pow, All, All, &metrics_own, &metrics_own_count)) {
        printf("Error during the get request: %s\n", strerror(errno));
        return 0;
    }
    print_metrics(metrics_own, metrics_own_count);
    // Just to test metrics haven't changed
    print_metrics(metrics, metrics_count);
    if (!easybus_request_get(Act, Freq, Cpu, All, &metrics, &metrics_count)) {
        printf("Error during the get request: %s\n", strerror(errno));
        return 0;
    }
    print_metrics(metrics, metrics_count);
    // Set test
#if 0
    if (!easybus_request_set(Freq, Cpu, All, (uint64_t [])
        { 3010000,3020000,3030000,3040000,3050000,3060000,3070000,3080000 }, 8, NULL)) {
        printf("Error during the set request: %s\n", strerror(errno));
        return 0;
    }
#endif
    easybus_disconnect();
    if (!easybus_request_get(Met, Freq, Cpu, All, &metrics, &metrics_count)) {
        printf("Error during the get request: %s\n", strerror(errno));
        return 0;
    }
    return 0;
}