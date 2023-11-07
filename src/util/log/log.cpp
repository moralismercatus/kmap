/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include <util/log/log.hpp>

#include <common.hpp>
#include <contract.hpp>

#if KMAP_NATIVE
#include <boost/stacktrace.hpp>
#endif // KMAP_NATIVE

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

#include <iostream>

#if KMAP_LOG
namespace kmap::util::log {

ScopedFunctionLog::ScopedFunctionLog( const char* function
                                    , const char* file
                                    , unsigned line )
    : parent{ kmap::util::log::Singleton::instance().call_stack.cs_current_ }
{
    if( KM_LOG_IS_ENABLED() )
    {
        auto& linst = kmap::util::log::Singleton::instance();

        if( linst.flags.enable_call_stack )
        {
            parent = linst.call_stack.current();

            auto csn = GlobalState::CallStack::Node{};

            csn.put( "<xmlattr>.function", function );
            csn.put( "<xmlattr>.line", line );
            csn.put( "<xmlattr>.file", file );

            linst.call_stack.push( "call", csn );
        }
    }
}

ScopedFunctionLog::~ScopedFunctionLog()
{
    if( KM_LOG_IS_ENABLED() )
    {
        auto& linst = kmap::util::log::Singleton::instance();

        if( linst.flags.enable_call_stack )
        {
            linst.call_stack.current( parent ); // "pop"
        }
    }
}

ScopedPauser::ScopedPauser()
    : logging_enabled_{ KM_LOG_IS_ENABLED() }
{
    // TODO: Somehow.. tags need to be communicated... need to store enabled tags and then re-enable them.
    KM_LOG_DISABLE( "*" );
}

ScopedPauser::~ScopedPauser()
{
    kmap::util::log::Singleton::instance().flags.enable_logging = logging_enabled_;
}

ScopedCallStack::ScopedCallStack()
    : call_stack_enabled_{ kmap::util::log::Singleton::instance().flags.enable_call_stack }
{
    // TODO: Somehow.. tags need to be communicated... need to store enabled tags and then re-enable them.
    kmap::util::log::Singleton::instance().flags.enable_call_stack = true;
}

ScopedCallStack::~ScopedCallStack()
{
    auto& linst = kmap::util::log::Singleton::instance();
    
    linst.flags.enable_call_stack = call_stack_enabled_;

    if( !call_stack_enabled_ ) // End of call stack enabling, so dump to file here.
    {
        auto ofs = std::ofstream{ "call_stack.xml" };

        BC_ASSERT( ofs.good() );

        boost::property_tree::write_xml( ofs, linst.call_stack.root() );
    }
}

GlobalState::CallStack::CallStack()
    : cs_root_{}
    , cs_current_{ cs_root_.put( "log.callstack", "" ) }
{
}

auto GlobalState::CallStack::add( std::string const& name
                                , CallStack::Node& n )
    -> void
{
    cs_current_.get().add_child( name, n );
}

auto GlobalState::CallStack::current()
    -> CallStack::Node&
{
    return cs_current_;
}

auto GlobalState::CallStack::current( CallStack::Node& n )
    -> void
{
    cs_current_ = n;
}

auto GlobalState::CallStack::push( std::string const& name
                                 , CallStack::Node& n )
    -> void
{
    cs_current_ = cs_current_.get().add_child( name, n );
}

auto GlobalState::push( std::string const tags
                      , std::string const& msg )
    -> void
{
    // TODO: If tags match enabled tags... && logging enabled...
    if( KM_LOG_IS_ENABLED() )
    {
        fmt::print( "[log][{}] {}\n", tags, msg );
    }
}

auto GlobalState::CallStack::root()
    -> CallStack::Node&
{
    return cs_root_;
}

std::unique_ptr< GlobalState > Singleton::inst_ = {};

GlobalState& Singleton::instance()
{
    if( !inst_ )
    {
        inst_ = std::make_unique< GlobalState >();
    }

    return *inst_;
}

#if KMAP_NATIVE
auto print_stacktrace()
    -> void
{
    std::cout << boost::stacktrace::stacktrace() << "\n";
}
#endif // KMAP_NATIVE

} // namespace kmap::util::log

#endif // KMAP_LOG
