#include <noir/core/basic_errors.h>

namespace noir {

const Error err_not_implemented = user_error_registry().register_error("not implemented");

} // namespace noir
