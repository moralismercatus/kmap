/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_UTIL_LOG_LOG_HPP
#define KMAP_UTIL_LOG_LOG_HPP

#if KMAP_LOG

#include <boost/property_tree/ptree.hpp>

#include <cstdint>
#include <memory>
#include <set>
#include <string>

    #define KM_LOG_ENABLE( tags ) kmap::util::log::Singleton::instance().flags.enable_logging = true;
    #define KM_LOG_DISABLE( tags ) kmap::util::log::Singleton::instance().flags.enable_logging = false;
    #define KM_LOG_PAUSE_SCOPE() auto km_log_scoped_pauser = kmap::util::log::ScopedPauser{};
    #define KM_LOG_CALL_STACK_SCOPE() auto km_log_scoped_call_stack = kmap::util::log::ScopedCallStack{};
    #define KM_LOG_IS_ENABLED() kmap::util::log::Singleton::instance().flags.enable_logging
    // TODO: KM_LOG_IS_TAG_ENABLED() ...query global state to see if a tag is enabled...

    #if KMAP_NATIVE
        #define KM_LOG_ST_ENABLE( tags ) kmap::util::log::Singleton::instance().flags.enable_stacktrace = true;
        #define KM_LOG_ST_DISABLE( tags ) kmap::util::log::Singleton::instance().flags.enable_stacktrace = false;
    #else // KMAP_NATIVE
        #define KM_LOG_ST_ENABLE( tags )
        #define KM_LOG_ST_DISABLE( tags )
    #endif // KMAP_NATIVE

    #define KM_LOG_FN_PROLOG() \
        auto const km_log_fn_prolog = kmap::util::log::ScopedFunctionLog{};

    #define KM_LOG_MSG( tags, msg ) kmap::util::log::Singleton::instance().push( tags, msg );

    #if KMAP_NATIVE
        #define KM_PRINT_STACKTRACE() \
            if( KM_LOG_IS_ENABLED() ) { \
                kmap::util::log::print_stacktrace(); \
            } 
    #else // KMAP_NATIVE
        #define KM_PRINT_STACKTRACE()
    #endif // KMAP_NATIVE

    #define KMAP_LOG_LINE() \
            fmt::print( "[log][line] {}|{}:{}\n", __func__, kmap::FsPath{ __FILE__ }.filename().string(), __LINE__ ); \
            std::cout.flush();

namespace kmap::util::log {

class GlobalState
{
public:
    using Tags = std::set< std::string >;


private:
    Tags tags_ = {};

public:
    GlobalState() = default;

    struct
    {
        bool enable_logging = false;
        bool enable_call_stack = false;
        #if KMAP_NATIVE
        bool enable_stacktrace = false;
        #endif // KMAP_NATIVE
    } flags;
    struct CallStack
    {
        using Node = boost::property_tree::ptree;

        Node cs_root_;
        std::reference_wrapper< Node > cs_current_;

        CallStack();

        auto add( std::string const& name
                , Node& node )
            -> void;
        auto current()
            -> Node&;
        auto current( Node& node )
            -> void;
        auto push( std::string const&
                 , Node& node )
            -> void;
        auto root()
            -> Node&;
    } call_stack;

    auto push( std::string const tags
             , std::string const& msg )
        -> void;
};

class Singleton
{
    static std::unique_ptr< GlobalState > inst_;

public:
    static GlobalState& instance();
};

struct ScopedFunctionLog
{
    std::reference_wrapper< GlobalState::CallStack::Node > parent;

    ScopedFunctionLog( const char* function = __builtin_FUNCTION() /* TODO: replace with std::source_location */
                     , const char* file = __builtin_FILE() /* TODO: replace with std::source_location */
                     , unsigned line = __builtin_LINE() ); /* TODO: replace with std::source_location */
    ~ScopedFunctionLog();
};

class ScopedPauser
{
    bool logging_enabled_;

public:
    ScopedPauser();
    ~ScopedPauser();
};

class ScopedCallStack
{
    bool call_stack_enabled_;

public:
    ScopedCallStack();
    ~ScopedCallStack();
};

#if KMAP_NATIVE
auto print_stacktrace()
    -> void;
#endif // KMAP_NATIVE

} // namespace kmap::util::log

#else // KMAP_LOG
    #define KMAP_LOG_LINE()
    #define KM_LOG_DISABLE( tags )
    #define KM_LOG_ENABLE( tags )
    #define KM_LOG_FN_PROLOG()
    #define KM_LOG_IS_ENABLED() false
    #define KM_LOG_MSG( tags, msg )
    #define KM_LOG_PAUSE_SCOPE()
    #define KM_LOG_ST_DISABLE( tags )
    #define KM_LOG_ST_ENABLE( tags )
    #define KM_PRINT_STACKTRACE()
#endif // KMAP_LOG

#endif // KMAP_UTIL_LOG_LOG_HPP