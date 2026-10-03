#include "mim/plug/runtime/runtime.h"

#include <mim/plugin.h>

using namespace mim;
using namespace mim::plug;

namespace mim::plug::runtime {
void register_phases(Flags2Phases&);
}

MIM_PLUGIN_ENTRY(runtime) {
    plugin.register_normalizers = runtime::register_normalizers;
    plugin.register_phases      = runtime::register_phases;
}
