#include <pipewire/pipewire.h>

struct RoundtripData {
    int pending;
    struct pw_main_loop* loop;
};

void onCoreDone(void* data, uint32_t id, int seq) {
    struct RoundtripData* d = data;

    if (id == PW_ID_CORE && seq == d->pending) {
        pw_main_loop_quit(d->loop);
    }
}

void roundtrip(struct pw_core* core, struct pw_main_loop* loop) {
    const struct pw_core_events coreEvents = {
        PW_VERSION_CORE_EVENTS,
        .done = onCoreDone
    };

    struct RoundtripData d = {.loop = loop};

    struct spa_hook coreListener;

    pw_core_add_listener(core, &coreListener, &coreEvents, &d);

    d.pending = pw_core_sync(core, PW_ID_CORE, 0);

    pw_main_loop_run(loop);

    spa_hook_remove(&coreListener);
}

void RegistryEventGlobal(void* data, uint32_t id, uint32_t permissions, const char* type, uint32_t version, const struct spa_dict* props) {
    printf("object: id: %u\t\ttype: %s\t\tversion: %d\n", id, type, version);
}

const struct pw_registry_events registryEvents = {
    PW_VERSION_REGISTRY_EVENTS,
    .global = RegistryEventGlobal
};

int main(int argc, char* argv[]) {
    struct pw_main_loop* loop;
    struct pw_context* context;
    struct pw_core* core;
    struct pw_registry* registry;
    struct spa_hook registryListener;

    pw_init(&argc, &argv);

    loop = pw_main_loop_new(NULL);

    context = pw_context_new(pw_main_loop_get_loop(loop), NULL, 0);

    core = pw_context_connect(context, NULL, 0);

    registry = pw_core_get_registry(core, PW_VERSION_REGISTRY, 0);

    spa_zero(registryListener);

    pw_registry_add_listener(registry, &registryListener, &registryEvents, NULL);

    roundtrip(core, loop);

    pw_proxy_destroy((struct pw_proxy*)registry);
    pw_core_disconnect(core);
    pw_context_destroy(context);
    pw_main_loop_destroy(loop);

    return 0;
}
