#include <stdio.h>
#include <unistd.h>

#include "../zenoh_shm.h"

// factory functions returning SHM providers and clients. There would be some default built-in in Zenoh
// or user-implemented like in custom_shared_memory_provider.h 
z_owned_shared_memory_provider_backend_t zenoh_shm_backend(const char*) { ... }
z_owned_shared_memory_provider_backend_t qualcomm_backend(const char*) { ... }
z_owned_shared_memory_client_t zenoh_shm_client() { ... }
z_owned_shared_memory_client_t qualcomm_client() { ... }

const char *kind_to_str(z_sample_kind_t kind) {
    switch (kind) {
        case Z_SAMPLE_KIND_PUT:
            return "PUT";
        case Z_SAMPLE_KIND_DELETE:
            return "DELETE";
        default:
            return "UNKNOWN";
    }
}

void data_handler(const z_sample_t *sample, void *arg) {
    z_owned_str_t keystr = z_keyexpr_to_string(sample->keyexpr);
    printf(">> [Subscriber] Received %s ('%s': '%.*s')\n", kind_to_str(sample->kind), z_loan(keystr),
           (int)sample->payload.len, sample->payload.start);
    z_drop(z_move(keystr));
}

// the simple example on how to use SHM provider API
void subscriber_usage_example(z_owned_config_t *config, z_owned_keyexpr_t *keyexpr)
{
    // create map with shared memory protocol providers
    z_shared_memory_mapped_providers_t providers[2];
    providers[0].id = 0;
    providers[0].backend = zenoh_shm_backend("1GB");
    providers[1].id = 1;
    providers[1].backend = qualcomm_backend("/dev/camera0");

    // create map with shared memory protocol clients
    z_shared_memory_mapped_clients_t clients[2];
    clients[0].id = 0;
    clients[0].client = zenoh_shm_client();
    clients[1].id = 1;
    clients[1].client = qualcomm_client();

    z_owned_str_t error;

    // create shared memory factory with desired shm protocols
    z_owned_shared_memory_factory_t shmf = z_shared_memory_factory_make(providers, 2, clients, 2, &error);
    if (!z_check(shmf))
    {
        printf("Unable to create shared memory factory!\n");
        exit(-1);
    }

    // create session with shared memory factory
    z_owned_session_t s = z_open_shm(z_move(*config), z_move(shmf));
    if (!z_check(s))
    {
        printf("Unable to open session!\n");
        exit(-1);
    }

    // declare subscriber
    z_owned_closure_sample_t callback = z_closure(data_handler);
    printf("Declaring Subscriber on '%s'...\n", expr);
    z_owned_subscriber_t sub = z_declare_subscriber(z_loan(s), z_move(keyexpr), z_move(callback), NULL);
    if (!z_check(sub)) {
        printf("Unable to declare subscriber.\n");
        exit(-1);
    }

    printf("Enter 'q' to quit...\n");
    char c = 0;
    while (c != 'q') {
        c = getchar();
        if (c == -1) {
            sleep(1);
        }
    }

    z_undeclare_subscriber(z_move(sub));
    z_close(z_move(s));
}
