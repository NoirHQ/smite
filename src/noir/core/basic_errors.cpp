#include <noir/core/basic_errors.h>

namespace noir {

const Error err_not_implemented = user_error_registry().register_error("not implemented");
const Error err_unreachable = user_error_registry().register_error("MUST not reach here");
const Error err_eof = user_error_registry().register_error("EOF");

} // namespace noir
