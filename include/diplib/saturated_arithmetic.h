/*
 * DIPlib 3.0
 * This file contains overloaded definitions for the functions dip::saturated_XXX().
 *
 * (c)2016, Cris Luengo.
 */

#ifndef DIP_SATURATED_ARITHMETIC_H
#define DIP_SATURATED_ARITHMETIC_H

#include <limits>

#include "diplib/library/types.h"
#include "diplib/library/clamp_cast.h"

#ifdef DIP__ENABLE_DOCTEST
#include "doctest.h"
#endif


/// \file
/// \brief Defines templated functions for saturated arithmetic.


// NOTE: A different strategy is described in http://locklessinc.com/articles/sat_arithmetic/


namespace dip {


/// \addtogroup sample_operators
///
/// `dip::saturated_XXX` are templated functions for saturated arithmetic. Most DIPlib functions take care
/// of properly clamping the result of operations on pixels by using these functions to perform arithmetic.
/// For example,
///
///     10u - 20u == 4294967286u;
///     saturated_sub(10u, 20u) == 0u;
///
/// Saturated arithmetic is made available by including `diplib/saturated_arithmetic.h`.
/// \{


//
// Addition
//

/// \brief Adds two values using saturated arithmetic.
// The base template is good for floats and complex.
template< typename T >
constexpr inline const T saturated_add( T const& lhs, T const& rhs ) {
   return lhs + rhs;
}
// Unsigned integers overflow by giving a result that is smaller than either operand.
// This code is supposed to be branchless, the compiler optimizes using a conditional move.
template<>
constexpr inline const uint32 saturated_add( uint32 const& lhs, uint32 const& rhs ) {
   uint32 res = lhs + rhs;
   return res < lhs ? std::numeric_limits< uint32 >::max() : res;
}
template<>
constexpr inline const uint16 saturated_add( uint16 const& lhs, uint16 const& rhs ) {
   uint16 res = lhs + rhs;
   return res < lhs ? std::numeric_limits< uint16 >::max() : res;
}
template<>
constexpr inline const uint8 saturated_add( uint8 const& lhs, uint8 const& rhs ) {
   uint8 res = lhs + rhs;
   return res < lhs ? std::numeric_limits< uint8 >::max() : res;
}
// Signed integers are more complex, we simply use a larger integer type to do the operation.
// TODO: overflow only happens if both operands have the same sign, opportunity for improvement?
template<>
constexpr inline const sint32 saturated_add( sint32 const& lhs, sint32 const& rhs ) {
   return clamp_both< sint32, int64_t >( ( int64_t )lhs + ( int64_t )rhs );
}
template<>
constexpr inline const sint16 saturated_add( sint16 const& lhs, sint16 const& rhs ) {
   return clamp_both< sint16, sint32 >( ( sint32 )lhs + ( sint32 )rhs );
}
template<>
constexpr inline const sint8 saturated_add( sint8 const& lhs, sint8 const& rhs ) {
   return clamp_both< sint8, sint16 >( ( sint16 )lhs + ( sint16 )rhs );
}
// Binary addition is equivalent to OR.
template<>
constexpr inline const bin saturated_add( bin const& lhs, bin const& rhs ) {
   return lhs | rhs;
}


//
// Subtraction
//

/// \brief Subtracts two values using saturated arithmetic.
// The base template is good for floats and complex.
template< typename T >
constexpr inline const T saturated_sub( T const& lhs, T const& rhs ) {
   return lhs - rhs;
}
// Unsigned integers underflow by giving a result that is larger than either operand.
// This code is supposed to be branchless, the compiler optimizes using a conditional move.
template<>
constexpr inline const uint32 saturated_sub( uint32 const& lhs, uint32 const& rhs ) {
   uint32 res = lhs - rhs;
   return res > lhs ? uint32( 0 ) : res;
}
template<>
constexpr inline const uint16 saturated_sub( uint16 const& lhs, uint16 const& rhs ) {
   uint16 res = lhs - rhs;
   return res > lhs ? uint16( 0 ) : res;
}
template<>
constexpr inline const uint8 saturated_sub( uint8 const& lhs, uint8 const& rhs ) {
   uint8 res = lhs - rhs;
   return res > lhs ? uint8( 0 ) : res;
}
// Signed integers are more complex, we simply use a larger integer type to do the operation.
// TODO: overflow only happens if both operands have the opposite sign, opportunity for improvement?
template<>
constexpr inline const sint32 saturated_sub( sint32 const& lhs, sint32 const& rhs ) {
   return clamp_both< sint32, int64_t >( ( int64_t )lhs - ( int64_t )rhs );
}
template<>
constexpr inline const sint16 saturated_sub( sint16 const& lhs, sint16 const& rhs ) {
   return clamp_both< sint16, sint32 >( ( sint32 )lhs - ( sint32 )rhs );
}
template<>
constexpr inline const sint8 saturated_sub( sint8 const& lhs, sint8 const& rhs ) {
   return clamp_both< sint8, sint16 >( ( sint16 )lhs - ( sint16 )rhs );
}
// Binary subtraction is equivalent to AND NOT
template<>
constexpr inline const bin saturated_sub( bin const& lhs, bin const& rhs ) {
   return lhs & !rhs;
}


//
// Multiplication
//

/// \brief Multiplies two values using saturated arithmetic.
// The base template is good for floats and complex.
template< typename T >
constexpr inline const T saturated_mul( T const& lhs, T const& rhs ) {
   return lhs * rhs;
}
// For unsigned integers we simply use a larger integer type to do the operation.
template<>
constexpr inline const uint32 saturated_mul( uint32 const& lhs, uint32 const& rhs ) {
   return clamp_upper< uint32 >( ( uint64_t )lhs * ( uint64_t )rhs );
}
template<>
constexpr inline const uint16 saturated_mul( uint16 const& lhs, uint16 const& rhs ) {
   return clamp_upper< uint16 >( ( uint32 )lhs * ( uint32 )rhs );
}
template<>
constexpr inline const uint8 saturated_mul( uint8 const& lhs, uint8 const& rhs ) {
   return clamp_upper< uint8 >( ( uint16 )lhs * ( uint16 )rhs );
}
// For signed integers we simply use a larger integer type to do the operation.
template<>
constexpr inline const sint32 saturated_mul( sint32 const& lhs, sint32 const& rhs ) {
   return clamp_both< sint32, int64_t >( ( int64_t )lhs * ( int64_t )rhs );
}
template<>
constexpr inline const sint16 saturated_mul( sint16 const& lhs, sint16 const& rhs ) {
   return clamp_both< sint16, sint32 >( ( sint32 )lhs * ( sint32 )rhs );
}
template<>
constexpr inline const sint8 saturated_mul( sint8 const& lhs, sint8 const& rhs ) {
   return clamp_both< sint8, sint16 >( ( sint16 )lhs * ( sint16 )rhs );
}
// Binary multiplication is equivalent to AND
template<>
constexpr inline const bin saturated_mul( bin const& lhs, bin const& rhs ) {
   return lhs & rhs;
}


//
// Division
//

/// \brief Divides two values using saturated arithmetic (but the division never overflows anyway).
// Division never overflows. We let the system handle division by 0.
template< typename T >
constexpr inline const T saturated_div( T const& lhs, T const& rhs ) {
   return lhs / rhs;
}
// Binary division is equivalent to XOR (just to pick something... is this meaningful?).
template<>
constexpr inline const bin saturated_div( bin const& lhs, bin const& rhs ) {
   return lhs ^ rhs;
}


//
// Inversion
//

/// \brief Inverts a value using saturated arithmetic. This is the same as negation, but not for unsigned values.
// The base template is good for floats and complex.
template< typename T >
constexpr inline const T saturated_inv( T const& v ) {
   return -v;
}
// Unsigned integers invert by subtracting from max value.
template<>
constexpr inline const uint32 saturated_inv( uint32 const& v ) {
   return std::numeric_limits< uint32 >::max() - v;
}
template<>
constexpr inline const uint16 saturated_inv( uint16 const& v ) {
   return std::numeric_limits< uint16 >::max() - v;
}
template<>
constexpr inline const uint8 saturated_inv( uint8 const& v ) {
   return std::numeric_limits< uint8 >::max() - v;
}
// Signed integers seem simple but overflow can happen if the value is equal to lowest possible value
template<>
constexpr inline const sint32 saturated_inv( sint32 const& v ) {
   return v == std::numeric_limits< sint32 >::lowest() ? std::numeric_limits< sint32 >::max() : -v;
}
template<>
constexpr inline const sint16 saturated_inv( sint16 const& v ) {
   return v == std::numeric_limits< sint16 >::lowest() ? std::numeric_limits< sint16 >::max() : -v;
}
template<>
constexpr inline const sint8 saturated_inv( sint8 const& v ) {
   return v == std::numeric_limits< sint8 >::lowest() ? std::numeric_limits< sint8 >::max() : -v;
}
// Binary inversion is equivalent to NOT
template<>
constexpr inline const bin saturated_inv( bin const& v ) {
   return !v;
}

/// \}

} // namespace dip


#ifdef DIP__ENABLE_DOCTEST

DOCTEST_TEST_CASE("[DIPlib] testing the dip::saturatedXXX functions") {
   // Addition
   DOCTEST_CHECK( dip::saturated_add( dip::uint8(50), dip::uint8(20) ) == dip::uint8(70) );
   DOCTEST_CHECK( dip::saturated_add( dip::uint8(250), dip::uint8(20) ) == dip::uint8(255) );
   DOCTEST_CHECK( dip::saturated_add( dip::sint16(250), dip::sint16(20) ) == dip::sint16(270) );
   DOCTEST_CHECK( dip::saturated_add( dip::sint16(30000), dip::sint16(10000) ) == dip::sint16(32767) );
   // Subtraction
   DOCTEST_CHECK( dip::saturated_sub( dip::uint16(20), dip::uint16(10) ) == dip::uint16(10) );
   DOCTEST_CHECK( dip::saturated_sub( dip::uint16(10), dip::uint16(20) ) == dip::uint16(0) );
   DOCTEST_CHECK( dip::saturated_sub( dip::sint16(10), dip::sint16(20) ) == dip::sint16(-10) );
   DOCTEST_CHECK( dip::saturated_sub( dip::uint8(10), dip::uint8(20) ) == dip::uint8(0) );
   DOCTEST_CHECK( dip::saturated_sub( dip::uint32(10000), dip::uint32(5000) ) == dip::uint32(5000) );
   DOCTEST_CHECK( dip::saturated_sub( dip::uint32(10000), dip::uint32(10000) ) == dip::uint32(0) );
   DOCTEST_CHECK( dip::saturated_sub( dip::uint32(10000), dip::uint32(20000) ) == dip::uint32(0) );
   // Multiplication
   DOCTEST_CHECK( dip::saturated_mul( dip::sint16(300), dip::sint16(100) ) == dip::sint16(30000) );
   DOCTEST_CHECK( dip::saturated_mul( dip::sint16(300), dip::sint16(-100) ) == dip::sint16(-30000) );
   DOCTEST_CHECK( dip::saturated_mul( dip::sint16(300), dip::sint16(1000) ) == dip::sint16(32767) );
   DOCTEST_CHECK( dip::saturated_mul( dip::sint16(300), dip::sint16(-1000) ) == dip::sint16(-32768) );
   DOCTEST_CHECK( dip::saturated_mul( dip::uint16(300), dip::uint16(1000) ) == dip::uint16(65535) );
   // Division
   DOCTEST_CHECK( dip::saturated_div( dip::sint16(300), dip::sint16(10) ) == dip::sint16(30) );
   // Inversion
   DOCTEST_CHECK( dip::saturated_inv( dip::sint16(300) ) == dip::sint16(-300) );
   DOCTEST_CHECK( dip::saturated_inv( dip::sint16(-32768) ) == dip::sint16(32767) );
   DOCTEST_CHECK( dip::saturated_inv( dip::sint16(-32767) ) == dip::sint16(32767) );
   DOCTEST_CHECK( dip::saturated_inv( dip::sint16(-32766) ) == dip::sint16(32766) );
   DOCTEST_CHECK( dip::saturated_inv( dip::uint16(300) ) == dip::uint16(65235) );
}


#endif // DIP__ENABLE_DOCTEST


#endif // DIP_SATURATED_ARITHMETIC_H
