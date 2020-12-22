/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_ARG_HPP
#define KMAP_ARG_HPP

#include "common.hpp"
#include "network.hpp"
#include "path.hpp"
#include "utility.hpp"

#include <map>
#include <string>
#include <vector>

namespace kmap
{

class Kmap;

class Argument
{
public:
    Argument( std::string const& desc // Describes the arg by itself e.g., "destination path".
            , std::string const& cmd_ctx_desc ); // Describes the command in the context of this arg e.g., "<cmd> copies file to destination path".
    virtual ~Argument() = default;

    virtual auto is_fmt_malformed( std::string const& raw ) const
        -> Optional< uint32_t >;
    virtual auto complete( std::string const& raw ) const
        -> StringVec;
    virtual auto tip( std::string const& raw ) const
        -> std::string;

    auto desc() const
        -> std::string;
    auto cmd_ctx_desc() const
        -> std::string;
    auto is_optional() const
        -> bool;
    auto make_optional()
        -> Argument&;

private:
    std::string desc_;
    std::string cmd_ctx_desc_;
    bool optional_ = false;
};

class CommandArg : public Argument
{
public:
    CommandArg( std::string const& descr
              , std::string const& cmd_ctx_desc
              , Kmap& kmap );

    auto is_fmt_malformed( std::string const& raw ) const 
        -> Optional< uint32_t > override;
    auto complete( std::string const& raw ) const 
        -> StringVec override;

    Kmap& kmap_;
};

class FsPathArg : public Argument
{
public:
    FsPathArg( std::string const& descr
             , std::string const& cmd_ctx_desc );

    auto is_fmt_malformed( std::string const& raw ) const 
        -> Optional< uint32_t > override;
    auto complete( std::string const& raw ) const 
        -> StringVec override;
};

class HeadingArg : public Argument
{
public:
    HeadingArg( std::string const& descr
              , std::string const& cmd_ctx_desc );
    HeadingArg( Heading const& root
              , std::string const& descr
              , std::string const& cmd_ctx_desc );

    auto is_fmt_malformed( std::string const& raw ) const
        -> Optional< uint32_t > override;
};

class HeadingPathArg : public Argument
{
public:
    HeadingPathArg( std::string const& arg_desc
                  , std::string const& cmd_ctx_desc
                  , Kmap const& kmap );
    HeadingPathArg( std::string const& arg_desc
                  , std::string const& cmd_ctx_desc
                  , Heading const& root
                  , Kmap const& kmap );

    auto is_fmt_malformed( std::string const& raw ) const 
        -> Optional< uint32_t > override;
    auto complete( std::string const& raw ) const 
        -> StringVec override;

    Kmap const& kmap_;
    Heading root_ = "/";
};

class InvertedPathArg : public Argument
{
public:
    InvertedPathArg( std::string const& arg_desc
                   , std::string const& cmd_ctx_desc
                   , Kmap const& kmap );
    InvertedPathArg( std::string const& arg_desc
                   , std::string const& cmd_ctx_desc
                   , Heading const& root
                   , Kmap const& kmap );

    auto is_fmt_malformed( std::string const& raw ) const 
        -> Optional< uint32_t > override;
    auto complete( std::string const& raw ) const 
        -> StringVec override;

    Kmap const& kmap_;
    Heading root_ = "/";
};

class TitleArg : public Argument
{
public:
    TitleArg( std::string const& descr
            , std::string const& cmd_ctx_desc );

    auto is_fmt_malformed( std::string const& raw ) const
        -> Optional< uint32_t > override;
};

class UIntArg : public Argument
{
public:
    UIntArg( std::string const& descr
           , std::string const& cmd_ctx_desc );

    auto is_fmt_malformed( std::string const& raw ) const
        -> Optional< uint32_t > override;
};

class JumpInArg : public Argument
{
public:
    JumpInArg( std::string const& arg_desc
             , std::string const& cmd_ctx_desc
             , Kmap const& kmap );

    auto is_fmt_malformed( std::string const& raw ) const 
        -> Optional< uint32_t > override;
    auto complete( std::string const& raw ) const 
        -> StringVec override;

    Kmap const& kmap_;
};

class JumpOutArg : public Argument
{
public:
    JumpOutArg( std::string const& arg_desc
              , std::string const& cmd_ctx_desc
              , Kmap const& kmap );

    auto is_fmt_malformed( std::string const& raw ) const 
        -> Optional< uint32_t > override;
    auto complete( std::string const& raw ) const 
        -> StringVec override;

    Kmap const& kmap_;
};

class DynamicHeadingPathArg : public Argument
{
public:
    // autocomplets path and changes view port
};

class DailyLogArg : public Argument
{
public:
    DailyLogArg( std::string const& arg_desc
               , std::string const& cmd_ctx_desc
               , Kmap const& kmap );

    auto is_fmt_malformed( std::string const& raw ) const 
        -> Optional< uint32_t > override;
    auto complete( std::string const& raw ) const 
        -> StringVec override;

    Kmap const& kmap_;
};

class ProjectArg : public Argument
{
public:
    ProjectArg( std::string const& arg_desc
              , std::string const& cmd_ctx_desc
              , Kmap const& kmap );

    auto is_fmt_malformed( std::string const& raw ) const 
        -> Optional< uint32_t > override;
    auto complete( std::string const& raw ) const 
        -> StringVec override;

    Kmap const& kmap_;
};

class ConclusionArg : public Argument
{
public:
    ConclusionArg( std::string const& arg_desc
                 , std::string const& cmd_ctx_desc
                 , Kmap const& kmap );

    auto is_fmt_malformed( std::string const& raw ) const 
        -> Optional< uint32_t > override;
    auto complete( std::string const& raw ) const 
        -> StringVec override;

    Kmap const& kmap_;
};

class RecipeArg : public Argument
{
public:
    RecipeArg( std::string const& arg_desc
                 , std::string const& cmd_ctx_desc
                 , Kmap const& kmap );

    auto is_fmt_malformed( std::string const& raw ) const 
        -> Optional< uint32_t > override;
    auto complete( std::string const& raw ) const 
        -> StringVec override;

    Kmap const& kmap_;
};

class TagPathArg : public Argument
{
public:
    TagPathArg( std::string const& arg_desc
              , std::string const& cmd_ctx_desc
              , Kmap const& kmap );

    auto is_fmt_malformed( std::string const& raw ) const 
        -> Optional< uint32_t > override;
    auto complete( std::string const& raw ) const 
        -> StringVec override;

    Kmap const& kmap_;
};

// TODO: Basically redunant with TagPathArg
class DefinitionPathArg : public Argument
{
public:
    DefinitionPathArg( std::string const& arg_desc
                     , std::string const& cmd_ctx_desc
                     , Kmap const& kmap );

    auto is_fmt_malformed( std::string const& raw ) const 
        -> Optional< uint32_t > override;
    auto complete( std::string const& raw ) const 
        -> StringVec override;

    Kmap const& kmap_;
};

class ResourcePathArg : public Argument
{
public:
    ResourcePathArg( std::string const& arg_desc
                   , std::string const& cmd_ctx_desc
                   , Kmap const& kmap );

    auto is_fmt_malformed( std::string const& raw ) const 
        -> Optional< uint32_t > override;
    auto complete( std::string const& raw ) const 
        -> StringVec override;

    Kmap const& kmap_;
};

class AliasDestArg : public Argument
{
public:
    AliasDestArg( std::string const& arg_desc
                , std::string const& cmd_ctx_desc
                , Kmap const& kmap );
    AliasDestArg( std::string const& arg_desc
                , std::string const& cmd_ctx_desc
                , Heading const& root
                , Kmap const& kmap );

    auto is_fmt_malformed( std::string const& raw ) const 
        -> Optional< uint32_t > override;
    auto complete( std::string const& raw ) const 
        -> StringVec override;

    Kmap const& kmap_;
};

class CommandPathArg : public Argument
{
public:
    CommandPathArg( std::string const& arg_desc
                  , std::string const& cmd_ctx_desc
                  , Kmap const& kmap );

    auto is_fmt_malformed( std::string const& raw ) const 
        -> Optional< uint32_t > override;
    auto complete( std::string const& raw ) const 
        -> StringVec override;

    Kmap const& kmap_;
};

using ArgumentList = std::vector< std::shared_ptr< Argument > >; // TODO: should be unique_ptr, but struggled to get pipe/chaining operator working with it.

template< typename DerivedArg >
auto operator|( ArgumentList lhs
              , DerivedArg rhs )
    -> ArgumentList&&
{
    using std::move;
    using std::make_shared;
    using std::static_pointer_cast;

    lhs.push_back( make_shared< DerivedArg >( rhs ) );

    return move( lhs );
}

enum class Attr
{
    optional
};

inline
auto operator|( ArgumentList lhs
              , Attr rhs )
    -> ArgumentList&&
{
    using std::move;

    if( lhs.empty() )
    {
        return move( lhs );
    }
    else
    {
        switch( rhs )
        {
        case Attr::optional:
            lhs.back()
              ->make_optional();
            break;
        default:
            assert( false );
            break;
        }

        return move( lhs );
    }
}

} // namespace kmap

#endif // KMAP_ARG_HPP
