/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_UTIL_POLYMORPHIC_VALUE_HPP
#define KMAP_UTIL_POLYMORPHIC_VALUE_HPP

#include "utility.hpp" // Testing
#include <contract.hpp>

#include <memory>

namespace kmap {

template< typename T >
    // requires requires( T t ){ t.clone(); }
class PolymorphicValue
{
public:
    using pointer_type = T;

private:
    std::unique_ptr< T > ptr_ = nullptr;

public:
    // TODO: Should ctor take T, T const&, and construct unique_ptr internally, so as to make the pointer aspect transparent to user?
    PolymorphicValue() = default;
    PolymorphicValue( std::unique_ptr< T >&& ptr )
        : ptr_{ std::move( ptr ) }
    {
    }
    PolymorphicValue( PolymorphicValue< T >&& other )
        : ptr_{ std::move( other.ptr_ ) }
    {
    }
    PolymorphicValue( PolymorphicValue const& other )
    {
        if( other.ptr_ )
        {
            ptr_ = other.ptr_->clone();
        }
    }

    auto operator=( PolymorphicValue&& other )
        -> PolymorphicValue< T >&
    {
        if( other.ptr_ )
        {
            ptr_ = std::move( other.ptr_ );
            other.ptr_ = nullptr;
        }
        else
        {
            ptr_ = nullptr;
        }

        return *this;
    }
    auto operator=( PolymorphicValue const& other )
        -> PolymorphicValue< T >&
    {
        if( other.ptr_ )
        {
            ptr_ = other.ptr_->clone();
        }
        else
        {
            ptr_ = nullptr;
        }

        return *this;
    }


    auto get() const { return ptr_.get(); }
    auto operator*() -> T& { BC_ASSERT( ptr_ ); return *ptr_; }
    auto operator*() const -> T const& { BC_ASSERT( ptr_ ); return *ptr_; }
    auto operator->() { BC_ASSERT( ptr_ ); return ptr_.operator->(); }
    auto operator->() const { BC_ASSERT( ptr_ ); return ptr_.operator->(); }
    // auto operator!(){ return !ptr_; }
    explicit operator bool() const{ return !!ptr_; }
    auto operator<( PolymorphicValue const& other ) const
    {
        if( ptr_ && other.ptr_ )
        {
            return *ptr_ < *other.ptr_;
        }
        else
        {
            return ptr_ < other.ptr_;
        }
    }
    // auto operator<=>( PolymorphicValue const& other ) const { return if( ptr_ && other.ptr_ ) ....ptr_ <=> other.ptr_; } // TODO: Why not this?
};

} // namespace kmap

#endif // KMAP_UTIL_POLYMORPHIC_VALUE_HPP