#ifndef MY_MQTT_CLIENT_H
#define MY_MQTT_CLIENT_H

void mqtt_app_start(void);
void mqtt_task(void *arg);
void mqtt_publish_adc(int val);

#endif
