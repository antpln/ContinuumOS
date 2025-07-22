
#include <stdint.h>

enum class HookType {
    TIME_REACHED,
    SIGNAL,
    CUSTOM
};

struct Hook {
    HookType type;
    uint64_t trigger_value; // e.g. logical time, signal number, etc.

    bool matches(HookType incoming_type, uint64_t incoming_value) const {
        return type == incoming_type && trigger_value == incoming_value;
    }
};
