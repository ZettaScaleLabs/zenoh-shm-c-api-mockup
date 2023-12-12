#include "../zenoh_shm.h"

// factory functions returning SHM providers and clients. There would be some default built-in in Zenoh
// or user-implemented like in custom_shared_memory_provider.h 
z_owned_shared_memory_provider_backend_t zenoh_shm_backend(const char*) { ... }
z_owned_shared_memory_provider_backend_t qualcomm_backend(const char*) { ... }
z_owned_shared_memory_client_t zenoh_shm_client() { ... }
z_owned_shared_memory_client_t qualcomm_client() { ... }

// the simple example on how to use SHM provider API
void publisher_usage_example(z_owned_config_t *config, z_owned_keyexpr_t *keyexpr)
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

    // declare publisher
    z_owned_publisher_t pub = z_declare_publisher(z_loan(s), z_move(keyexpr), NULL);
    if (!z_check(pub))
    {
        printf("Unable to declare Publisher for key expression!\n");
        exit(-1);
    }

    // get shared memory provider handle from session
    z_shared_memory_provider_t provider = z_session_shared_memory_provider(s, 0);
    if (!z_check(provider))
    {
        printf("Unable to get provider from session!\n");
        exit(-1);
    }

    zc_owned_shmbuf_t shmbuf;
    for (int idx = 0; true; ++idx)
    {

        // try alloc shared memory buffer
        z_alloc_result_t result = z_shared_memory_provider_alloc(provider, 1024, &shmbuf);
        // check allocation result
        switch (result)
        {
        case z_alloc_result_t::NEED_DEFRAGMENT:
            // shm provider needs defragmentation, try to defragment and allocate again
            z_shared_memory_provider_defragment(provider);
            z_alloc_result_t result = z_shared_memory_provider_alloc(provider, 1024, &shmbuf);
            if (result != z_alloc_result_t::OK)
            {
                printf("Failed to allocate a SHM buffer, even after GCing\n");
                exit(-1);
            }
            break;
        case z_alloc_result_t::OK:
            break;
        case z_alloc_result_t::OTHER_ERROR:
            printf("Other error\n");
            exit(-1);
            break;
        case z_alloc_result_t::OUT_OF_MEMORY:
            printf("Out of memory\n");
            exit(-1);
            break;
        }

        // obtain data pointer
        char *buf = (char *)zc_shmbuf_ptr(&shmbuf);
        buf[256] = 0;
        snprintf(buf, 255, "[%4d] %s", idx, value);
        size_t len = strlen(buf);
        zc_shmbuf_set_length(&shmbuf, len);

        sleep(1);

        // publish the shared memory buffer
        z_publisher_put_options_t options = z_publisher_put_options_default();
        options.encoding = z_encoding(Z_ENCODING_PREFIX_TEXT_PLAIN, NULL);
        zc_owned_payload_t payload = zc_shmbuf_into_payload(z_move(shmbuf));
        zc_publisher_put_owned(z_loan(pub), z_move(payload), &options);
    }

    z_undeclare_publisher(z_move(pub));
    z_close(z_move(s));
}
