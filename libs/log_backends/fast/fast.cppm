module;

export module PixelForge.logging.backends.fast;

export import :types;
export import :collector;

/**
 *@brief A fast logger backend. Each thread owns its own logging instance
 *       and a background thread collects logs from each thread and writes
 *       them to a log file (WITHOUT SORTING). The log file should later be
 *       serialised
 */
namespace pf::log::fast_backend {}
