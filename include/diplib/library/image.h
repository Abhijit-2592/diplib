/*
 * DIPlib 3.0
 * This file contains definitions for the Image class and related functions.
 *
 * (c)2014-2017, Cris Luengo.
 * Based on original DIPlib code: (c)1995-2014, Delft University of Technology.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


//
// NOTE!
// This file is included through diplib.h -- no need to include directly
//


#ifndef DIP_IMAGE_H
#define DIP_IMAGE_H

#include <memory>
#include <functional>
#include <cstring> // std::memcpy

#include "diplib/library/tensor.h"
#include "diplib/library/physical_dimensions.h"
#include "diplib/library/clamp_cast.h"
#include "diplib/library/sample_iterator.h"


/// \file
/// \brief Defines the `dip::Image` class and support functions. This file is always included through `diplib.h`.
/// \see infrastructure


/// \brief The `dip` namespace contains all the library functionality.
namespace dip {


// Forward declarations
class DIP_NO_EXPORT Image;                      // in this file
template< typename T >
class DIP_NO_EXPORT GenericImageIterator;       // in `generic_iterators.h`, include explicitly if needed
template< dip::uint N, typename T >
class DIP_NO_EXPORT GenericJointImageIterator;  // in `generic_iterators.h`, include explicitly if needed


/// \addtogroup infrastructure
/// \{


//
// Support for external interfaces
//

/// \brief A `dip::Image` holds a shared pointer to the data segment using this type.
using DataSegment = std::shared_ptr< void >;

/// \brief This function converts a pointer to a `dip::DataSegment` that does not own the data pointed to.
inline DataSegment NonOwnedRefToDataSegment( void* ptr ) {
   return DataSegment{ ptr, []( void* ){} };
}

/// \brief This function converts a pointer to a `dip::DataSegment` that does not own the data pointed to.
inline DataSegment NonOwnedRefToDataSegment( void const* ptr ) {
   return DataSegment{ const_cast< void* >( ptr ), []( void* ){} };
}

/// \brief Support for external interfaces.
///
/// Software using *DIPlib* might want to control how the image data is allocated. Such software
/// should derive a class from this one, and assign a pointer to it into each of the images that
/// it creates, through `dip::Image::SetExternalInterface`. The caller will maintain ownership of
/// the interface.
///
/// See \ref external_interface for details on how to use the external interfaces.
class DIP_EXPORT ExternalInterface {
   public:
      /// Allocates the data for an image. The function is required to set `strides`,
      /// `tensorStride` and `origin`, and return a `dip::DataSegment` that owns the
      /// allocated data segment. Note that `strides` and `tensorStride` might have
      /// been set by the user before calling `dip::Image::Forge`, and should be honored
      /// if possible.
      virtual DataSegment AllocateData(
            void*& origin,
            dip::DataType dataType,
            UnsignedArray const& sizes,
            IntegerArray& strides,
            dip::Tensor const& tensor,
            dip::sint& tensorStride
      ) = 0;
};


//
// Functor that converts indices or offsets to coordinates.
//

/// \brief Computes pixel coordinates based on an index or offset.
///
/// Objects of this class are returned by `dip::Image::OffsetToCoordinatesComputer`
/// and `dip::Image::IndexToCoordinatesComputer`, and act as functors.
/// Call it with an offset or index (depending on which function created the
/// functor), and it will return the coordinates:
///
/// ```cpp
///     auto coordComp = img.OffsetToCoordinatesComputer();
///     auto coords1 = coordComp( offset1 );
///     auto coords2 = coordComp( offset2 );
///     auto coords3 = coordComp( offset3 );
/// ```
///
/// Note that the coordinates must be inside the image domain, if the offset given
/// does not correspond to one of the image's pixels, the result is meaningless.
class DIP_NO_EXPORT CoordinatesComputer {
   public:
      DIP_EXPORT CoordinatesComputer( UnsignedArray const& sizes, IntegerArray const& strides );

      DIP_EXPORT UnsignedArray operator()( dip::sint offset ) const;

   private:
      IntegerArray strides_; // a copy of the image's strides array, but with all positive values
      IntegerArray sizes_;   // a copy of the image's sizes array, but with negative values where the strides are negative
      UnsignedArray index_;  // sorted indices to the strides array (largest to smallest)
      dip::sint offset_;     // offset needed to handle negative strides
};


//
// The Image class
//


/// \brief An array of images
using ImageArray = std::vector< Image >;

/// \brief An array of const images
using ConstImageArray = std::vector< Image const >;

/// \brief An array of image references
using ImageRefArray = std::vector< std::reference_wrapper< Image >>;

/// \brief An array of const image references
using ImageConstRefArray = std::vector< std::reference_wrapper< Image const >>;

// The class is documented in the file src/documentation/image.md
class DIP_NO_EXPORT Image {

   public:

      //
      // Pixels and Samples. Find the implementation of some functions towards the end of the file.
      //

      class Pixel;
      template< typename T > class CastSample;
      template< typename T > class CastPixel;

      /// \brief A sample represents a single numeric value in an image, see \ref image_representation.
      ///
      /// Objects of this class are meant as an interface between images and numbers. These objects are
      /// not actually how values are stored in an image, but rather represent a reference to a sample
      /// in an image. Through this reference, individual samples in an image can be changed. For example:
      ///
      /// ```cpp
      ///     dip::Image img( { 256, 256 } );
      ///     img.At( 10, 20 )[ 0 ] = 3;
      /// ```
      ///
      /// In the code above, `img.At( 10, 20 )[ 0 ]` returns a `%Sample` object. Assigning to this object
      /// changes the sample in `img` that is referenced.
      ///
      /// See \ref indexing for more information.
      ///
      /// \see dip::Image::Pixel, dip::Image::CastPixel, dip::Image::CastSample
      class Sample {
            friend class Pixel; // This is necessary so that Pixel::Iterator can modify origin_.

         public:

            // Default copy constructor doesn't do what we need
            Sample( Sample const& sample ) : dataType_( sample.dataType_ ) {
               origin_ = &buffer_;
               std::memcpy( origin_, sample.origin_, dataType_.SizeOf() );
            }

            // Default move constructor, otherwise it's implicitly deleted.
            Sample( Sample&& ) = default;

            // Construct a Sample over existing data, used by dip::Image, dip::GenericImageIterator,
            // dip::GenericJointImageIterator.
            constexpr Sample( void* data, dip::DataType dataType ) : origin_( data ), dataType_( dataType ) {}

            /// Construct a new `%Sample` by giving the data type. Initialized to 0.
            explicit Sample( dip::DataType dataType = DT_SFLOAT ) : dataType_( dataType ) {
               buffer_ = { 0.0, 0.0 };
               // The buffer filled with zeros yields a zero value no matter as what data type we interpret it.
            }

            /// A numeric value implicitly converts to a `%Sample`.
            template< typename T, typename std::enable_if< IsSampleType< T >::value, int >::type = 0 >
            constexpr Sample( T value ) {
               dataType_ = dip::DataType( value );
               *static_cast< T* >( origin_ ) = value;
            }
            constexpr Sample( bool value ) : dataType_( DT_BIN ) {
               *static_cast< bin* >( origin_ ) = value;
            }
            #if SIZE_MAX != UINT32_MAX // we don't want to compile the next two on 32-bit machines, they'd conflict with s/uint32 constructors above.
            constexpr Sample( dip::uint value ) : dataType_( DT_UINT32 ) {
               *static_cast< uint32* >( origin_ ) = clamp_cast< uint32 >( value );
            }
            constexpr Sample( dip::sint value ) : dataType_( DT_SINT32 ) {
               *static_cast< sint32* >( origin_ ) = clamp_cast< sint32 >( value );
            }
            #endif

            /// A `dip::Image::Pixel`, when cast to a `%Sample`, references the first value in the pixel.
            Sample( Pixel const& pixel );

            /// A `dip::Image`, when cast to a `%Sample`, references the first sample in the first pixel in the image.
            explicit Sample( Image const& image ) : origin_( image.Origin() ), dataType_( image.DataType() ) {}

            /// Swaps `*this` and `other`.
            void swap( Sample& other ) {
               using std::swap;
               bool thisInternal = origin_ == &buffer_;
               bool otherInternal = other.origin_ == &other.buffer_;
               if( thisInternal ) {
                  if( otherInternal ) {
                     swap( buffer_, other.buffer_ );
                  } else {
                     origin_ = other.origin_;
                     other.buffer_ = buffer_;
                     other.origin_ = &other.buffer_;
                  }
               } else {
                  if( otherInternal ) {
                     other.origin_ = origin_;
                     buffer_ = other.buffer_;
                     origin_ = &buffer_;
                  } else {
                     swap( origin_, other.origin_ );
                  }
               }
               swap( dataType_, other.dataType_ );
            }

            void swap( Sample&& other ) { swap( other ); };

            /// Returns the value of the sample as the given numeric type, similar to using `static_cast`.
            template< typename T, typename std::enable_if< IsNumericType< T >::value, int >::type = 0 >
            constexpr T As() const { return detail::CastSample< T >( dataType_, origin_ ); };

            /// A `%Sample` can be cast to basic numerical types.
            constexpr explicit operator bool() const { return As< bin >(); }
            /// A `%Sample` can be cast to basic numerical types.
            constexpr explicit operator dip::uint() const { return As< dip::uint >(); }
            /// A `%Sample` can be cast to basic numerical types.
            constexpr explicit operator dip::sint() const { return As< dip::sint >(); }
            /// A `%Sample` can be cast to basic numerical types.
            constexpr explicit operator sfloat() const { return As< sfloat >(); }
            /// A `%Sample` can be cast to basic numerical types.
            constexpr explicit operator dfloat() const { return As< dfloat >(); }
            /// A `%Sample` can be cast to basic numerical types.
            constexpr explicit operator scomplex() const { return As< scomplex >(); }
            /// A `%Sample` can be cast to basic numerical types.
            constexpr explicit operator dcomplex() const { return As< dcomplex >(); }

            /// Assigning to a `%Sample` copies the value over to the sample referenced.
            constexpr Sample& operator=( Sample const& sample ) {
               detail::CastSample( sample.dataType_, sample.origin_, dataType_, origin_ );
               return *this;
            }
            constexpr Sample& operator=( Sample&& sample ) {
               detail::CastSample( sample.dataType_, sample.origin_, dataType_, origin_ );
               return *this;
            }
            template< typename T >
            constexpr Sample& operator=( CastSample< T > const& sample ) {
               return operator=( static_cast< Sample const& >( sample ));
            }
            /// It is also possible to assign a constant directly.
            template< typename T, typename std::enable_if< IsSampleType< T >::value, int >::type = 0 >
            constexpr Sample& operator=( T value ) {
               detail::CastSample( dip::DataType( value ), &value, dataType_, origin_ );
               return *this;
            }
            constexpr Sample& operator=( bool value ) {
               detail::CastSample( DT_BIN, &value, dataType_, origin_ );
               return *this;
            }
            #if SIZE_MAX != UINT32_MAX // we don't want to compile the next two on 32-bit machines, they'd conflict with s/uint32 constructors above.
            constexpr Sample& operator=( dip::uint value ) {
               uint32 tmp = clamp_cast< uint32 >( value );
               detail::CastSample( DT_UINT32, &tmp, dataType_, origin_ );
               return *this;
            }
            constexpr Sample& operator=( dip::sint value ) {
               sint32 tmp = clamp_cast< sint32 >( value );
               detail::CastSample( DT_SINT32, &tmp, dataType_, origin_ );
               return *this;
            }
            #endif

            /// Returns a pointer to the sample referenced.
            constexpr void* Origin() const { return origin_; }
            /// The data type of the sample referenced.
            constexpr dip::DataType DataType() const { return dataType_; }

            /// \brief Compound assignment operator.
            template< typename T >
            Sample& operator+=( T const& rhs );
            /// \brief Compound assignment operator.
            template< typename T >
            Sample& operator-=( T const& rhs );
            /// \brief Compound assignment operator.
            template< typename T >
            Sample& operator*=( T const& rhs );
            /// \brief Compound assignment operator.
            template< typename T >
            Sample& operator/=( T const& rhs );
            /// \brief Compound assignment operator.
            template< typename T >
            Sample& operator%=( T const& rhs );
            /// \brief Bit-wise compound assignment operator.
            template< typename T >
            Sample& operator&=( T const& rhs );
            /// \brief Bit-wise compound assignment operator.
            template< typename T >
            Sample& operator|=( T const& rhs );
            /// \brief Bit-wise compound assignment operator.
            template< typename T >
            Sample& operator^=( T const& rhs );

         protected:
            dcomplex buffer_;
            void* origin_ = &buffer_;
            dip::DataType dataType_;
      };

      /// \brief A pixel represents a set of numeric value in an image, see \ref image_representation.
      ///
      /// Objects of this class are meant as an interface between images and numbers. These objects are
      /// not actually how pixels are stored in an image, but rather represent a reference to a pixel
      /// in an image. Through this reference, individual pixels in an image can be changed. For example:
      ///
      /// ```cpp
      ///     dip::Image img( { 256, 256 }, 3 );
      ///     img.At( 10, 20 ) = { 4, 5, 6 };
      /// ```
      ///
      /// In the code above, `img.At( 10, 20 )` returns a `%Pixel` object. Assigning to this object
      /// changes the pixel in `img` that is referenced.
      ///
      /// See \ref indexing for more information.
      ///
      /// \see dip::Image::Sample, dip::Image::CastSample, dip::Image::CastPixel
      class Pixel {
         public:

            // Default copy constructor doesn't do what we need
            Pixel( Pixel const& pixel ) : dataType_( pixel.dataType_ ), tensor_( pixel.tensor_ ) {
               buffer_.resize( dataType_.SizeOf() * tensor_.Elements() );
               origin_ = buffer_.data();
               operator=( pixel );
            }

            // Default move constructor, otherwise it's implicitly deleted.
            Pixel( Pixel&& ) = default;

            // Construct a Pixel over existing data, used by dip::Image, dip::GenericImageIterator,
            // dip::GenericJointImageIterator, dml::GetArray.
            Pixel( void* data, dip::DataType dataType, dip::Tensor const& tensor, dip::sint tensorStride ) :
                  origin_( data ), dataType_( dataType ), tensor_( tensor ), tensorStride_( tensorStride ) {}

            /// Construct a new `%Pixel` by giving data type and number of tensor elements. Initialized to 0.
            explicit Pixel( dip::DataType dataType = DT_SFLOAT, dip::uint tensorElements = 1 ) :
                  dataType_( dataType ), tensor_( tensorElements ) {
               buffer_.resize( dataType_.SizeOf() * tensor_.Elements() );
               std::fill( buffer_.begin(), buffer_.end(), 0 );
               origin_ = buffer_.data();
            }

            /// \brief A `%Pixel` can be constructed from a single sample, yielding a scalar pixel with the same
            /// data type as the sample.
            Pixel( Sample const& sample ) : dataType_( sample.DataType() ) { // tensor_ is scalar by default
               buffer_.resize( dataType_.SizeOf() );
               origin_ = buffer_.data();
               std::memcpy( buffer_.data(), sample.Origin(), dataType_.SizeOf() );
            }

            /// \brief A `%Pixel` can be constructed from an initializer list, yielding a pixel with the same data
            /// type and number of tensor elements as the initializer list. The pixel will be a column vector.
            template< typename T, typename std::enable_if< IsSampleType< T >::value, int >::type = 0 >
            Pixel( std::initializer_list< T > values ) {
               dip::uint N = values.size();
               tensor_.SetVector( N );
               dataType_ = dip::DataType( T( 0 ));
               dip::uint sz = dataType_.SizeOf();
               buffer_.resize( sz * N );
               origin_ = buffer_.data();
               uint8* dest = buffer_.data();
               for( auto it = values.begin(); it != values.end(); ++it ) {
                  std::memcpy( dest, &*it, sz );
                  dest += sz;
               }
            }

            /// A `dip::Image`, when cast to a `%Pixel`, references the first pixel in the image.
            explicit Pixel( Image const& image ) :
                  origin_( image.Origin() ),
                  dataType_( image.DataType() ),
                  tensor_( image.Tensor() ),
                  tensorStride_( image.TensorStride() ) {}

            /// Swaps `*this` and `other`.
            void swap( Pixel& other ) {
               using std::swap;
               bool thisInternal = origin_ == buffer_.data();
               bool otherInternal = other.origin_ == other.buffer_.data();
               if( thisInternal ) {
                  if( otherInternal ) {
                     swap( buffer_, other.buffer_ );
                     origin_ = buffer_.data();
                     other.origin_ = other.buffer_.data();
                  } else {
                     origin_ = other.origin_;
                     other.buffer_ = std::move( buffer_ );
                     other.origin_ = other.buffer_.data();
                  }
               } else {
                  if( otherInternal ) {
                     other.origin_ = origin_;
                     buffer_ = std::move( other.buffer_ );
                     origin_ = buffer_.data();
                  } else {
                     swap( origin_, other.origin_ );
                  }
               }
               swap( dataType_, other.dataType_ );
               swap( tensor_, other.tensor_ );
               swap( tensorStride_, other.tensorStride_ );
            }

            void swap( Pixel&& other ) { swap( other ); }

            /// Returns the value of the first sample in the pixel as the given numeric type, similar to using `static_cast`.
            template< typename T, typename std::enable_if< IsNumericType< T >::value, int >::type = 0 >
            T As() const { return detail::CastSample< T >( dataType_, origin_ ); };

            /// A `%Pixel` can be cast to basic numerical types. The first sample in the pixel is used.
            explicit operator bool() const { return As< bin >(); }
            /// A `%Pixel` can be cast to basic numerical types. The first sample in the pixel is used.
            explicit operator dip::uint() const { return As< dip::uint >(); }
            /// A `%Pixel` can be cast to basic numerical types. The first sample in the pixel is used.
            explicit operator dip::sint() const { return As< dip::sint >(); }
            /// A `%Pixel` can be cast to basic numerical types. The first sample in the pixel is used.
            explicit operator sfloat() const { return As< sfloat >(); }
            /// A `%Pixel` can be cast to basic numerical types. The first sample in the pixel is used.
            explicit operator dfloat() const { return As< dfloat >(); }
            /// A `%Pixel` can be cast to basic numerical types. The first sample in the pixel is used.
            explicit operator scomplex() const { return As< scomplex >(); }
            /// A `%Pixel` can be cast to basic numerical types. The first sample in the pixel is used.
            explicit operator dcomplex() const { return As< dcomplex >(); }

            /// Assigning a number or sample to a `%Pixel` copies the value over each of the samples in the pixel.
            Pixel& operator=( Sample const& sample ) {
               dip::uint N = tensor_.Elements();
               dip::uint sz = dataType_.SizeOf();
               uint8* dest = static_cast< uint8* >( origin_ );
               detail::CastSample( sample.DataType(), sample.Origin(), dataType_, dest );
               uint8* src = dest;
               for( dip::uint ii = 1; ii < N; ++ii ) {
                  dest += static_cast< dip::sint >( sz ) * tensorStride_;
                  std::memcpy( dest, src, sz );
               }
               return *this;
            }
            /// Assigning to a `%Pixel` copies the values over to the pixel referenced.
            Pixel& operator=( Pixel const& pixel ) {
               dip::uint N = tensor_.Elements();
               DIP_THROW_IF( pixel.TensorElements() != N, E::NTENSORELEM_DONT_MATCH );
               dip::sint srcSz = static_cast< dip::sint >( pixel.DataType().SizeOf() );
               dip::sint destSz = static_cast< dip::sint >( dataType_.SizeOf() );
               uint8* src = static_cast< uint8* >( pixel.Origin() );
               uint8* dest = static_cast< uint8* >( origin_ );
               for( dip::uint ii = 0; ii < N; ++ii ) {
                  detail::CastSample( pixel.DataType(), src, dataType_, dest );
                  src += srcSz * pixel.TensorStride();
                  dest += destSz * tensorStride_;
               }
               return *this;
            }
            Pixel& operator=( Pixel&& pixel ) {
               return operator=( const_cast< Pixel const& >( pixel )); // Call copy assignment instead
            }
            template< typename T >
            Pixel& operator=( CastPixel< T > const& pixel ) {
               return operator=( static_cast< Pixel const& >( pixel ));
            }
            /// It is also possible to assign from an initializer list.
            template< typename T, typename std::enable_if< IsSampleType< T >::value, int >::type = 0 >
            Pixel& operator=( std::initializer_list< T > values ) {
               dip::uint N = tensor_.Elements();
               DIP_THROW_IF( values.size() != N, E::NTENSORELEM_DONT_MATCH );
               dip::DataType srcDT = dip::DataType( T( 0 ));
               dip::sint destSz = static_cast< dip::sint >( dataType_.SizeOf() );
               uint8* dest = static_cast< uint8* >( origin_ );
               for( auto it = values.begin(); it != values.end(); ++it ) {
                  detail::CastSample( srcDT, &(*it), dataType_, dest );
                  dest += destSz * tensorStride_;
               }
               return *this;
            }

            /// Returns a pointer to the first sample referenced.
            void* Origin() const { return origin_; }
            /// The data type of the pixel referenced.
            dip::DataType DataType() const { return dataType_; }
            /// The tensor shape for the pixel referenced.
            dip::Tensor const& Tensor() const { return tensor_; }
            /// The number of samples in the pixel referenced.
            dip::uint TensorElements() const { return tensor_.Elements(); }
            /// Is it a scalar pixel?
            bool IsScalar() const { return tensor_.IsScalar(); }
            /// The stride to use to access the various samples in the pixel referenced.
            dip::sint TensorStride() const { return tensorStride_; }

            /// \brief Change the tensor shape, without changing the number of tensor elements.
            Pixel& ReshapeTensor( dip::uint rows, dip::uint cols ) {
               DIP_THROW_IF( tensor_.Elements() != rows * cols, "Cannot reshape tensor to requested sizes" );
               tensor_.ChangeShape( rows );
               return *this;
            }

            /// \brief Change the tensor shape, without changing the number of tensor elements.
            Pixel& ReshapeTensor( dip::Tensor const& other ) {
               tensor_.ChangeShape( other );
               return *this;
            }

            /// \brief Change the tensor to a vector, without changing the number of tensor elements.
            Pixel& ReshapeTensorAsVector() {
               tensor_.ChangeShape();
               return *this;
            }

            /// \brief Change the tensor to a diagonal matrix, without changing the number of tensor elements.
            Pixel& ReshapeTensorAsDiagonal() {
               dip::Tensor other{ dip::Tensor::Shape::DIAGONAL_MATRIX, tensor_.Elements(), tensor_.Elements() };
               tensor_.ChangeShape( other );
               return *this;
            }

            /// Indexing into a `%Pixel` retrieves a reference to the specific sample.
            Sample operator[]( dip::uint index ) const {
               DIP_ASSERT( index < tensor_.Elements() );
               dip::uint sz = dataType_.SizeOf();
               return Sample(
                     static_cast< uint8* >( origin_ ) + static_cast< dip::sint >( sz * index ) * tensorStride_,
                     dataType_ );
            }
            /// Indexing into a `%Pixel` retrieves a reference to the specific sample, `indices` must have one or two elements.
            Sample operator[]( UnsignedArray const& indices ) const {
               DIP_START_STACK_TRACE
                  dip::uint index = tensor_.Index( indices );
                  return operator[]( index );
               DIP_END_STACK_TRACE
            }

            /// \brief Extracts the tensor elements along the diagonal.
            Pixel Diagonal() const {
               Pixel out( *this );
               out.tensor_.ExtractDiagonal( out.tensorStride_ );
               return out;
            }

            /// \brief Extracts the tensor elements along the given row. The tensor representation must be full
            /// (i.e. no symmetric or triangular matrices).
            Pixel TensorRow( dip::uint index ) const {
               DIP_THROW_IF( index >= tensor_.Rows(), E::INDEX_OUT_OF_RANGE );
               Pixel out( *this );
               DIP_START_STACK_TRACE
                  dip::sint offset = out.tensor_.ExtractRow( index, out.tensorStride_ );
                  out.origin_ = static_cast< uint8* >( out.origin_ ) + offset * static_cast< dip::sint >( dataType_.SizeOf() );
               DIP_END_STACK_TRACE
               return out;
            }

            /// \brief Extracts the tensor elements along the given column. The tensor representation must be full
            /// (i.e. no symmetric or triangular matrices).
            Pixel TensorColumn( dip::uint index ) const {
               DIP_THROW_IF( index >= tensor_.Columns(), E::INDEX_OUT_OF_RANGE );
               Pixel out( *this );
               DIP_START_STACK_TRACE
                  dip::sint offset = out.tensor_.ExtractColumn( index, out.tensorStride_ );
                  out.origin_ = static_cast< uint8* >( out.origin_ ) + offset * static_cast< dip::sint >( dataType_.SizeOf() );
               DIP_END_STACK_TRACE
               return out;
            }

            /// \brief Extracts the real component of the pixel values, returns an identical copy if the data type is
            /// not complex.
            Pixel Real() const {
               Pixel out = *this;
               if( dataType_.IsComplex() ) {
                  // Change data type
                  out.dataType_ = dataType_ == DT_SCOMPLEX ? DT_SFLOAT : DT_DFLOAT;
                  // Sample size is halved, meaning stride must be doubled
                  out.tensorStride_ *= 2;
               }
               return out;
            }

            /// \brief Extracts the imaginary component of the pixel values, throws an exception if the data type
            /// is not complex.
            Pixel Imaginary() const {
               DIP_THROW_IF( !dataType_.IsComplex(), E::DATA_TYPE_NOT_SUPPORTED );
               Pixel out = *this;
               // Change data type
               out.dataType_ = dataType_ == DT_SCOMPLEX ? DT_SFLOAT : DT_DFLOAT;
               // Sample size is halved, meaning stride must be doubled
               out.tensorStride_ *= 2;
               // Change the offset
               out.origin_ = static_cast< uint8* >( out.origin_ ) + out.dataType_.SizeOf();
               return out;
            }

            /// \brief An iterator to iterate over the samples in the pixel. Mutable forward iterator.
            class Iterator {
               public:
                  using iterator_category = std::forward_iterator_tag;
                  using value_type = Sample;
                  using difference_type = dip::sint;
                  using reference = value_type&;
                  using pointer = value_type*;

                  Iterator() : value_( nullptr, DT_BIN ), tensorStride_( 0 ) {}

                  void swap( Iterator& other ) {
                     value_.swap( other.value_ );
                     std::swap( tensorStride_, other.tensorStride_ );
                  }

                  reference operator*() { return value_; }
                  pointer operator->() { return &value_; }

                  Iterator& operator++() {
                     value_.origin_ = static_cast< uint8* >( value_.origin_ ) +
                                      tensorStride_ * static_cast< dip::sint >( value_.dataType_.SizeOf() );
                     return *this;
                  }
                  Iterator operator++( int ) { Iterator tmp( *this ); operator++(); return tmp; }

                  bool operator==( Iterator const& other ) const { return value_.Origin() == other.value_.Origin(); }
                  bool operator!=( Iterator const& other ) const { return !operator==( other ); }

               protected:
                  value_type value_;
                  dip::sint tensorStride_;

                  // `dip::Image::Pixel` needs to use the private constructor, as do the generic image iterators.
                  friend class Pixel;
                  template< typename T > friend class dip::GenericImageIterator;
                  template< dip::uint N, typename T > friend class dip::GenericJointImageIterator;

                  Iterator( void* origin, dip::DataType dataType, dip::sint tensorStride ):
                        value_( origin, dataType ),
                        tensorStride_( tensorStride ) {}
                  Iterator( void* origin, dip::DataType dataType, dip::sint tensorStride, dip::uint index ) :
                        value_( static_cast< uint8* >( origin ) + tensorStride * static_cast< dip::sint >( index * dataType.SizeOf() ), dataType ),
                        tensorStride_( tensorStride ) {}
            };

            /// Returns an iterator to the first sample in the pixel.
            Iterator begin() const { return Iterator( origin_, dataType_, tensorStride_ ); }
            /// Returns an iterator to one past the last sample in the pixel.
            Iterator end() const { return Iterator( origin_, dataType_, tensorStride_, tensor_.Elements() ); }

            /// True if all tensor elements are non-zero.
            bool All() const {
               for( auto it = begin(); it != end(); ++it ) {
                  if( !( it->As< bool >() )) {
                     return false;
                  }
               }
               return true;
            }

            /// True if one tensor element is non-zero.
            bool Any() const {
               for( auto it = begin(); it != end(); ++it ) {
                  if( it->As< bool >() ) {
                     return true;
                  }
               }
               return false;
            }

            /// \brief Compound assignment operator.
            template< typename T >
            Pixel& operator+=( T const& rhs );
            /// \brief Compound assignment operator.
            template< typename T >
            Pixel& operator-=( T const& rhs );
            /// \brief Compound assignment operator.
            template< typename T >
            Pixel& operator*=( T const& rhs );
            /// \brief Compound assignment operator.
            template< typename T >
            Pixel& operator/=( T const& rhs );
            /// \brief Compound assignment operator.
            template< typename T >
            Pixel& operator%=( T const& rhs );
            /// \brief Bit-wise compound assignment operator.
            template< typename T >
            Pixel& operator&=( T const& rhs );
            /// \brief Bit-wise compound assignment operator.
            template< typename T >
            Pixel& operator|=( T const& rhs );
            /// \brief Bit-wise compound assignment operator.
            template< typename T >
            Pixel& operator^=( T const& rhs );

         protected:
            std::vector< uint8 > buffer_;
            void* origin_;
            dip::DataType dataType_;
            dip::Tensor tensor_;
            dip::sint tensorStride_ = 1;
      };

      /// \brief Derived from `dip::Image::Sample`, works identically except it implicitly converts to type `T`.
      template< class T >
      class CastSample : public Sample {
         public:
            using Sample::Sample;
            CastSample( Sample&& sample ) : Sample( std::move( sample )) {}
            using Sample::operator=;
            operator T() const { return As< T >(); }
      };

      /// \brief Derived from `dip::Image::Pixel`, works identically except it implicitly converts to type `T`.
      template< class T >
      class CastPixel : public Pixel {
            friend class Image;
            template< typename S > friend class dip::GenericImageIterator;
            template< dip::uint N, typename S > friend class dip::GenericJointImageIterator;
         public:
            using Pixel::Pixel;
            CastPixel( Pixel&& pixel ) : Pixel( std::move( pixel )) {}
            using Pixel::operator=;
            operator T() const { return As< T >(); }
            CastSample< T > operator[]( dip::uint index ) const { return Pixel::operator[]( index ); }
            CastSample< T > operator[]( UnsignedArray const& indices ) const { return Pixel::operator[]( indices ); }
      };

      //
      // Constructor
      //

      /// \name Constructors
      /// \{

      /// \brief The default-initialized image is 0D (an empty sizes array), one tensor element, dip::DT_SFLOAT,
      /// and raw (it has no data segment).
      Image() = default;

      // Copy constructor, move constructor and destructor are all default.
      Image( Image const& ) = default;
      Image( Image&& ) = default;
      ~Image() = default;

      /// \brief The copy assignment does not copy pixel data, the LHS shares the data pointer with the RHS, except
      /// in the case where the LHS image has an external interface set. See \ref external_interface.
      Image& operator=( Image const& rhs ) {
         if( externalInterface_ && ( externalInterface_ != rhs.externalInterface_ )) {
            // Copy pixel data too
            this->Copy( rhs );
         } else {
            // Do what the default copy assignment would do
            dataType_ = rhs.dataType_;
            sizes_ = rhs.sizes_;
            strides_ = rhs.strides_;
            tensor_ = rhs.tensor_;
            tensorStride_ = rhs.tensorStride_;
            protect_ = rhs.protect_;
            colorSpace_ = rhs.colorSpace_;
            pixelSize_ = rhs.pixelSize_;
            dataBlock_ = rhs.dataBlock_;
            origin_ = rhs.origin_;
            externalData_ = rhs.externalData_;
            externalInterface_ = rhs.externalInterface_;
         }
         return *this;
      }

      /// \brief The move assignment copies the data in the case where the LHS image has an external interface set.
      /// See \ref external_interface.
      Image& operator=( Image&& rhs ) {
         if( externalInterface_ && ( externalInterface_ != rhs.externalInterface_ )) {
            // Copy pixel data too
            this->Copy( rhs );
         } else {
            // Do what the default move assignment would do
            this->swap( rhs );
         }
         return *this;
      }

      /// \brief Forged image of given sizes and data type. The data is left uninitialized.
      explicit Image( UnsignedArray const& sizes, dip::uint tensorElems = 1, dip::DataType dt = DT_SFLOAT ) :
            dataType_( dt ),
            sizes_( sizes ),
            tensor_( tensorElems ) {
         TestSizes( sizes_ );
         Forge();
      }

      /// \brief Create a 0-D image with the data type, tensor shape, and values of `pixel`.
      ///
      /// Note that `pixel` can be created through an initializer list. Thus, the following
      /// is a valid way of creating a 0-D tensor image with 3 tensor components:
      ///
      /// ```cpp
      ///     dip::Image image( { 10.0f, 1.0f, 0.0f } );
      /// ```
      ///
      /// The image in the example above will be of type `dip::DT_SFLOAT`.
      explicit Image( Pixel const& pixel ) :
            dataType_( pixel.DataType() ),
            tensor_( pixel.Tensor() ),
            tensorStride_( 1 ) {
         Forge();
         uint8 const* src = static_cast< uint8 const* >( pixel.Origin() );
         uint8* dest = static_cast< uint8* >( origin_ );
         dip::uint sz = dataType_.SizeOf();
         dip::sint srcStep = pixel.TensorStride() * static_cast< dip::sint >( sz );
         dip::sint destStep = tensorStride_ * static_cast< dip::sint >( sz );
         for( dip::uint ii = 0; ii < tensor_.Elements(); ++ii ) {
            std::memcpy( dest, src, sz );
            src += srcStep;
            dest += destStep;
         }
      }

      /// \brief Create a 0-D image with with data type `dt`, and tensor shape and values of `pixel`.
      ///
      /// Note that `pixel` can be created through an initializer list. Thus, the following
      /// is a valid way of creating a 0-D tensor image with 3 tensor components:
      ///
      /// ```cpp
      ///     dip::Image image( { 10, 1, 0 }, dip::DT_SFLOAT );
      /// ```
      ///
      /// The image in the example above will be of type `dip::DT_SFLOAT`.
      explicit Image( Pixel const& pixel, dip::DataType dt ) :
            dataType_( dt ),
            tensor_( pixel.Tensor() ),
            tensorStride_( 1 ) {
         Forge();
         uint8 const* src = static_cast< uint8 const* >( pixel.Origin() );
         uint8* dest = static_cast< uint8* >( origin_ );
         dip::sint srcStep = pixel.TensorStride() * static_cast< dip::sint >( pixel.DataType().SizeOf() );
         dip::sint destStep = tensorStride_ * static_cast< dip::sint >( dataType_.SizeOf() );
         for( dip::uint ii = 0; ii < tensor_.Elements(); ++ii ) {
            detail::CastSample( pixel.DataType(), src, dataType_, dest );
            src += srcStep;
            dest += destStep;
         }
      }

      /// \brief Create a 0-D image with the data type and value of `sample`.
      ///
      /// Note that `sample` can be created by implicit cast from any numeric value. Thus, the following
      /// are valid ways of creating a 0-D image:
      ///
      /// ```cpp
      ///     dip::Image image( 10.0f );
      ///     dip::Image complex_image( dip::dcomplex( 3, 4 ));
      /// ```
      ///
      /// The images in the examples above will be of type `dip::DT_SFLOAT` and `dip::DCOMPLEX`.
      explicit Image( Sample const& sample ) :
            dataType_( sample.DataType() ) {
         Forge();
         uint8 const* src = static_cast< uint8 const* >( sample.Origin());
         dip::uint sz = dataType_.SizeOf();
         std::memcpy( origin_, src, sz );
      }

      /// \brief Create a 0-D image with data type `dt` and value of `sample`.
      ///
      /// Note that `sample` can be created by implicit cast from any numeric value. Thus, the following
      /// is a valid way of creating a 0-D image:
      ///
      /// ```cpp
      ///     dip::Image image( 10, dip::DT_SFLOAT );
      /// ```
      ///
      /// The image in the example above will be of type `dip::DT_SFLOAT`.
      explicit Image( Sample const& sample, dip::DataType dt ) :
            dataType_( dt ) {
         Forge();
         detail::CastSample( sample.DataType(), sample.Origin(), dataType_, origin_ );
      }

      // This one is to disambiguate calling with a single initializer list. We don't mean UnsignedArray, we mean Pixel.
      template< typename T, typename std::enable_if< IsSampleType< T >::value, int >::type = 0 >
      explicit Image( std::initializer_list< T > values ) {
         Image tmp{ Pixel( values ) };
         swap( tmp ); // a way of calling a different constructor.
      }

      // This one is to disambiguate calling with a single initializer list. We don't mean UnsignedArray, we mean Pixel.
      template< typename T, typename std::enable_if< IsSampleType< T >::value, int >::type = 0 >
      explicit Image( std::initializer_list< T > values, dip::DataType dt ) {
         Image tmp{ Pixel( values ), dt };
         swap( tmp ); // a way of calling a different constructor.
      }

      /// \brief Create an image around existing data.
      ///
      /// `data` is a shared pointer used to manage the lifetime of the data segment.
      /// If the image is supposed to take ownership, put a pointer to the data segment or the object
      /// that owns it in `data`, with a deleter function that will delete the data segment or object
      /// when the image is stripped or deleted. Otherwise, use `dip::NonOwnedRefToDataSegment` to
      /// create a shared pointer without a deleter function, implying ownership is not transferred.
      ///
      /// `origin` is the pointer to the first pixel. It must be a valid pointer. This is typically,
      /// but not necessarily, the same pointer as used in `data`.
      ///
      /// `dataType` and `sizes` must be set appropriately. `strides` must either have the same number
      /// of elements as `sizes`, or be an empty array. If `strides` is an empty array, normal strides
      /// will be assumed (i.e. row-major, with tensor elements for one pixel stored contiguously). In
      /// this case, `tensorStride` will be ignored. `tensor` defaults to scalar (i.e. a single tensor
      /// element). No tests will be performed on the validity of the values passed in,
      /// except to enforce a few class invariants.
      ///
      /// See \ref external_interface for information about the `externalInterface` parameter.
      ///
      /// See \ref use_external_data for more information on how to use this function.
      Image(
            DataSegment const& data,
            void* origin,
            dip::DataType dataType,
            UnsignedArray const& sizes,
            IntegerArray const& strides = {},
            dip::Tensor const& tensor = {},
            dip::sint tensorStride = 1,
            dip::ExternalInterface* externalInterface = nullptr
      ) :
            dataType_( dataType ),
            sizes_( sizes ),
            strides_( strides ),
            tensor_( tensor ),
            tensorStride_( tensorStride ),
            dataBlock_( data ),
            origin_( origin ),
            externalData_( true ),
            externalInterface_( externalInterface ) {
         DIP_THROW_IF( data.get() == nullptr, "Bad data pointer" );
         DIP_THROW_IF( origin == nullptr, "Bad origin pointer" );
         TestSizes( sizes_ );
         dip::uint nDims = sizes_.size();
         if( strides_.empty() ) {
            SetNormalStrides();
         } else {
            DIP_THROW_IF( strides_.size() != nDims, "Strides array size does not match image dimensionality" );
         }
      }

      /// \brief Create a new forged image similar to `this`. The data is not copied, and left uninitialized.
      Image Similar() {
         Image out;
         out.CopyProperties( *this );
         out.Forge();
         return out;
      }

      /// \brief Create a new forged image similar to `this`, but with different data type. The data is not copied, and left uninitialized.
      Image Similar( dip::DataType dt ) {
         Image out;
         out.CopyProperties( *this );
         out.dataType_ = dt;
         out.Forge();
         return out;
      }

      /// \}

      //
      // Sizes
      //

      /// \name Sizes
      /// \{

      /// \brief Get the number of spatial dimensions.
      dip::uint Dimensionality() const {
         return sizes_.size();
      }

      /// \brief Get a const reference to the sizes array (image size).
      UnsignedArray const& Sizes() const {
         return sizes_;
      }

      /// \brief Get the image size along a specific dimension, without test for dimensionality.
      dip::uint Size( dip::uint dim ) const {
         return sizes_[ dim ];
      }

      /// \brief Get the number of pixels.
      dip::uint NumberOfPixels() const {
         return sizes_.product();
      }

      /// \brief Get the number of samples.
      dip::uint NumberOfSamples() const {
         return NumberOfPixels() * TensorElements();
      }

      /// \brief Set the image sizes. The image must be raw.
      void SetSizes( UnsignedArray const& d ) {
         DIP_THROW_IF( IsForged(), E::IMAGE_NOT_RAW );
         TestSizes( d );
         sizes_ = d;
      }

      // Sets the sizes of the image. Do not use this function unless you know what you're doing.
      void dip__SetSizes( UnsignedArray const& d ) {
         sizes_ = d;
      }

      /// \}

      //
      // Strides
      //

      /// \name Strides
      /// \{

      /// \brief Get a const reference to the strides array.
      IntegerArray const& Strides() const {
         return strides_;
      }

      /// \brief Get the stride along a specific dimension, without test for dimensionality.
      dip::sint Stride( dip::uint dim ) const {
         return strides_[ dim ];
      }

      /// \brief Get the tensor stride.
      dip::sint TensorStride() const {
         return tensorStride_;
      }

      /// \brief Set the strides array. The image must be raw.
      void SetStrides( IntegerArray const& s ) {
         DIP_THROW_IF( IsForged(), E::IMAGE_NOT_RAW );
         strides_ = s;
      }

      /// \brief Set the tensor stride. The image must be raw.
      void SetTensorStride( dip::sint ts ) {
         DIP_THROW_IF( IsForged(), E::IMAGE_NOT_RAW );
         tensorStride_ = ts;
      }

      /// \brief Test if all the pixels are contiguous.
      ///
      /// If all pixels are contiguous, you can traverse the whole image,
      /// accessing each of the pixels, using a single stride with a value
      /// of 1. To do so, you don't necessarily start at the origin: if any
      /// of the strides is negative, the origin of the contiguous data will
      /// be elsewhere.
      /// Use `dip::Image::GetSimpleStrideAndOrigin` to get a pointer to the origin
      /// of the contiguous data.
      ///
      /// The image must be forged.
      ///
      /// \see GetSimpleStrideAndOrigin, HasSimpleStride, HasNormalStrides, IsSingletonExpanded, Strides, TensorStride.
      bool HasContiguousData() const {
         DIP_THROW_IF( !IsForged(), E::IMAGE_NOT_FORGED );
         dip::uint size = NumberOfPixels() * TensorElements();
         dip::sint start;
         dip::uint sz;
         GetDataBlockSizeAndStartWithTensor( sz, start );
         return sz == size;
      }

      /// \brief Test if strides are as by default. The image must be forged.
      DIP_EXPORT bool HasNormalStrides() const;

      /// \brief Test if the image has been singleton expanded.
      ///
      /// If any dimension is larger than 1, but has a stride of 0, it means that a single pixel is being used
      /// across that dimension. The methods `dip::Image::ExpandSingletonDimension` and
      /// `dip::Image::ExpandSingletonTensor` create such dimensions.
      ///
      /// The image must be forged.
      ///
      /// \see HasContiguousData, HasNormalStrides, ExpandSingletonDimension, ExpandSingletonTensor.
      DIP_EXPORT bool IsSingletonExpanded() const;

      /// \brief Test if the whole image can be traversed with a single stride
      /// value.
      ///
      /// This is similar to `dip::Image::HasContiguousData`, but the stride
      /// value can be larger than 1.
      /// Use `dip::Image::GetSimpleStrideAndOrigin` to get a pointer to the origin
      /// of the contiguous data. Note that this only tests spatial
      /// dimensions, the tensor dimension must still be accessed separately.
      ///
      /// The image must be forged.
      ///
      /// \see GetSimpleStrideAndOrigin, HasContiguousData, HasNormalStrides, Strides, TensorStride.
      bool HasSimpleStride() const {
         void* p;
         dip::sint s;
         GetSimpleStrideAndOrigin( s, p );
         return p != nullptr;
      }

      /// \brief Return a pointer to the start of the data and a single stride to
      /// walk through all pixels.
      ///
      /// If this is not possible, the function
      /// sets `porigin==nullptr`. Note that this only tests spatial dimensions,
      /// the tensor dimension must still be accessed separately.
      ///
      /// The `stride` returned is always positive.
      ///
      /// The image must be forged.
      ///
      /// \see HasSimpleStride, HasContiguousData, HasNormalStrides, Strides, TensorStride, Data.
      DIP_EXPORT void GetSimpleStrideAndOrigin( dip::sint& stride, void*& origin ) const;

      /// \brief Checks to see if `other` and `this` have their dimensions ordered in
      /// the same way.
      ///
      /// Traversing more than one image using simple strides is only
      /// possible if they have their dimensions ordered in the same way, otherwise
      /// the simple stride does not visit the pixels in the same order in the
      /// various images.
      ///
      /// The images must be forged.
      ///
      /// \see HasSimpleStride, GetSimpleStrideAndOrigin, HasContiguousData.
      DIP_EXPORT bool HasSameDimensionOrder( Image const& other ) const;

      /// \}

      //
      // Tensor
      //

      /// \name Tensor
      /// \{

      /// \brief Get the tensor sizes. The array returned can have 0, 1 or
      /// 2 elements, as those are the allowed tensor dimensionalities.
      UnsignedArray TensorSizes() const {
         return tensor_.Sizes();
      }

      /// \brief Get the number of tensor elements (i.e. the number of samples per pixel),
      /// the product of the elements in the array returned by TensorSizes.
      dip::uint TensorElements() const {
         return tensor_.Elements();
      }

      /// \brief Get the number of tensor columns.
      dip::uint TensorColumns() const {
         return tensor_.Columns();
      }

      /// \brief Get the number of tensor rows.
      dip::uint TensorRows() const {
         return tensor_.Rows();
      }

      /// \brief Get the tensor shape.
      enum dip::Tensor::Shape TensorShape() const {
         return tensor_.TensorShape();
      }

      // Note: This function is the reason we refer to the Tensor class as
      // dip::Tensor everywhere in this file.

      /// \brief Get the tensor shape.
      dip::Tensor const& Tensor() const {
         return tensor_;
      }

      /// \brief True for non-tensor (grey-value) images.
      bool IsScalar() const {
         return tensor_.IsScalar();
      }

      /// \brief True for vector images, where the tensor is one-dimensional.
      bool IsVector() const {
         return tensor_.IsVector();
      }

      /// \brief True for square matrix images, independent from how they are stored.
      bool IsSquare() const {
         return tensor_.IsSquare();
      }

      /// \brief Set tensor sizes. The image must be raw.
      void SetTensorSizes( UnsignedArray const& tdims ) {
         DIP_THROW_IF( IsForged(), E::IMAGE_NOT_RAW );
         tensor_.SetSizes( tdims );
      }

      /// \brief Set tensor sizes. The image must be raw.
      void SetTensorSizes( dip::uint nelems ) {
         DIP_THROW_IF( IsForged(), E::IMAGE_NOT_RAW );
         tensor_.SetVector( nelems );
      }

      // Sets the tensor sizes. Do not use this function unless you know what you're doing.
      void dip__SetTensorSizes( dip::uint nelems ) {
         tensor_.SetVector( nelems );
      }

      /// \}

      //
      // Data type
      //

      /// \name Data type
      /// \{

      // Note: This function is the reason we refer to the DataType class as
      // dip::DataType everywhere in this file.

      /// \brief Get the image's data type.
      dip::DataType DataType() const {
         return dataType_;
      }

      /// \brief Set the image's data type. The image must be raw.
      void SetDataType( dip::DataType dt ) {
         DIP_THROW_IF( IsForged(), E::IMAGE_NOT_RAW );
         dataType_ = dt;
      }

      /// \}

      //
      // Color space
      //

      /// \name Color space
      /// \{

      /// \brief Get the image's color space name.
      String const& ColorSpace() const {
         return colorSpace_;
      }

      /// \brief Returns true if the image is in color, false if the image is grey-valued.
      bool IsColor() const {
         return !colorSpace_.empty();
      }

      /// \brief Sets the image's color space name. This causes the image to be a color image,
      /// but will cause errors to occur (eventually, not immediately) if the number of tensor elements
      /// does not match the expected number of channels for the given color space.
      void SetColorSpace( String const& cs ) {
         colorSpace_ = cs;
      }

      /// \brief Resets the image's color space information, turning the image into a non-color image.
      void ResetColorSpace() {
         colorSpace_.clear();
      }

      /// \}

      //
      // Pixel size
      //

      /// \name Pixel size
      /// \{

      // Note: This function is the reason we refer to the PixelSize class as
      // dip::PixelSize everywhere in this file.

      /// \brief Get the pixels' size in physical units, by reference, allowing to modify it at will.
      dip::PixelSize& PixelSize() {
         return pixelSize_;
      }

      /// \brief Get the pixels' size in physical units.
      dip::PixelSize const& PixelSize() const {
         return pixelSize_;
      }

      /// \brief Get the pixels' size in physical units along the given dimension.
      PhysicalQuantity PixelSize( dip::uint dim ) const {
         return pixelSize_[ dim ];
      }

      /// \brief Set the pixels' size.
      void SetPixelSize( dip::PixelSize const& ps ) {
         pixelSize_ = ps;
      }

      /// \brief Reset the pixels' size, so that `HasPixelSize` returns false.
      void ResetPixelSize() {
         pixelSize_.Clear();
      }

      /// \brief Returns true if the pixel has physical dimensions.
      bool HasPixelSize() const {
         return pixelSize_.IsDefined();
      }

      /// \brief Returns true if the pixel has the same size in all dimensions.
      bool IsIsotropic() const {
         return pixelSize_.IsIsotropic();
      }

      /// \brief Converts a size in pixels to a size in physical units.
      PhysicalQuantityArray PixelsToPhysical( FloatArray const& in ) const {
         return pixelSize_.ToPhysical( in );
      }

      /// \brief Converts a size in physical units to a size in pixels.
      FloatArray PhysicalToPixels( PhysicalQuantityArray const& in ) const {
         return pixelSize_.ToPixels( in );
      }

      /// \}

      //
      // Utility functions
      //

      /// \name Utility functions
      /// \{

      /// \brief Compare properties of an image against a template, either
      /// returns true/false or throws an error.
      DIP_EXPORT bool CompareProperties(
            Image const& src,
            Option::CmpProps cmpProps,
            Option::ThrowException throwException = Option::ThrowException::DO_THROW
      ) const;

      /// \brief Check image properties, either returns true/false or throws an error.
      DIP_EXPORT bool CheckProperties(
            dip::uint ndims,
            dip::DataType::Classes dts,
            Option::ThrowException throwException = Option::ThrowException::DO_THROW
      ) const;

      /// \brief Check image properties, either returns true/false or throws an error.
      DIP_EXPORT bool CheckProperties(
            dip::uint ndims,
            dip::uint tensorElements,
            dip::DataType::Classes dts,
            Option::ThrowException throwException = Option::ThrowException::DO_THROW
      ) const;

      /// \brief Check image properties, either returns true/false or throws an error.
      DIP_EXPORT bool CheckProperties(
            UnsignedArray const& sizes,
            dip::DataType::Classes dts,
            Option::ThrowException throwException = Option::ThrowException::DO_THROW
      ) const;

      /// \brief Check image properties, either returns true/false or throws an error.
      DIP_EXPORT bool CheckProperties(
            UnsignedArray const& sizes,
            dip::uint tensorElements,
            dip::DataType::Classes dts,
            Option::ThrowException throwException = Option::ThrowException::DO_THROW
      ) const;

      /// \brief Check image properties for a mask image, either returns true/false or throws an error.
      DIP_EXPORT bool CheckIsMask(
            UnsignedArray const& sizes,
            Option::AllowSingletonExpansion allowSingletonExpansion = Option::AllowSingletonExpansion::DONT_ALLOW,
            Option::ThrowException throwException = Option::ThrowException::DO_THROW
      ) const;

      /// \brief Copy all image properties from `src`. The image must be raw.
      void CopyProperties( Image const& src ) {
         DIP_THROW_IF( IsForged(), E::IMAGE_NOT_RAW );
         dataType_ = src.dataType_;
         sizes_ = src.sizes_;
         strides_ = src.strides_;
         tensor_ = src.tensor_;
         tensorStride_ = src.tensorStride_;
         colorSpace_ = src.colorSpace_;
         pixelSize_ = src.pixelSize_;
         if( !externalInterface_ ) {
            externalInterface_ = src.externalInterface_;
         }
      }

      /// \brief Copy non-data image properties from `src`.
      ///
      /// The non-data image properties are those that do not influence how the data is stored in
      /// memory: tensor shape, color space, and pixel size. The number of tensor elements of the
      /// the two images must match. The image must be forged.
      ///
      /// \see CopyProperties, ResetNonDataProperties
      void CopyNonDataProperties( Image const& src ) {
         DIP_THROW_IF( !IsForged(), E::IMAGE_NOT_FORGED );
         DIP_THROW_IF( tensor_.Elements() != src.tensor_.Elements(), E::NTENSORELEM_DONT_MATCH );
         tensor_ = src.tensor_;
         colorSpace_ = src.colorSpace_;
         pixelSize_ = src.pixelSize_;
      }

      /// \brief Reset non-data image properties.
      ///
      /// The non-data image properties are those that do not influence how the data is stored in
      /// memory: tensor shape, color space, and pixel size.
      ///
      /// \see CopyNonDataProperties, Strip
      void ResetNonDataProperties() {
         tensor_ = {};
         colorSpace_ = {};
         pixelSize_ = {};
      }

      /// \brief Swaps `this` and `other`.
      void swap( Image& other ) {
         using std::swap;
         swap( dataType_, other.dataType_ );
         swap( sizes_, other.sizes_ );
         swap( strides_, other.strides_ );
         swap( tensor_, other.tensor_ );
         swap( tensorStride_, other.tensorStride_ );
         swap( protect_, other.protect_ );
         swap( colorSpace_, other.colorSpace_ );
         swap( pixelSize_, other.pixelSize_ );
         swap( dataBlock_, other.dataBlock_ );
         swap( origin_, other.origin_ );
         swap( externalData_, other.externalData_ );
         swap( externalInterface_, other.externalInterface_ );
      }

      /// \}

      //
      // Data
      // Defined in src/library/image_data.cpp
      //

      /// \name Data
      /// \{

      /// \brief Get pointer to the data segment.
      ///
      /// This is useful to identify
      /// the data segment, but not to access the pixel data stored in
      /// it. Use `dip::Image::Origin` instead. The image must be forged.
      ///
      /// The pointer returned could be tangentially related to the data segment, if
      /// `dip::Image::IsExternalData` is true.
      ///
      /// \see Origin, IsShared, ShareCount, SharesData, IsExternalData.
      void* Data() const {
         DIP_THROW_IF( !IsForged(), E::IMAGE_NOT_FORGED );
         return dataBlock_.get();
      }

      /// \brief Check to see if the data segment is shared with other images.
      ///
      /// \see Data, ShareCount, SharesData, IsExternalData.
      bool IsShared() const {
         return IsForged() && ( dataBlock_.use_count() > 1 );
      }

      /// \brief Get the number of images that share their data with this image.
      ///
      /// For normal images. the count is always at least 1. If the count is
      /// larger than 1, `dip::Image::IsShared` is true.
      ///
      /// If `this` encapsulates external data (`dip::Image::IsExternalData` is true),
      /// then the share count is not necessarily correct, as it might not count
      /// the uses of the source data.
      ///
      /// The image must be forged.
      ///
      /// \see Data, IsShared, SharesData, IsExternalData.
      dip::uint ShareCount() const {
         DIP_THROW_IF( !IsForged(), E::IMAGE_NOT_FORGED );
         return static_cast< dip::uint >( dataBlock_.use_count() );
      }

      /// \brief Determine if `this` shares its data pointer with `other`.
      ///
      /// Note that sharing the data pointer
      /// does not imply that the two images share any pixel data, as it
      /// is possible for the two images to represent disjoint windows
      /// into the same data block. To determine if any pixels are shared,
      /// use `dip::Image::Aliases`.
      ///
      /// \see Aliases, IsIdenticalView, IsOverlappingView, Data, IsShared, ShareCount, IsExternalData.
      bool SharesData( Image const& other ) const {
         return IsForged() && other.IsForged() && ( dataBlock_ == other.dataBlock_ );
      }

      /// \brief Returns true if the data segment was not allocated by *DIPlib*. See \ref external_data_segment.
      bool IsExternalData() const {
         return IsForged() && externalData_;
      }

      /// \brief Determine if `this` shares any samples with `other`.
      ///
      /// If `true`, writing into this image will change the data in
      /// `other`, and vice-versa.
      ///
      /// \see SharesData, IsIdenticalView, IsOverlappingView, Alias.
      DIP_EXPORT bool Aliases( Image const& other ) const;

      /// \brief Determine if `this` and `other` offer an identical view of the
      /// same set of pixels.
      ///
      /// If `true`, changing one sample in this image will change the same sample in `other`.
      ///
      /// \see SharesData, Aliases, IsOverlappingView.
      bool IsIdenticalView( Image const& other ) const {
         DIP_THROW_IF( !IsForged(), E::IMAGE_NOT_FORGED );
         // We don't need to check dataBlock_ here, as origin_ is a pointer, not an offset.
         return IsForged() && other.IsForged() &&
                ( origin_ == other.origin_ ) &&
                ( dataType_ == other.dataType_ ) &&
                ( sizes_ == other.sizes_ ) &&
                ( tensor_.Elements() == other.tensor_.Elements()) &&
                ( strides_ == other.strides_ ) &&
                ( tensorStride_ == other.tensorStride_ );
      }

      /// \brief Determine if `this` and `other` offer different views of the
      /// same data segment, and share at least one sample.
      ///
      /// If `true`, changing one
      /// sample in this image might change a different sample in `other`.
      /// An image with an overlapping view of an input image cannot be used as output to a
      /// filter, as it might change input data that still needs to be used. Use this function
      /// to test whether to use the existing data segment or allocate a new one.
      ///
      /// \see SharesData, Aliases, IsIdenticalView.
      bool IsOverlappingView( Image const& other ) const {
         // Aliases checks for both images to be forged.
         return Aliases( other ) && !IsIdenticalView( other );
      }

      /// \brief Determine if `this` and any of those in `other` offer different views of the
      /// same data segment, and share at least one sample.
      ///
      /// If `true`, changing one
      /// sample in this image might change a different sample in at least one image in `other`.
      /// An image with an overlapping view of an input image cannot be used as output to a
      /// filter, as it might change input data that still needs to be used. Use this function
      /// to test whether to use the existing data segment or allocate a new one.
      ///
      /// \see SharesData, Aliases, IsIdenticalView.
      bool IsOverlappingView( ImageConstRefArray const& other ) const {
         for( dip::uint ii = 0; ii < other.size(); ++ii ) {
            Image const& tmp = other[ ii ].get();
            if( IsOverlappingView( tmp )) {
               return true;
            }
         }
         return false;
      }

      /// Determine if `this` and any of those in `other` offer different views of the
      /// same data segment, and share at least one sample.
      ///
      /// If `true`, changing one
      /// sample in this image might change a different sample in at least one image in `other`.
      /// An image with an overlapping view of an input image cannot be used as output to a
      /// filter, as it might change input data that still needs to be used. Use this function
      /// to test whether to use the existing data segment or allocate a new one.
      ///
      /// \see SharesData, Aliases, IsIdenticalView.
      bool IsOverlappingView( ImageArray const& other ) const {
         for( dip::uint ii = 0; ii < other.size(); ++ii ) {
            Image const& tmp = other[ ii ];
            if( IsOverlappingView( tmp )) {
               return true;
            }
         }
         return false;
      }

      /// \brief Allocate data segment.
      ///
      /// This function allocates a memory block
      /// to hold the pixel data. If the stride array is consistent with
      /// size array, and leads to a compact data segment, it is honored.
      /// Otherwise, it is ignored and a new stride array is created that
      /// leads to an image that `dip::Image::HasNormalStrides`. If an
      /// external interface is registered for this image, that interface
      /// may create whatever strides are suitable, may honor or not the
      /// existing stride array, and may or may not produce normal strides.
      DIP_EXPORT void Forge();

      /// \brief Modify image properties and forge the image.
      ///
      /// `%ReForge` has three
      /// signatures that match three image constructors. `%ReForge` will try
      /// to avoid freeing the current data segment and allocating a new one.
      /// This version will cause `this` to be an identical copy of `src`,
      /// but with uninitialized data. The external interface of `src` is
      /// not used, nor are its strides.
      ///
      /// If `this` doesn't match the requested properties, it must be stripped
      /// and forged. If `this` is protected (see `dip::Image::Protect`) and
      /// forged, an exception will be thrown by `dip::Image::Strip`. However,
      /// if `acceptDataTypeChange` is `dip::Option::AcceptDataTypeChange::DO_ALLOW`,
      /// a protected image will keep its
      /// old data type, and no exception will be thrown if this data type
      /// is different from `dt`. Note that other properties much still match
      /// if `this` was forged. Thus, this flag allows `this` to control the
      /// data type of the image, ignoring any requested data type here.
      void ReForge(
            Image const& src,
            Option::AcceptDataTypeChange acceptDataTypeChange = Option::AcceptDataTypeChange::DONT_ALLOW
      ) {
         ReForge( src, src.dataType_, acceptDataTypeChange );
      }

      /// \brief Modify image properties and forge the image.
      ///
      /// `%ReForge` has three
      /// signatures that match three image constructors. `%ReForge` will try
      /// to avoid freeing the current data segment and allocating a new one.
      /// This version will cause `this` to be an identical copy of `src`,
      /// but with a different data type and uninitialized data. The
      /// external interface of `src` is not used, nor are its strides.
      ///
      /// If `this` doesn't match the requested properties, it must be stripped
      /// and forged. If `this` is protected (see `dip::Image::Protect`) and
      /// forged, an exception will be thrown by `dip::Image::Strip`. However,
      /// if `acceptDataTypeChange` is `dip::Option::AcceptDataTypeChange::DO_ALLOW`,
      /// a protected image will keep its
      /// old data type, and no exception will be thrown if this data type
      /// is different from `dt`. Note that other properties much still match
      /// if `this` was forged. Thus, this flag allows `this` to control the
      /// data type of the image, ignoring any requested data type here.
      void ReForge(
            Image const& src,
            dip::DataType dt,
            Option::AcceptDataTypeChange acceptDataTypeChange = Option::AcceptDataTypeChange::DONT_ALLOW
      ) {
         ReForge( src.sizes_, src.tensor_.Elements(), dt, acceptDataTypeChange );
         CopyNonDataProperties( src );
      }

      /// \brief Modify image properties and forge the image.
      ///
      /// `%ReForge` has three
      /// signatures that match three image constructors. `%ReForge` will try
      /// to avoid freeing the current data segment and allocating a new one.
      /// This version will cause `this` to be of the requested sizes and
      /// data type.
      ///
      /// If `this` doesn't match the requested properties, it must be stripped
      /// and forged. If `this` is protected (see `dip::Image::Protect`) and
      /// forged, an exception will be thrown by `dip::Image::Strip`. However,
      /// if `acceptDataTypeChange` is `dip::Option::AcceptDataTypeChange::DO_ALLOW`,
      /// a protected image will keep its
      /// old data type, and no exception will be thrown if this data type
      /// is different from `dt`. Note that other properties much still match
      /// if `this` was forged. Thus, this flag allows `this` to control the
      /// data type of the image, ignoring any requested data type here.
      DIP_EXPORT void ReForge(
            UnsignedArray const& sizes,
            dip::uint tensorElems = 1,
            dip::DataType dt = DT_SFLOAT,
            Option::AcceptDataTypeChange acceptDataTypeChange = Option::AcceptDataTypeChange::DONT_ALLOW
      );

      /// \brief Disassociate the data segment from the image. If there are no
      /// other images using the same data segment, it will be freed.
      void Strip() {
         if( IsForged() ) {
            DIP_THROW_IF( IsProtected(), "Image is protected" );
            dataBlock_ = nullptr; // Automatically frees old memory if no other pointers to it exist.
            origin_ = nullptr;    // Keep this one in sync!
            externalData_ = false;
         }
      }

      /// \brief Test if forged.
      bool IsForged() const {
         return origin_ != nullptr;
      }

      /// \brief Set protection flag.
      ///
      /// A protected image cannot be stripped or reforged. See \ref protect for more information.
      ///
      /// Returns the old setting. This can be used as follows to temporarily
      /// protect an image:
      ///
      /// ```cpp
      ///     bool wasProtected = img.Protect();
      ///     [...] // do your thing
      ///     img.Protect( wasProtected );
      /// ```
      ///
      /// \see IsProtected, Strip
      bool Protect( bool set = true ) {
         bool old = protect_;
         protect_ = set;
         return old;
      }

      /// \brief Test if protected. See `dip::Image::Protect` for information.
      bool IsProtected() const {
         return protect_;
      }

      // Note: This function is the reason we refer to the ExternalInterface class as
      // dip::ExternalInterface everywhere inside the dip::Image class.

      /// \brief Set external interface pointer. The image must be raw. See \ref external_interface.
      void SetExternalInterface( dip::ExternalInterface* ei ) {
         DIP_THROW_IF( IsForged(), E::IMAGE_NOT_RAW );
         externalInterface_ = ei;
      }

      /// \brief Get external interface pointer. See \ref external_interface
      dip::ExternalInterface* ExternalInterface() const {
         return externalInterface_;
      }

      /// \brief Test if an external interface is set. See \ref external_interface
      bool HasExternalInterface() const {
         return externalInterface_ != nullptr;
      }

      /// \}

      //
      // Pointers, offsets, indices
      // Defined in src/library/image_data.cpp
      //

      /// \name Pointers, offsets, indices
      /// \{

      /// \brief Get pointer to the first sample in the image, the first tensor
      /// element at coordinates (0,0,0,...). The image must be forged.
      void* Origin() const {
         DIP_THROW_IF( !IsForged(), E::IMAGE_NOT_FORGED );
         return origin_;
      }

      // Sets the pointer to the first sample in the image. Do not use this function
      // unless you know what you're doing.
      void dip__SetOrigin( void* origin ) {
         origin_ = origin;
      }

      // Shifts the pointer to the first sample in the image by offset. Do not use this
      // function unless you know what you're doing.
      void dip__ShiftOrigin( dip::sint offset ) {
         origin_ = static_cast< uint8* >( origin_ ) + offset * static_cast< dip::sint >( dataType_.SizeOf() );
      }

      /// \brief Get a pointer to the pixel given by the offset.
      ///
      /// Cast the pointer to the right type before use. No check is made on the index.
      ///
      /// The image must be forged.
      ///
      /// \see Origin, Offset, OffsetToCoordinates
      void* Pointer( dip::sint offset ) const {
         DIP_THROW_IF( !IsForged(), E::IMAGE_NOT_FORGED );
         return static_cast< uint8* >( origin_ ) + offset * static_cast< dip::sint >( dataType_.SizeOf() );
      }

      /// \brief Get a pointer to the pixel given by the coordinates index.
      ///
      /// Cast the
      /// pointer to the right type before use. This is not the most efficient
      /// way of indexing many pixels in the image.
      ///
      /// If `coords` is not within the image domain, an exception is thrown.
      ///
      /// The image must be forged.
      ///
      /// \see Origin, Offset, OffsetToCoordinates
      void* Pointer( UnsignedArray const& coords ) const {
         return Pointer( Offset( coords ));
      }

      /// \brief Get a pointer to the pixel given by the coordinates index.
      ///
      /// Cast the
      /// pointer to the right type before use. This is not the most efficient
      /// way of indexing many pixels in the image.
      ///
      /// `coords` can be outside the image domain.
      ///
      /// The image must be forged.
      ///
      /// \see Origin, Offset, OffsetToCoordinates
      void* Pointer( IntegerArray const& coords ) const {
         return Pointer( Offset( coords ));
      }

      /// \brief Compute offset given coordinates.
      ///
      /// The offset needs to be multiplied
      /// by the number of bytes of each sample to become a memory offset
      /// within the image.
      ///
      /// If `coords` is not within the image domain, an exception is thrown.
      ///
      /// The image must be forged.
      ///
      /// \see Origin, Pointer, OffsetToCoordinates
      dip::sint Offset( UnsignedArray const& coords ) const {
         DIP_THROW_IF( !IsForged(), E::IMAGE_NOT_FORGED );
         DIP_THROW_IF( coords.size() != sizes_.size(), E::ARRAY_ILLEGAL_SIZE );
         dip::sint offset = 0;
         for( dip::uint ii = 0; ii < sizes_.size(); ++ii ) {
            DIP_THROW_IF( coords[ ii ] >= sizes_[ ii ], E::INDEX_OUT_OF_RANGE );
            offset += static_cast< dip::sint >( coords[ ii ] ) * strides_[ ii ];
         }
         return offset;
      }

      /// \brief Compute offset given coordinates.
      ///
      /// The offset needs to be multiplied
      /// by the number of bytes of each sample to become a memory offset
      /// within the image.
      ///
      /// `coords` can be outside the image domain.
      ///
      /// The image must be forged.
      ///
      /// \see Origin, Pointer, OffsetToCoordinates
      dip::sint Offset( IntegerArray const& coords ) const {
         DIP_THROW_IF( !IsForged(), E::IMAGE_NOT_FORGED );
         DIP_THROW_IF( coords.size() != sizes_.size(), E::ARRAY_ILLEGAL_SIZE );
         dip::sint offset = 0;
         for( dip::uint ii = 0; ii < sizes_.size(); ++ii ) {
            offset += coords[ ii ] * strides_[ ii ];
         }
         return offset;
      }

      /// \brief Compute coordinates given an offset.
      ///
      /// If the image has any singleton-expanded
      /// dimensions, the computed coordinate along that dimension will always be 0.
      /// This is an expensive operation, use `dip::Image::OffsetToCoordinatesComputer` to make it
      /// more efficient when performing multiple computations in sequence.
      ///
      /// Note that the coordinates must be inside the image domain, if the offset given
      /// does not correspond to one of the image's pixels, the result is meaningless.
      ///
      /// The image must be forged.
      ///
      /// \see Offset, OffsetToCoordinatesComputer, IndexToCoordinates
      UnsignedArray OffsetToCoordinates( dip::sint offset ) const  {
         CoordinatesComputer comp = OffsetToCoordinatesComputer();
         return comp( offset );
      }

      /// \brief Returns a functor that computes coordinates given an offset.
      ///
      /// This is
      /// more efficient than using `dip::Image::OffsetToCoordinates` when repeatedly
      /// computing offsets, but still requires complex calculations.
      ///
      /// The image must be forged.
      ///
      /// \see Offset, OffsetToCoordinates, IndexToCoordinates, IndexToCoordinatesComputer
      CoordinatesComputer OffsetToCoordinatesComputer() const {
         DIP_THROW_IF( !IsForged(), E::IMAGE_NOT_FORGED );
         return CoordinatesComputer( sizes_, strides_ );
      }

      /// \brief Compute linear index (not offset) given coordinates.
      ///
      /// This index is not
      /// related to the position of the pixel in memory, and should not be used
      /// to index many pixels in sequence.
      ///
      /// The image must be forged.
      ///
      /// \see IndexToCoordinates, Offset
      dip::uint Index( UnsignedArray const& coords ) const {
         DIP_THROW_IF( !IsForged(), E::IMAGE_NOT_FORGED );
         DIP_THROW_IF( coords.size() != sizes_.size(), E::ARRAY_ILLEGAL_SIZE );
         dip::uint index = 0;
         for( dip::uint ii = sizes_.size(); ii > 0; ) {
            --ii;
            DIP_THROW_IF( coords[ ii ] >= sizes_[ ii ], E::INDEX_OUT_OF_RANGE );
            index *= sizes_[ ii ];
            index += coords[ ii ];
         }
         return index;
      }

      /// \brief Compute coordinates given a linear index.
      ///
      /// If the image has any singleton-expanded
      /// dimensions, the computed coordinate along that dimension will always be 0.
      /// This is an expensive operation, use `dip::Image::IndexToCoordinatesComputer` to make it
      /// more efficient when performing multiple computations in sequence.
      ///
      /// Note that the coordinates must be inside the image domain, if the index given
      /// does not correspond to one of the image's pixels, the result is meaningless.
      ///
      /// The image must be forged.
      ///
      /// \see Index, Offset, IndexToCoordinatesComputer, OffsetToCoordinates
      UnsignedArray IndexToCoordinates( dip::uint index ) const {
         CoordinatesComputer comp = IndexToCoordinatesComputer();
         return comp( static_cast< dip::sint >( index ));
      }

      /// \brief Returns a functor that computes coordinates given a linear index.
      ///
      /// This is
      /// more efficient than using `dip::Image::IndexToCoordinates`, when repeatedly
      /// computing indices, but still requires complex calculations.
      ///
      /// The image must be forged.
      ///
      /// \see Index, Offset, IndexToCoordinates, OffsetToCoordinates, OffsetToCoordinatesComputer
      DIP_EXPORT CoordinatesComputer IndexToCoordinatesComputer() const;

      /// \}

      //
      // Modifying geometry of a forged image without data copy
      // Defined in src/library/image_manip.cpp
      //

      /// \name Reshaping forged image
      /// These functions change the image object, providing a differently-shaped version of the same data.
      /// No data is copied, and the image contains the same set of samples as before the method call.
      /// \{

      /// \brief Permute dimensions.
      ///
      /// This function allows to re-arrange the dimensions
      /// of the image in any order. It also allows to remove singleton dimensions
      /// (but not to add them, should we add that? how?). For example, given
      /// an image with size `{ 30, 1, 50 }`, and an `order` array of
      /// `{ 2, 0 }`, the image will be modified to have size `{ 50, 30 }`.
      /// Dimension number 1 is not referenced, and was removed (this can only
      /// happen if the dimension has size 1, otherwise an exception will be
      /// thrown!). Dimension 2 was placed first, and dimension 0 was placed second.
      ///
      /// The image must be forged. If it is not, you can simply assign any
      /// new sizes array through Image::SetSizes. The data will never
      /// be copied (i.e. this is a quick and cheap operation).
      ///
      /// \see SwapDimensions, Squeeze, AddSingleton, ExpandDimensionality, Flatten.
      DIP_EXPORT Image& PermuteDimensions( UnsignedArray const& order );

      /// \brief Swap dimensions d1 and d2. This is a simplified version of `PermuteDimensions`.
      ///
      /// The image must be forged. The data will never be copied (i.e. this is a quick and cheap operation).
      ///
      /// \see PermuteDimensions.
      DIP_EXPORT Image& SwapDimensions( dip::uint dim1, dip::uint dim2 );

      /// \brief Make image 1D.
      ///
      /// The image must be forged. If HasSimpleStride,
      /// this is a quick and cheap operation, but if not, the data segment
      /// will be copied. Note that the order of the pixels in the
      /// resulting image depend on the strides, and do not necessarily
      /// follow the same order as linear indices.
      ///
      /// \see PermuteDimensions, ExpandDimensionality.
      DIP_EXPORT Image& Flatten();

      /// \brief Remove singleton dimensions (dimensions with size==1).
      ///
      /// The image must be forged. The data will never be copied (i.e. this is a quick and cheap operation).
      ///
      /// \see AddSingleton, ExpandDimensionality, PermuteDimensions, UnexpandSingletonDimensions.
      DIP_EXPORT Image& Squeeze();

      /// \brief Remove singleton dimension `dim` (has size==1).
      ///
      /// The image must be forged. The data will never be copied (i.e. this is a quick and cheap operation).
      ///
      /// \see Squeeze, AddSingleton, ExpandDimensionality, PermuteDimensions, UnexpandSingletonDimensions.
      DIP_EXPORT Image& Squeeze( dip::uint dim );

      /// \brief Add a singleton dimension (with size==1) to the image.
      ///
      /// Dimensions `dim` to last are shifted up, dimension `dim` will have a size of 1.
      ///
      /// The image must be forged. The data will never be copied (i.e. this is a quick and cheap operation).
      ///
      /// Example: to an image with sizes `{ 4, 5, 6 }` we add a
      /// singleton dimension `dim == 1`. The image will now have
      /// sizes `{ 4, 1, 5, 6 }`.
      ///
      /// \see Squeeze, ExpandDimensionality, PermuteDimensions.
      DIP_EXPORT Image& AddSingleton( dip::uint dim );

      /// \brief Append singleton dimensions to increase the image dimensionality.
      ///
      /// The image will have `n` dimensions. However, if the image already
      /// has `n` or more dimensions, nothing happens.
      ///
      /// The image must be forged. The data will never be copied (i.e. this is a quick and cheap operation).
      ///
      /// \see AddSingleton, ExpandSingletonDimension, Squeeze, PermuteDimensions, Flatten.
      DIP_EXPORT Image& ExpandDimensionality( dip::uint dim );

      /// \brief Expand singleton dimension `dim` to `sz` pixels, setting the corresponding stride to 0.
      ///
      /// If `dim` is not a singleton dimension (size==1), an exception is thrown.
      ///
      /// The image must be forged. The data will never be copied (i.e. this is a quick and cheap operation).
      ///
      /// \see ExpandSingletonDimensions, ExpandSingletonTensor, IsSingletonExpanded, UnexpandSingletonDimensions, AddSingleton, ExpandDimensionality.
      DIP_EXPORT Image& ExpandSingletonDimension( dip::uint dim, dip::uint sz );

      /// \brief Performs singleton expansion.
      ///
      /// The image is modified so that it has `size`
      /// as dimensions. It must be forged and singleton-expandable to `size`,
      /// otherwise an exception is thrown. See `dip::Image::ExpandSingletonDimension`.
      /// `size` is the array as returned by `dip::Framework::SingletonExpandedSize`.
      ///
      /// \see ExpandSingletonDimension, ExpandSingletonTensor, IsSingletonExpanded, UnexpandSingletonDimensions.
      DIP_EXPORT Image& ExpandSingletonDimensions( UnsignedArray const& newSizes );

      /// \brief Unexpands singleton-expanded dimensions.
      ///
      /// The image is modified so that each singleton-expanded dimension has a size of 1.
      /// That is, the resulting image will no longer be `dip::Image::IsSingletonExpanded`.
      ///
      /// \see ExpandSingletonDimension, ExpandSingletonTensor, IsSingletonExpanded, Squeeze.
      DIP_EXPORT Image& UnexpandSingletonDimensions();

      /// \brief Tests if the image can be singleton-expanded to `size`.
      ///
      /// \see ExpandSingletonDimensions, ExpandSingletonTensor, IsSingletonExpanded.
      DIP_EXPORT bool IsSingletonExpansionPossible( UnsignedArray const& newSizes ) const;

      /// \brief Expand singleton tensor dimension `sz` samples, setting the tensor
      /// stride to 0.
      ///
      /// If there is more than one tensor element, an exception is thrown.
      ///
      /// The image must be forged. The data will never be copied (i.e. this is a quick and cheap operation).
      ///
      /// \see ExpandSingletonDimension, IsSingletonExpanded.
      DIP_EXPORT Image& ExpandSingletonTensor( dip::uint sz );

      /// \brief Mirror the image about selected axes.
      ///
      /// The image must be forged. The data will never be copied (i.e. this is a quick and cheap operation).
      ///
      /// `process` indicates which axes to mirror. If `process` is an empty array, all
      /// axes will be mirrored.
      DIP_EXPORT Image& Mirror( BooleanArray process = {} );

      /// \brief Rotates the image by `n` times 90 degrees, in the plane defined by dimensions
      /// `dimension1` and `dimension2`.
      ///
      /// The image must be forged. The data will never be copied (i.e. this is a quick and cheap operation).
      ///
      /// The rotation occurs in the direction of positive angles, as defined in the image coordinate system.
      /// That is, if `dimension1` is 0 (x-axis) and `dimension2` is 1 (y-axis), and considering the y-axis is
      /// positive in the up direction, then the rotation happens in counter-clockwise direction. A negative
      /// value for `n` inverts the direction of rotation.
      DIP_EXPORT Image& Rotation90( dip::sint n, dip::uint dimension1, dip::uint dimension2 );

      /// \brief Rotates the image by `n` times 90 degrees, in the plane perpendicular to dimension `axis`.
      ///
      /// The image must be forged and have three dimensions. The data will never be copied
      /// (i.e. this is a quick and cheap operation).
      Image& Rotation90( dip::sint n, dip::uint axis ) {
         DIP_THROW_IF( Dimensionality() != 3, E::DIMENSIONALITY_NOT_SUPPORTED );
         dip::uint dim1, dim2;
         switch( axis ) {
            case 0: // x-axis
               dim1 = 1;
               dim2 = 2;
               break;
            case 1: // y-axis
               dim1 = 2;
               dim2 = 0;
               break;
            case 2: // z-axis
               dim1 = 0;
               dim2 = 1;
               break;
            default:
               DIP_THROW( E::ILLEGAL_DIMENSION );
         }
         return Rotation90( n, dim1, dim2 );
      }

      /// \brief Rotates the image by `n` times 90 degrees, in the x-y plane.
      ///
      /// The image must be forged. The data will never be copied (i.e. this is a quick and cheap operation).
      Image& Rotation90( dip::sint n ) {
         return Rotation90( n, 0, 1 );
      }

      /// \brief Undo the effects of `Mirror`, `Rotation90` and `PermuteDimensions`.
      ///
      /// Modifies the image such that all strides are positive and sorted smaller to larger. The first
      /// dimension will have the smallest stride. Visiting pixels in linear indexing order (as is done
      /// through `dip::ImageIterator`) will be most efficient after calling this function.
      ///
      /// Note that strides are not necessarily normal after this call, if the image is a view over a
      /// larger image, if singleton dimensions were created or expanded, etc. Use `ForceNormalStrides`
      /// to ensure that strides are normal.
      DIP_EXPORT Image& StandardizeStrides();

      /// \brief Change the tensor shape, without changing the number of tensor elements.
      Image& ReshapeTensor( dip::uint rows, dip::uint cols ) {
         DIP_THROW_IF( tensor_.Elements() != rows * cols, "Cannot reshape tensor to requested sizes" );
         tensor_.ChangeShape( rows );
         return *this;
      }

      /// \brief Change the tensor shape, without changing the number of tensor elements.
      Image& ReshapeTensor( dip::Tensor const& other ) {
         tensor_.ChangeShape( other );
         return *this;
      }

      /// \brief Change the tensor to a vector, without changing the number of tensor elements.
      Image& ReshapeTensorAsVector() {
         tensor_.ChangeShape();
         return *this;
      }

      /// \brief Change the tensor to a diagonal matrix, without changing the number of tensor elements.
      Image& ReshapeTensorAsDiagonal() {
         dip::Tensor other{ dip::Tensor::Shape::DIAGONAL_MATRIX, tensor_.Elements(), tensor_.Elements() };
         tensor_.ChangeShape( other );
         return *this;
      }

      /// \brief Transpose the tensor.
      Image& Transpose() {
         tensor_.Transpose();
         return *this;
      }

      /// \brief Convert tensor dimensions to spatial dimension.
      ///
      /// Works even for scalar images, creating a singleton dimension. `dim`
      /// defines the new dimension, subsequent dimensions will be shifted over.
      /// `dim` should not be larger than the number of dimensions. `dim`
      /// defaults to the image dimensionality, meaning that the new dimension will
      /// be the last one. The image must be forged.
      DIP_EXPORT Image& TensorToSpatial( dip::uint dim );
      inline Image& TensorToSpatial() {
         return TensorToSpatial( Dimensionality() );
      }

      /// \brief Convert spatial dimension to tensor dimensions. The image must be scalar.
      ///
      /// If `rows` or `cols` is zero, its size is computed from the size of the
      /// image along dimension `dim`. If both are zero, a default column tensor
      /// is created. `dim` defaults to the last spatial dimension. The image must
      /// be forged.
      DIP_EXPORT Image& SpatialToTensor( dip::uint dim, dip::uint rows, dip::uint cols );
      inline Image& SpatialToTensor( dip::uint rows, dip::uint cols ) {
         return SpatialToTensor( Dimensionality() - 1, rows, cols );
      }
      inline Image& SpatialToTensor( dip::uint dim ) {
         return SpatialToTensor( dim, 0, 0 );
      }
      inline Image& SpatialToTensor() {
         return SpatialToTensor( Dimensionality() - 1, 0, 0 );
      }

      /// \brief Split the two values in a complex sample into separate samples,
      /// creating a new spatial dimension of size 2.
      ///
      /// `dim` defines the new
      /// dimension, subsequent dimensions will be shifted over. `dim` should
      /// not be larger than the number of dimensions. `dim` defaults to the
      /// image dimensionality, meaning that the new dimension will be the last one.
      /// The image must be forged.
      DIP_EXPORT Image& SplitComplex( dip::uint dim );
      inline Image& SplitComplex() {
         return SplitComplex( Dimensionality() );
      }

      /// \brief Merge the two samples along dimension `dim` into a single complex-valued sample.
      ///
      /// Dimension `dim` must have size 2 and a stride of 1. `dim` defaults to the last
      /// spatial dimension. The image must be forged.
      DIP_EXPORT Image& MergeComplex( dip::uint dim );
      inline Image& MergeComplex() {
         return MergeComplex( Dimensionality() - 1 );
      }

      /// \brief Split the two values in a complex sample into separate samples of
      /// a tensor. The image must be scalar and forged.
      DIP_EXPORT Image& SplitComplexToTensor();

      /// \brief Merge the two samples in the tensor into a single complex-valued sample.
      ///
      /// The image must have two tensor elements, a tensor stride of 1, and be forged.
      DIP_EXPORT Image& MergeTensorToComplex();

      /// \}

      //
      // Creating views of the data -- indexing without data copy
      // Defined in src/library/image_indexing.cpp
      //

      /// \name Indexing without data copy
      /// These functions create a different view of the data contained in the object. The output
      /// is a new `%Image` object. No data is copied, and the output typically contains a subset
      /// of the samples from the input.
      /// \{

      /// \brief Extract a tensor element, `indices` must have one or two elements. The image must be forged.
      DIP_EXPORT Image operator[]( UnsignedArray const& indices ) const;

      /// \brief Extract a tensor element using linear indexing. The image must be forged.
      DIP_EXPORT Image operator[]( dip::uint index ) const;

      /// \brief Extract tensor elements using linear indexing. The image must be forged.
      DIP_EXPORT Image operator[]( Range range ) const;

      /// \brief Extracts the tensor elements along the diagonal. The image must be forged.
      DIP_EXPORT Image Diagonal() const;

      /// \brief Extracts the tensor elements along the given row. The image must be forged and the tensor
      /// representation must be full (i.e. no symmetric or triangular matrices). Use `dip::Image::ExpandTensor`
      /// to obtain a full representation.
      DIP_EXPORT Image TensorRow( dip::uint index ) const;

      /// \brief Extracts the tensor elements along the given column. The image must be forged and the tensor
      /// representation must be full (i.e. no symmetric or triangular matrices). Use `dip::Image::ExpandTensor`
      /// to obtain a full representation.
      DIP_EXPORT Image TensorColumn( dip::uint index ) const;

      /// \brief Extracts the pixel at the given coordinates. The image must be forged.
      Pixel At( UnsignedArray const& coords ) const {
         DIP_THROW_IF( coords.size() != sizes_.size(), E::ARRAY_ILLEGAL_SIZE );
         return Pixel( Pointer( coords ), dataType_, tensor_, tensorStride_ );
      }

      /// \brief Same as above, but returns a type that implicitly casts to `T`.
      template< typename T >
      CastPixel< T > At( UnsignedArray const& coords ) const { return CastPixel< T >( At( coords )); }

      /// \brief Extracts the pixel at the given linear index (inefficient if image is not 1D!). The image must be forged.
      Pixel At( dip::uint index ) const {
         if( index == 0 ) { // shortcut to the first pixel
            return Pixel( Origin(), dataType_, tensor_, tensorStride_ );
         } else if( sizes_.size() < 2 ) {
            dip::uint n = sizes_.size() == 0 ? 1 : sizes_[ 0 ];
            DIP_THROW_IF( index >= n, E::INDEX_OUT_OF_RANGE );
            return Pixel( Pointer( static_cast< dip::sint >( index ) * strides_[ 0 ] ),
                          dataType_, tensor_, tensorStride_ );
         } else {
            return At( IndexToCoordinates( index ));
         }
      }

      /// \brief Same as above, but returns a type that implicitly casts to `T`.
      template< typename T >
      CastPixel< T > At( dip::uint index ) const { return CastPixel< T >( At( index )); }

      /// \brief Extracts the pixel at the given coordinates from a 2D image. The image must be forged.
      Pixel At( dip::uint x_index, dip::uint y_index ) const {
         DIP_THROW_IF( sizes_.size() != 2, E::ILLEGAL_DIMENSIONALITY );
         DIP_THROW_IF( x_index >= sizes_[ 0 ], E::INDEX_OUT_OF_RANGE );
         DIP_THROW_IF( y_index >= sizes_[ 1 ], E::INDEX_OUT_OF_RANGE );
         return Pixel( Pointer( static_cast< dip::sint >( x_index ) * strides_[ 0 ] +
                                static_cast< dip::sint >( y_index ) * strides_[ 1 ] ),
                       dataType_, tensor_, tensorStride_ );
      }

      /// \brief Same as above, but returns a type that implicitly casts to `T`.
      template< typename T >
      CastPixel< T > At( dip::uint x_index, dip::uint y_index ) const { return CastPixel< T >( At( x_index, y_index )); }

      /// \brief Extracts the pixel at the given coordinates from a 3D image. The image must be forged.
      Pixel At( dip::uint x_index, dip::uint y_index, dip::uint z_index ) const {
         DIP_THROW_IF( sizes_.size() != 3, E::ILLEGAL_DIMENSIONALITY );
         DIP_THROW_IF( x_index >= sizes_[ 0 ], E::INDEX_OUT_OF_RANGE );
         DIP_THROW_IF( y_index >= sizes_[ 1 ], E::INDEX_OUT_OF_RANGE );
         DIP_THROW_IF( z_index >= sizes_[ 2 ], E::INDEX_OUT_OF_RANGE );
         return Pixel( Pointer( static_cast< dip::sint >( x_index ) * strides_[ 0 ] +
                                static_cast< dip::sint >( y_index ) * strides_[ 1 ] +
                                static_cast< dip::sint >( z_index ) * strides_[ 2 ] ),
                       dataType_, tensor_, tensorStride_ );
      }

      /// \brief Same as above, but returns a type that implicitly casts to `T`.
      template< typename T >
      CastPixel< T > At( dip::uint x_index, dip::uint y_index, dip::uint z_index ) const {
         return CastPixel< T >( At( x_index, y_index, z_index ));
      }

      /// \brief Extracts a subset of pixels from a 1D image. The image must be forged.
      DIP_EXPORT Image At( Range x_range ) const;

      /// \brief Extracts a subset of pixels from a 2D image. The image must be forged.
      DIP_EXPORT Image At( Range x_range, Range y_range ) const;

      /// \brief Extracts a subset of pixels from a 3D image. The image must be forged.
      DIP_EXPORT Image At( Range x_range, Range y_range, Range z_range ) const;

      /// \brief Extracts a subset of pixels from an image. The image must be forged.
      DIP_EXPORT Image At( RangeArray ranges ) const;

      /// \brief Extracts a subset of pixels from an image.
      ///
      /// Crops the image to the given size. Which pixels are selected is controlled by the
      /// `cropLocation` parameter. The default is `dip::Option::CropLocation::CENTER`, which
      /// maintains the origin pixel (as defined in `dip::FourierTransform` and other other places)
      /// at the origin of the output image.
      ///
      /// `dip::Image::Pad` does the inverse operation.
      ///
      /// The image must be forged.
      DIP_EXPORT Image Crop( UnsignedArray const& sizes, Option::CropLocation cropLocation = Option::CropLocation::CENTER ) const;

      /// \brief Extracts a subset of pixels from an image.
      ///
      /// This is an overloaded version of the function above, meant for use from bindings in other languages. The
      /// string `cropLocation` is translated to one of the `dip::Option::CropLocation` values as follows:
      ///
      /// %CropLocation constant  | String
      /// ----------------------- | ----------
      /// CENTER                  | "center"
      /// MIRROR_CENTER           | "mirror center"
      /// TOP_LEFT                | "top left"
      /// BOTTOM_RIGHT            | "bottom right"
      inline Image Crop( UnsignedArray const& sizes, String const& cropLocation ) const {
         if( cropLocation == "center" ) {
            return Crop( sizes, Option::CropLocation::CENTER );
         } else if( cropLocation == "mirror center" ) {
            return Crop( sizes, Option::CropLocation::MIRROR_CENTER );
         } else if( cropLocation == "top left" ) {
            return Crop( sizes, Option::CropLocation::TOP_LEFT );
         } else if( cropLocation == "bottom right" ) {
            return Crop( sizes, Option::CropLocation::BOTTOM_RIGHT );
         } else {
            DIP_THROW( "Unknown crop location flag" );
         }
      };

      /// \brief Extracts the real component of a complex-typed image. The image must be forged.
      DIP_EXPORT Image Real() const;

      /// \brief Extracts the imaginary component of a complex-typed image. The image must be forged.
      DIP_EXPORT Image Imaginary() const;

      /// \brief Quick copy, returns a new image that points at the same data as `this`,
      /// and has mostly the same properties.
      ///
      /// The color space and pixel size information are not copied, and the protect flag is reset.
      /// This function is mostly meant for use in functions that need to modify some properties of
      /// the input images, without actually modifying the input images.
      Image QuickCopy() const {
         Image out;
         out.dataType_ = dataType_;
         out.sizes_ = sizes_;
         out.strides_ = strides_;
         out.tensor_ = tensor_;
         out.tensorStride_ = tensorStride_;
         out.dataBlock_ = dataBlock_;
         out.origin_ = origin_;
         out.externalData_ = externalData_;
         out.externalInterface_ = externalInterface_;
         return out;
      }

      /// \}

      //
      // Getting/setting pixel values
      // Defined in src/library/image_data.cpp
      //

      /// \name Getting and setting pixel values, copying
      /// \{

      /// \brief Creates a 1D image containing the pixels selected by `mask`.
      ///
      /// The values are copied, not referenced. The output image will be of the same data type and tensor shape
      /// as `this`, but have only one dimension. Pixels will be read from `mask` in the linear index order.
      ///
      /// `this` must be forged and be of equal size as `mask`. `mask` is a scalar binary image.
      DIP_EXPORT Image CopyAt( Image const& mask ) const;

      /// \brief Creates a 1D image containing the pixels selected by `indices`.
      ///
      /// The values are copied, not referenced. The output image will be of the same data type and tensor shape
      /// as `this`, but have only one dimension. It will have as many pixels as indices are in `indices`, and
      /// be sorted in the same order.
      ///
      /// `indices` contains linear indices into the image. Note that converting indices into offsets is not a
      /// trivial operation; prefer to use the version of this function that uses coordinates.
      ///
      /// `this` must be forged.
      DIP_EXPORT Image CopyAt( UnsignedArray const& indices ) const;

      /// \brief Creates a 1D image containing the pixels selected by `coordinates`.
      ///
      /// The values are copied, not referenced. The output image will be of the same data type and tensor shape
      /// as `this`, but have only one dimension. It will have as many pixels as coordinates are in `coordinates`,
      /// and be sorted in the same order.
      ///
      /// Each of the coordinates must have the same number of dimensions as `this`.
      ///
      /// `this` must be forged.
      DIP_EXPORT Image CopyAt( CoordinateArray const& coordinates ) const;

      /// \brief Copies the pixel values from `source` into `this`, to the pixels selected by `mask`.
      ///
      /// `source` must have the same number of tensor elements as `this`. The sample values will be
      /// cast to match the data type of `this`, where values are clipped to the target range and/or
      /// truncated, as applicable. Complex values are converted to non-complex values by taking the
      /// absolute value.
      ///
      /// Pixels selected by `mask` are taken in the linear index order.
      ///
      /// `source` is expected to have the same number of pixels as are selected by `mask`, but this is
      /// not tested for before the copy begins unless `throws` is set to `dip::Option::ThrowException::DO_THROW`.
      /// Note that this tests costs extra time, and often is not necessary.
      /// By default, the pixels are copied until either all `source` pixels have been copied or until all
      /// pixels selected by `mask` have been written to.
      ///
      /// `this` must be forged and be of equal size as `mask`. `mask` is a scalar binary image.
      DIP_EXPORT void CopyAt( Image const& source, Image const& mask, Option::ThrowException throws = Option::ThrowException::DONT_THROW );

      /// \brief Copies the pixel values from `source` into `this`, to the pixels selected by `indices`.
      ///
      /// `source` must have the same number of tensor elements as `this`. The sample values will be
      /// cast to match the data type of `this`, where values are clipped to the target range and/or
      /// truncated, as applicable. Complex values are converted to non-complex values by taking the
      /// absolute value.
      ///
      /// `source` must have the same number of pixels as indices are in `indices`, and are used in the
      /// same order. `indices` contains linear indices into the image. Note that converting indices
      /// into offsets is not a trivial operation; prefer to use the version of this function that uses coordinates.
      ///
      /// `this` must be forged.
      DIP_EXPORT void CopyAt( Image const& source, UnsignedArray const& indices );

      /// \brief Copies the pixel values from `source` into `this`, to the pixels selected by `coordinates`.
      ///
      /// `source` must have the same number of tensor elements as `this`. The sample values will be
      /// cast to match the data type of `this`, where values are clipped to the target range and/or
      /// truncated, as applicable. Complex values are converted to non-complex values by taking the
      /// absolute value.
      ///
      /// `source` must have the same number of pixels as coordinates are in `coordinates`, and are used
      /// in the same order. Each of the coordinates must have the same number of dimensions as `this`.
      ///
      /// `this` must be forged.
      DIP_EXPORT void CopyAt( Image const& source, CoordinateArray const& coordinates );

      /// \brief Extends the image by padding with zeros.
      ///
      /// Pads the image to the given size. Where the original image data is located in the output image
      /// is controlled by the `cropLocation` parameter. The default is `dip::Option::CropLocation::CENTER`,
      /// which maintains the origin pixel (as defined in `dip::FourierTransform` and other other places)
      /// at the origin of the output image.
      ///
      /// The object is not modified, a new image is created, with identical properties, but of the requested
      /// size.
      ///
      /// `dip::Image::Crop` does the inverse operation. See also `dip::ExtendImage`.
      ///
      /// The image must be forged.
      DIP_EXPORT Image Pad( UnsignedArray const& sizes, Option::CropLocation cropLocation = Option::CropLocation::CENTER ) const;

      /// \brief Extends the image by padding with zeros.
      ///
      /// This is an overloaded version of the function above, meant for use from bindings in other languages. The
      /// string `cropLocation` is translated to one of the `dip::Option::CropLocation` values as follows:
      ///
      /// %CropLocation constant  | String
      /// ----------------------- | ----------
      /// CENTER                  | "center"
      /// MIRROR_CENTER           | "mirror center"
      /// TOP_LEFT                | "top left"
      /// BOTTOM_RIGHT            | "bottom right"
      inline Image Pad( UnsignedArray const& sizes, String const& cropLocation ) const {
         if( cropLocation == "center" ) {
            return Pad( sizes, Option::CropLocation::CENTER );
         } else if( cropLocation == "mirror center" ) {
            return Pad( sizes, Option::CropLocation::MIRROR_CENTER );
         } else if( cropLocation == "top left" ) {
            return Pad( sizes, Option::CropLocation::TOP_LEFT );
         } else if( cropLocation == "bottom right" ) {
            return Pad( sizes, Option::CropLocation::BOTTOM_RIGHT );
         } else {
            DIP_THROW( "Unknown crop location flag" );
         }
      };

      /// \brief Deep copy, `this` will become a copy of `src` with its own data.
      ///
      /// If `this` is forged, then `src` is expected to have the same sizes
      /// and number of tensor elements, and the data is copied over from `src`
      /// to `this`. The copy will apply data type conversion, where values are
      /// clipped to the target range and/or truncated, as applicable. Complex
      /// values are converted to non-complex values by taking the absolute
      /// value.
      ///
      /// If `this` is not forged, then all the properties of `src` will be
      /// copied to `this`, `this` will be forged, and the data from `src` will
      /// be copied over.
      ///
      /// `src` must be forged.
      DIP_EXPORT void Copy( Image const& src );

      /// \brief Converts the image to another data type.
      ///
      /// The data type conversion clips values to the target range and/or truncates them, as applicable.
      /// Complex values are converted to non-complex values by taking the absolute value.
      ///
      /// The data segment is replaced by a new one, unless the old and new data
      /// types have the same size and it is not shared with other images.
      /// If the data segment is replaced, strides are set to normal.
      DIP_EXPORT void Convert( dip::DataType dt );

      /// \brief Expands the image's tensor, such that the tensor representation is a column-major matrix.
      ///
      /// If the image has a non-full tensor representation (diagonal, symmetric, triangular), or
      /// a row-major ordering, then the data segment is replaced by a new one. Otherwise, nothing is done.
      ///
      /// After calling this method, the object always has `dip::Tensor::HasNormalOrder` equal `true`.
      /// This method simplifies manipulating tensors by normalizing their storage.
      DIP_EXPORT void ExpandTensor();

      /// \brief Copies pixel data over to a new data segment if the strides are not normal.
      ///
      /// Will throw an exception if reallocating the data segment does not yield normal strides.
      /// This can happen only if there is an external interface.
      ///
      /// The image must be forged.
      ///
      /// \see HasNormalStrides, ForceContiguousData.
      void ForceNormalStrides() {
         if( !HasNormalStrides() ) {
            Image tmp;
            tmp.externalInterface_ = externalInterface_;
            tmp.ReForge( *this ); // This way we don't copy the strides. out.Copy( *this ) would do so if out is not yet forged!
            DIP_THROW_IF( !tmp.HasNormalStrides(), "Cannot force strides to normal" );
            tmp.Copy( *this );
            swap( tmp );
         }
      }

      /// \brief Copies pixel data over to a new data segment if the data is not contiguous.
      ///
      /// The image must be forged.
      ///
      /// \see HasContiguousData, ForceNormalStrides.
      void ForceContiguousData() {
         if( !HasContiguousData() ) {
            Image tmp;
            tmp.externalInterface_ = externalInterface_;
            tmp.ReForge( *this ); // This way we don't copy the strides. out.Copy( *this ) would do so if out is not yet forged!
            DIP_ASSERT( tmp.HasContiguousData() );
            tmp.Copy( *this );
            swap( tmp );
         }
      }

      /// \brief Sets all pixels in the image to the value `pixel`.
      ///
      /// `pixel` must have the same number of tensor elements as the image, or be a scalar.
      /// Its values will be clipped to the target range and/or truncated, as applicable.
      ///
      /// The image must be forged.
      DIP_EXPORT void Fill( Pixel const& pixel );

      /// \brief Sets all samples in the image to the value `sample`.
      ///
      /// The value will be clipped to the target range and/or truncated, as applicable.
      ///
      /// The image must be forged.
      DIP_EXPORT void Fill( Sample const& sample );

      /// \brief Fills the pixels selected by `mask` with the values of `pixel`.
      ///
      /// `pixel` must have the same number of tensor elements as `this`. The sample values will be
      /// cast to match the data type of `this`, where values are clipped to the target range and/or
      /// truncated, as applicable. Complex values are converted to non-complex values by taking the
      /// absolute value.
      ///
      /// `this` must be forged and be of equal size as `mask`. `mask` is a scalar binary image.
      DIP_EXPORT void FillAt( Pixel const& pixel, Image const& mask );

      /// \brief Fills the pixels selected by `mask` with the value of `sample`.
      ///
      /// The sample value will be cast to match the data type of `this`, and will be clipped to the
      /// target range and/or truncated, as applicable. A complex value is converted to non-complex
      /// values by taking the absolute value.
      ///
      /// `this` must be forged and be of equal size as `mask`. `mask` is a scalar binary image.
      DIP_EXPORT void FillAt( Sample const& sample, Image const& mask );

      /// \brief Fills the pixels selected by `indices` with the values of `pixel`.
      ///
      /// `pixel` must have the same number of tensor elements as `this`. The sample values will be
      /// cast to match the data type of `this`, where values are clipped to the target range and/or
      /// truncated, as applicable. Complex values are converted to non-complex values by taking the
      /// absolute value.
      ///
      /// `indices` contains linear indices into the image. Note that converting indices into offsets
      /// is not a trivial operation; prefer to use the version of this function that uses coordinates.
      ///
      /// `this` must be forged.
      DIP_EXPORT void FillAt( Pixel const& pixel, UnsignedArray const& indices );

      /// \brief Fills the pixels selected by `indices` with the value of `sample`.
      ///
      /// The sample value will be cast to match the data type of `this`, and will be clipped to the
      /// target range and/or truncated, as applicable. A complex value is converted to non-complex
      /// values by taking the absolute value.
      ///
      /// `indices` contains linear indices into the image. Note that converting indices into offsets
      /// is not a trivial operation; prefer to use the version of this function that uses coordinates.
      ///
      /// `this` must be forged.
      DIP_EXPORT void FillAt( Sample const& sample, UnsignedArray const& indices );

      /// \brief Fills the pixels selected by `coordinates` with the values of `pixel`.
      ///
      /// `pixel` must have the same number of tensor elements as `this`. The sample values will be
      /// cast to match the data type of `this`, where values are clipped to the target range and/or
      /// truncated, as applicable. Complex values are converted to non-complex values by taking the
      /// absolute value.
      ///
      /// Each of the coordinates must have the same number of dimensions as `this`.
      /// `this` must be forged.
      DIP_EXPORT void FillAt( Pixel const& pixel, CoordinateArray const& coordinates );

      /// \brief Fills the pixels selected by `coordinates` with the value of `sample`.
      ///
      /// The sample value will be cast to match the data type of `this`, and will be clipped to the
      /// target range and/or truncated, as applicable. A complex value is converted to non-complex
      /// values by taking the absolute value.
      ///
      /// Each of the coordinates must have the same number of dimensions as `this`.
      /// `this` must be forged.
      DIP_EXPORT void FillAt( Sample const& sample, CoordinateArray const& coordinates );

      /// \brief Sets all pixels in the image to the value `pixel`.
      ///
      /// `pixel` must have the same number of tensor elements as the image, or be a scalar.
      /// Its values will be clipped to the target range and/or truncated, as applicable.
      ///
      /// The image must be forged.
      Image& operator=( Pixel const& pixel ) {
         Fill( pixel );
         return *this;
      }

      /// \brief Sets all samples in the image to the value `sample`.
      ///
      /// The value will be clipped to the target range and/or truncated, as applicable.
      ///
      /// The image must be forged.
      Image& operator=( Sample const& sample ) {
         Fill( sample );
         return *this;
      }

      // This one is to disambiguate calling with a single initializer list. We don't mean UnsignedArray, we mean Pixel.
      template< typename T, typename std::enable_if< IsSampleType< T >::value, int >::type = 0 >
      Image& operator=( std::initializer_list< T > values ) {
         Fill( Pixel( values ));
         return *this;
      }

      /// Returns the value of the first sample in the first pixel in the image as the given numeric type.
      template< typename T, typename std::enable_if< IsNumericType< T >::value, int >::type = 0 >
      T As() const { return detail::CastSample< T >( dataType_, origin_ ); };

      /// \}

   private:

      //
      // Implementation
      //

      dip::DataType dataType_ = DT_SFLOAT;
      UnsignedArray sizes_;               // sizes_.size() == ndims (if forged)
      IntegerArray strides_;              // strides_.size() == ndims (if forged)
      dip::Tensor tensor_;
      dip::sint tensorStride_ = 0;
      bool protect_ = false;              // When set, don't strip image
      String colorSpace_;
      dip::PixelSize pixelSize_;
      DataSegment dataBlock_;             // Holds the pixel data. Data block will be freed when last image that uses it is destroyed.
      void* origin_ = nullptr;            // Points to the origin ( pixel (0,0) ), not necessarily the first pixel of the data block.
      bool externalData_ = false;         // Is true if origin_ points to a data segment that was not allocated by DIPlib.
      dip::ExternalInterface* externalInterface_ = nullptr; // A function that will be called instead of the default forge function.

      //
      // Some private functions
      //

      // Are the strides such that no two samples are in the same memory cell?
      DIP_EXPORT bool HasValidStrides() const;

      // Fill in all strides.
      DIP_EXPORT void SetNormalStrides();

      DIP_EXPORT void GetDataBlockSizeAndStart( dip::uint& size, dip::sint& start ) const;

      DIP_EXPORT void GetDataBlockSizeAndStartWithTensor( dip::uint& size, dip::sint& start ) const;
      // `size` is the distance between top left and bottom right corners. `start` is the distance between
      // top left corner and origin (will be <0 if any strides[ii] < 0). All measured in samples.

      // Throws is `sizes_` is not good.
      DIP_NO_EXPORT static void TestSizes( UnsignedArray sizes ) {
         for( auto s : sizes ) {
            DIP_THROW_IF(( s == 0 ) || ( s > maxint ), "Sizes must be non-zero and no larger than " + std::to_string( maxint ));
         }
      }

}; // class Image


//
// Implementation of Pixels and Samples
//

inline Image::Sample::Sample( Image::Pixel const& pixel ) : origin_( pixel.Origin() ), dataType_( pixel.DataType() ) {}
inline void swap( Image::Sample& v1, Image::Sample& v2 ) { v1.swap( v2 ); }
inline void swap( Image::Pixel& v1, Image::Pixel& v2 ) { v1.swap( v2 ); }
inline void swap( Image::Pixel::Iterator& v1, Image::Pixel::Iterator& v2 ) { v1.swap( v2 ); }

/// \brief Arithmetic operator, element-wise.
DIP_EXPORT Image::Pixel operator+( Image::Pixel const& lhs, Image::Pixel const& rhs );
template< typename T, typename std::enable_if< IsNumericType< T >::value, int >::type = 0 >
Image::Pixel operator+( Image::Pixel const& lhs, T const& rhs ) { return operator+( lhs, Image::Pixel{ rhs } ); }
/// \brief Arithmetic operator, element-wise.
DIP_EXPORT Image::Pixel operator-( Image::Pixel const& lhs, Image::Pixel const& rhs );
template< typename T, typename std::enable_if< IsNumericType< T >::value, int >::type = 0 >
Image::Pixel operator-( Image::Pixel const& lhs, T const& rhs ) { return operator-( lhs, Image::Pixel{ rhs } ); }
/// \brief Arithmetic operator, tensor multiplication.
DIP_EXPORT Image::Pixel operator*( Image::Pixel const& lhs, Image::Pixel const& rhs );
template< typename T, typename std::enable_if< IsNumericType< T >::value, int >::type = 0 >
Image::Pixel operator*( Image::Pixel const& lhs, T const& rhs ) { return operator*( lhs, Image::Pixel{ rhs } ); }
/// \brief Arithmetic operator, element-wise.
DIP_EXPORT Image::Pixel operator/( Image::Pixel const& lhs, Image::Pixel const& rhs );
template< typename T, typename std::enable_if< IsNumericType< T >::value, int >::type = 0 >
Image::Pixel operator/( Image::Pixel const& lhs, T const& rhs ) { return operator/( lhs, Image::Pixel{ rhs } ); }
/// \brief Arithmetic operator, element-wise.
DIP_EXPORT Image::Pixel operator%( Image::Pixel const& lhs, Image::Pixel const& rhs );
template< typename T, typename std::enable_if< IsNumericType< T >::value, int >::type = 0 >
Image::Pixel operator%( Image::Pixel const& lhs, T const& rhs ) { return operator%( lhs, Image::Pixel{ rhs } ); }
/// \brief Bit-wise operator, element-wise.
DIP_EXPORT Image::Pixel operator&( Image::Pixel const& lhs, Image::Pixel const& rhs );
template< typename T, typename std::enable_if< IsNumericType< T >::value, int >::type = 0 >
Image::Pixel operator&( Image::Pixel const& lhs, T const& rhs ) { return operator&( lhs, Image::Pixel{ rhs } ); }
/// \brief Bit-wise operator, element-wise.
DIP_EXPORT Image::Pixel operator|( Image::Pixel const& lhs, Image::Pixel const& rhs );
template< typename T, typename std::enable_if< IsNumericType< T >::value, int >::type = 0 >
Image::Pixel operator|( Image::Pixel const& lhs, T const& rhs ) { return operator|( lhs, Image::Pixel{ rhs } ); }
/// \brief Bit-wise operator, element-wise.
DIP_EXPORT Image::Pixel operator^( Image::Pixel const& lhs, Image::Pixel const& rhs );
template< typename T, typename std::enable_if< IsNumericType< T >::value, int >::type = 0 >
Image::Pixel operator^( Image::Pixel const& lhs, T const& rhs ) { return operator^( lhs, Image::Pixel{ rhs } ); }
/// \brief Unary operator, element-wise.
DIP_EXPORT Image::Pixel operator-( Image::Pixel const& in );
/// \brief Bit-wise unary operator operator.
DIP_EXPORT Image::Pixel operator~( Image::Pixel const& in );
/// \brief Boolean unary operator, element-wise.
DIP_EXPORT Image::Pixel operator!( Image::Pixel const& in );
/// \brief Comparison operator, can only be true if the two pixels have compatible number of tensor elements.
DIP_EXPORT bool operator==( Image::Pixel const& lhs, Image::Pixel const& rhs );
template< typename T, typename std::enable_if< IsNumericType< T >::value, int >::type = 0 >
bool operator==( Image::Pixel const& lhs, T const& rhs ) { return operator==( lhs, Image::Pixel{ rhs } ); }
/// \brief Comparison operator, equivalent to `!(lhs==rhs)`.
template< typename T, typename std::enable_if< IsNumericType< T >::value, int >::type = 0 >
bool operator!=( Image::Pixel const& lhs, T const& rhs ) { return !operator==( lhs, rhs ); }
/// \brief Comparison operator, can only be true if the two pixels have compatible number of tensor elements.
DIP_EXPORT bool operator< ( Image::Pixel const& lhs, Image::Pixel const& rhs );
template< typename T, typename std::enable_if< IsNumericType< T >::value, int >::type = 0 >
bool operator< ( Image::Pixel const& lhs, T const& rhs ) { return operator< ( lhs, Image::Pixel{ rhs } ); }
/// \brief Comparison operator, can only be true if the two pixels have compatible number of tensor elements.
DIP_EXPORT bool operator> ( Image::Pixel const& lhs, Image::Pixel const& rhs );
template< typename T, typename std::enable_if< IsNumericType< T >::value, int >::type = 0 >
bool operator> ( Image::Pixel const& lhs, T const& rhs ) { return operator> ( lhs, Image::Pixel{ rhs } ); }
/// \brief Comparison operator, can only be true if the two pixels have compatible number of tensor elements.
DIP_EXPORT bool operator<=( Image::Pixel const& lhs, Image::Pixel const& rhs );
template< typename T, typename std::enable_if< IsNumericType< T >::value, int >::type = 0 >
bool operator<=( Image::Pixel const& lhs, T const& rhs ) { return operator<=( lhs, Image::Pixel{ rhs } ); }
/// \brief Comparison operator, can only be true if the two pixels have compatible number of tensor elements.
DIP_EXPORT bool operator>=( Image::Pixel const& lhs, Image::Pixel const& rhs );
template< typename T, typename std::enable_if< IsNumericType< T >::value, int >::type = 0 >
bool operator>=( Image::Pixel const& lhs, T const& rhs ) { return operator>=( lhs, Image::Pixel{ rhs } ); }

template< typename T >
Image::Pixel& Image::Pixel::operator+=( T const& rhs ) { return *this = operator+( *this, rhs ); }
template< typename T >
Image::Pixel& Image::Pixel::operator-=( T const& rhs ) { return *this = operator-( *this, rhs ); }
template< typename T >
Image::Pixel& Image::Pixel::operator*=( T const& rhs ) { return *this = operator*( *this, rhs ); }
template< typename T >
Image::Pixel& Image::Pixel::operator/=( T const& rhs ) { return *this = operator/( *this, rhs ); }
template< typename T >
Image::Pixel& Image::Pixel::operator%=( T const& rhs ) { return *this = operator%( *this, rhs ); }
template< typename T >
Image::Pixel& Image::Pixel::operator&=( T const& rhs ) { return *this = operator&( *this, rhs ); }
template< typename T >
Image::Pixel& Image::Pixel::operator|=( T const& rhs ) { return *this = operator|( *this, rhs ); }
template< typename T >
Image::Pixel& Image::Pixel::operator^=( T const& rhs ) { return *this = operator^( *this, rhs ); }

template< typename T >
Image::Sample& Image::Sample::operator+=( T const& rhs ) {
   return *this = dataType_.IsComplex()
                  ? detail::CastSample< dcomplex >( dataType_, origin_ ) + static_cast< dcomplex >( rhs )
                  : detail::CastSample< dfloat >( dataType_, origin_ ) + rhs;
}
template< typename T >
Image::Sample& Image::Sample::operator-=( T const& rhs ) {
   return *this = dataType_.IsComplex()
                  ? detail::CastSample< dcomplex >( dataType_, origin_ ) - static_cast< dcomplex >( rhs )
                  : detail::CastSample< dfloat >( dataType_, origin_ ) - rhs;
}
template< typename T >
Image::Sample& Image::Sample::operator*=( T const& rhs ) {
   return *this = dataType_.IsComplex()
                  ? detail::CastSample< dcomplex >( dataType_, origin_ ) * static_cast< dcomplex >( rhs )
                  : detail::CastSample< dfloat >( dataType_, origin_ ) * rhs;
}
template< typename T >
Image::Sample& Image::Sample::operator/=( T const& rhs ) {
   return *this = dataType_.IsComplex()
                  ? detail::CastSample< dcomplex >( dataType_, origin_ ) / static_cast< dcomplex >( rhs )
                  : detail::CastSample< dfloat >( dataType_, origin_ ) / rhs;
}
// Operators below are too difficult to implement with only dcomplex and dfloat types.
template< typename T >
Image::Sample& Image::Sample::operator%=( T const& rhs ) { return *this = operator%( Image::Pixel( *this ), rhs )[ 0 ]; }
template< typename T >
Image::Sample& Image::Sample::operator&=( T const& rhs ) { return *this = operator&( Image::Pixel( *this ), rhs )[ 0 ]; }
template< typename T >
Image::Sample& Image::Sample::operator|=( T const& rhs ) { return *this = operator|( Image::Pixel( *this ), rhs )[ 0 ]; }
template< typename T >
Image::Sample& Image::Sample::operator^=( T const& rhs ) { return *this = operator^( Image::Pixel( *this ), rhs )[ 0 ]; }


//
// Overloaded stream output operators
//

/// \brief You can output a `dip::Image::Sample` to `std::cout` or any other stream.
/// It is printed like any numeric value of the same type.
inline std::ostream& operator<<(
      std::ostream& os,
      Image::Sample const& sample
) {
   switch( sample.DataType() ) {
      case DT_BIN:
         os << sample.As< bin >(); break;
      case DT_UINT8:
      case DT_UINT16:
      case DT_UINT32:
         os << sample.As< uint32 >(); break;
      default: // signed integers
         os << sample.As< sint32 >(); break;
      case DT_SFLOAT:
      case DT_DFLOAT:
         os << sample.As< dfloat >(); break;
      case DT_SCOMPLEX:
      case DT_DCOMPLEX:
         os << sample.As< dcomplex >(); break;
   }
   return os;
}

/// \brief You can output a `dip::Image::Pixel` to `std::cout` or any other stream.
/// It is printed as a sequence of values, prepended with "Pixel with values:".
inline std::ostream& operator<<(
      std::ostream& os,
      Image::Pixel const& pixel
) {
   dip::uint N = pixel.TensorElements();
   if( N == 1 ) {
      os << "Pixel with value: " << pixel[ 0 ];
   } else {
      os << "Pixel with values: " << pixel[ 0 ];
      for( dip::uint ii = 1; ii < N; ++ii ) {
         os << ", " << pixel[ ii ];
      }
   }
   return os;
}

/// \brief You can output a `dip::Image` to `std::cout` or any other stream. Some
/// information about the image is printed.
DIP_EXPORT std::ostream& operator<<( std::ostream& os, Image const& img );

//
// Utility functions
//

inline void swap( Image& v1, Image& v2 ) {
   v1.swap( v2 );
}

/// \brief Calls `img1.Aliases( img2 )`. See `dip::Image::Aliases`.
inline bool Alias( Image const& img1, Image const& img2 ) {
   return img1.Aliases( img2 );
}

/// \brief Makes a new image object pointing to same pixel data as `src`, but
/// with different origin, strides and size.
///
/// This function does the same as `dip::Image::At`, but allows for more flexible
/// defaults: If `origin`, `sizes` or `spacing` have only one value, that value is
/// repeated for each dimension. For empty arrays, `origin` defaults to all zeros,
/// `sizes` to `src.Sizes() - origin`, and `spacing` to all ones. These defaults
/// make it easy to crop pixels from one side of the image, to subsample the image,
/// etc. For example, the following code subsamples by a factor 2 in each dimension:
///
/// ```cpp
///     DefineROI( src, dest, {}, {}, { 2 } );
/// ```
DIP_EXPORT void DefineROI(
      Image const& src,
      Image& dest,
      UnsignedArray origin = {},
      UnsignedArray sizes = {},
      UnsignedArray spacing = {}
);

inline Image DefineROI(
      Image const& src,
      UnsignedArray const& origin,
      UnsignedArray const& sizes,
      UnsignedArray const& spacing = {}
) {
   Image dest;
   DefineROI( src, dest, origin, sizes, spacing );
   return dest;
}

/// \brief Copies samples over from `src` to `dest`, identical to the `dip::Image::Copy` method.
inline void Copy( Image const& src, Image& dest ) {
   dest.Copy( src );
}

inline Image Copy( Image const& src ) {
   Image dest;
   dest.Copy( src );
   return dest;
}

/// \brief Copies samples over from `src` to `dest`, expanding the tensor so it's a standard, column-major matrix.
///
/// If the tensor representation in `src` is one of those that do not save symmetric or zero values, to save space,
/// a new data segment will be allocated for `dest`, where the tensor representation is a column-major matrix
/// (`dest` will have `dip::Tensor::HasNormalOrder` be true). Otherwise, `dest` will share the data segment with `src`.
/// This function simplifies manipulating tensors by normalizing their storage.
///
/// \see dip::Copy, dip::Convert, dip::Image::ExpandTensor
inline void ExpandTensor( Image const& src, Image& dest ) {
   if( &src == &dest ) {
      dest.ExpandTensor();
   } else {
      dest = src;
      dest.ExpandTensor();
   }
}

inline Image ExpandTensor( Image const& src ) {
   Image dest = src;
   dest.ExpandTensor();
   return dest;
}

/// \brief Copies samples over from `src` to `dest`, with data type conversion.
///
/// If `dest` is forged,
/// has the same size as number of tensor elements as `src`, and has data type `dt`, then
/// its data segment is reused. If `src` and `dest` are the same object, its `dip::Image::Convert`
/// method is called instead.
///
/// The data type conversion clips values to the target range and/or truncates them, as applicable.
/// Complex values are converted to non-complex values by taking the absolute value.
inline void Convert(
      Image const& src,
      Image& dest,
      dip::DataType dt
) {
   if( &src == &dest ) {
      dest.Convert( dt );
   } else {
      dest.ReForge( src, dt );
      dest.Copy( src );
   }
}

inline Image Convert( Image const& src, dip::DataType dt ) {
   Image dest;
   dest.ReForge( src, dt );
   dest.Copy( src );
   return dest;
}

/// \}

} // namespace dip

#endif // DIP_IMAGE_H
