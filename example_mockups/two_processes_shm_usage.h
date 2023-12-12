#include "../zenoh_shm.h"

//  The shm provider and client parts are splitted intentionally: the buffer producer and consumer may have
//  different codebases, and there is no real need to link consumer with the code that is used for producer.
//  In an example below PROCESS_1 may be linked with hypotetical Qualcomm camera libraries, while PROCESS_2
//  does not need to have the same codebase.

/// PROCESS 1 ///
z_owned_shared_memory_provider_backend_t qualcomm_backend(const char*) { ... }
void process1()
{
    // Create the SHM Factory for camera publisher

    // create map with Qualcomm camera shared memory provider
    z_shared_memory_mapped_providers_t providers[1];
    providers[0].id = 1;
    providers[0].backend = qualcomm_backend("/dev/camera0");

    z_owned_str_t error;

    // create shared memory factory with qualcomm camera provider
    z_owned_shared_memory_factory_t shmf = z_shared_memory_factory_make(providers, 1, NULL, 0, &error);
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

    // publish camera frames.... (like in simple_shm_publisher.h)
}


/// PROCESS 2 ///
z_owned_shared_memory_client_t qualcomm_client() { ... }
void process2()
{
    // Create the SHM Factory for camera subscriber

    // create map with shared memory protocol clients
    z_shared_memory_mapped_clients_t clients[1];
    clients[0].id = 1;
    clients[0].client = qualcomm_client();

    z_owned_str_t error;

    // create shared memory factory with qualcomm camera client
    z_owned_shared_memory_factory_t shmf = z_shared_memory_factory_make(NULL, 0, clients, 1, &error);
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

    // subscribe to camera frames.... (like in simple_shm_subscriber.h)
}
