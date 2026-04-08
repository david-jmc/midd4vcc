#include "mqtt_adapter.h"
#include <MQTTClient.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define ADDRESS     "tcp://localhost:1883"
#define QOS         0
#define TIMEOUT     100L
#define MAX_SUBS    50

// Estrutura para gerir múltiplas subscrições no adaptador
typedef struct {
    char topic[128];
    mqtt_msg_cb_t cb;
    void *userdata;
    int active;
} subscription_entry_t;

static MQTTClient client;
static subscription_entry_t sub_table[MAX_SUBS];

/* Helper para verificar se um tópico MQTT coincide (suporta wildcards + e #) */
static int topic_matches(const char *sub, const char *topic) {
    while (*sub && *topic) {
        if (*sub == '#') return 1;
        if (*sub == '+') {
            while (*topic && *topic != '/') topic++;
            sub++;
            if (*sub == '/') { sub++; if(*topic == '/') topic++; }
            continue;
        }
        if (*sub != *topic) return 0;
        sub++; topic++;
    }
    return (*sub == '\0' && *topic == '\0');
}

static int msg_arrived(void *context, char *topicName, int topicLen, MQTTClient_message *message) {
    char *payload = malloc(message->payloadlen + 1);
    memcpy(payload, message->payload, message->payloadlen);
    payload[message->payloadlen] = '\0';

    // Roteamento interno: percorre a tabela e chama todos os callbacks interessados
    for (int i = 0; i < MAX_SUBS; i++) {
        if (sub_table[i].active && topic_matches(sub_table[i].topic, topicName)) {
            sub_table[i].cb(sub_table[i].userdata, topicName, payload);
        }
    }

    free(payload);
    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);
    return 1;
}

void mqtt_init(const char *client_id) {
    memset(sub_table, 0, sizeof(sub_table));
    MQTTClient_create(&client, ADDRESS, client_id, MQTTCLIENT_PERSISTENCE_NONE, NULL);
    MQTTClient_setCallbacks(client, NULL, NULL, msg_arrived, NULL);
}

int mqtt_connect(const char *host, int port, const char *lwt_topic, const char *lwt_message) {
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    MQTTClient_willOptions will_opts = MQTTClient_willOptions_initializer;

    if (lwt_topic && lwt_message) {
        will_opts.topicName = lwt_topic;
        will_opts.message = lwt_message;
        will_opts.retained = 0; 
        will_opts.qos = 1;
        conn_opts.will = &will_opts;
    }

    conn_opts.keepAliveInterval = 10;
    conn_opts.cleansession = 1;
    int rc = MQTTClient_connect(client, &conn_opts);
    if (rc != MQTTCLIENT_SUCCESS) {
        printf("[MQTT] Failure to connect, return code %d\n", rc);
    }
    return rc;
}

void mqtt_subscribe(const char *topic, mqtt_msg_cb_t cb, void *userdata) {
    // Adiciona à tabela de subscrições interna
    int added = 0;
    for (int i = 0; i < MAX_SUBS; i++) {
        if (!sub_table[i].active) {
            strncpy(sub_table[i].topic, topic, 127);
            sub_table[i].cb = cb;
            sub_table[i].userdata = userdata;
            sub_table[i].active = 1;
            added = 1;
            break;
        }
    }

    if (added) {
        MQTTClient_subscribe(client, topic, QOS);
    } else {
        printf("[MQTT] Error: Subscription table full\n");
    }
}

void mqtt_publish(const char *topic, const char *payload) {
    MQTTClient_message msg = MQTTClient_message_initializer;
    msg.payload = (void *)payload;
    msg.payloadlen = (int)strlen(payload);
    msg.qos = QOS;
    msg.retained = 0;
    MQTTClient_deliveryToken token;
    
    MQTTClient_publishMessage(client, topic, &msg, &token);
    MQTTClient_waitForCompletion(client, token, TIMEOUT);
}

void mqtt_disconnect(void) {
    MQTTClient_disconnect(client, 1000);
    MQTTClient_destroy(&client);
}