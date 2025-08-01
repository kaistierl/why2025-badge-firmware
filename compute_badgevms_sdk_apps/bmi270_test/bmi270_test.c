#include "badgevms/device.h"

#include <stdio.h>
#include <unistd.h> 

int main(int argc, char *argv[]) {
    sleep(5);

    bmi270_device_t *bmi270_sensor;

    bmi270_sensor = (bmi270_device_t *)device_get("BMISENSOR0");

    printf("Get BMI270 accel and gyro...\n");
    int ret = bmi270_sensor->_get_orientation(bmi270_sensor);

    sleep(5);
}