#ifndef MQTT_ADAPTER_H
#define MQTT_ADAPTER_H

typedef void (*mqtt_msg_cb_t)(
    void *userdata,
    const char *topic,
    const char *payload
);

void mqtt_init(
    const char *client_id
);

int mqtt_connect(
    const char *host, 
    int port, 
    const char *lwt_topic, 
    const char *lwt_message
);

//void mqtt_connect(const char *host, int port);

void mqtt_disconnect(void);

void mqtt_subscribe(
    const char *topic,
    mqtt_msg_cb_t cb,
    void *userdata
);

void mqtt_publish(
    const char *topic,
    const char *payload
);

#endif
