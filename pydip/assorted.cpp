/*
 * PyDIP 3.0, Python bindings for DIPlib 3.0
 *
 * (c)2017, Flagship Biosciences, Inc., written by Cris Luengo.
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

#include "pydip.h"
#include "diplib/color.h"
#include "diplib/display.h"
#include "diplib/file_io.h"
#include "diplib/generation.h"
#include "diplib/geometry.h"
#include "diplib/histogram.h"
#include "diplib/lookup_table.h"

namespace {

dip::ColorSpaceManager colorSpaceManager;
dip::Random randomNumberGenerator;

dip::Image Display(
      dip::Image const& input,
      dip::String const& mappingMode = "lin",
      dip::dfloat lower = 0.0,
      dip::dfloat upper = 1.0,
      dip::String const& complexMode = "abs",
      dip::String const& projectionMode = "mean",
      dip::UnsignedArray const& coordinates = {},
      dip::uint dim1 = 0,
      dip::uint dim2 = 1
) {
   dip::ImageDisplay imageDisplay( input, &colorSpaceManager );
   if( mappingMode.empty() ) {
      imageDisplay.SetRange( dip::ImageDisplay::Limits{ lower, upper } );
   } else {
      imageDisplay.SetRange( mappingMode );
   }
   imageDisplay.SetComplexMode( complexMode );
   if( input.Dimensionality() > 2 ) {
      imageDisplay.SetGlobalStretch( true );
      imageDisplay.SetProjectionMode( projectionMode );
      if( !coordinates.empty()) {
         imageDisplay.SetCoordinates( coordinates );
      }
   }
   if( input.Dimensionality() >= 2 ) { // also for 2D images, you can rotate the output this way
      imageDisplay.SetDirection( dim1, dim2 );
   }
   return imageDisplay.Output();
}

dip::Image DisplayRange(
      dip::Image const& input,
      dip::FloatArray const& range,
      dip::String const& complexMode = "abs",
      dip::String const& projectionMode = "mean",
      dip::UnsignedArray const& coordinates = {},
      dip::uint dim1 = 0,
      dip::uint dim2 = 1
) {
   if( range.empty() ) {
      return Display( input, "lin", 0.0, 1.0, complexMode, projectionMode, coordinates, dim1, dim2 );
   }
   DIP_THROW_IF( range.size() != 2, "Range must be a 2-tuple" );
   return Display( input, "", range[ 0 ], range[ 1 ], complexMode, projectionMode, coordinates, dim1, dim2 );
}

dip::Image DisplayMode(
      dip::Image const& input,
      dip::String const& mappingMode = "lin",
      dip::String const& complexMode = "abs",
      dip::String const& projectionMode = "mean",
      dip::UnsignedArray const& coordinates = {},
      dip::uint dim1 = 0,
      dip::uint dim2 = 1
) {
   return Display( input, mappingMode, 0.0, 1.0, complexMode, projectionMode, coordinates, dim1, dim2 );
}

} // namespace

void init_assorted( py::module& m ) {
   // diplib/color.h
   auto mcol = m.def_submodule("ColorSpaceManager", "A Tool to convert images from one color space to another.");
   mcol.def( "Convert", []( dip::Image const& in, dip::String const& colorSpaceName ){ return colorSpaceManager.Convert( in, colorSpaceName ); }, "in"_a, "colorSpaceName"_a = "RGB" );
   mcol.def( "IsDefined", []( dip::String const& colorSpaceName ){ return colorSpaceManager.IsDefined( colorSpaceName ); }, "colorSpaceName"_a = "RGB" );
   mcol.def( "NumberOfChannels", []( dip::String const& colorSpaceName ){ return colorSpaceManager.NumberOfChannels( colorSpaceName ); }, "colorSpaceName"_a = "RGB" );
   mcol.def( "CanonicalName", []( dip::String const& colorSpaceName ){ return colorSpaceManager.CanonicalName( colorSpaceName ); }, "colorSpaceName"_a = "RGB" );
   // TODO: WhitePoint stuff

   // diplib/display.h
   m.def( "ImageDisplay", &DisplayRange, "in"_a, "range"_a, "complexMode"_a = "abs", "projectionMode"_a = "mean", "coordinates"_a = dip::UnsignedArray{}, "dim1"_a = 0, "dim2"_a = 1 );
   m.def( "ImageDisplay", &DisplayMode, "in"_a, "mappingMode"_a = "", "complexMode"_a = "abs", "projectionMode"_a = "mean", "coordinates"_a = dip::UnsignedArray{}, "dim1"_a = 0, "dim2"_a = 1 );

   // diplib/file_io.h
   m.def( "ImageReadICS", py::overload_cast< dip::String const&, dip::RangeArray const&, dip::Range const&, dip::String const& >( &dip::ImageReadICS ),
          "filename"_a, "roi"_a = dip::RangeArray{}, "channels"_a = dip::Range{}, "mode"_a = "" );
   m.def( "ImageReadICS", py::overload_cast< dip::String const&, dip::UnsignedArray const&, dip::UnsignedArray const&, dip::UnsignedArray const&, dip::Range const&, dip::String const& >( &dip::ImageReadICS ),
          "filename"_a, "origin"_a = dip::UnsignedArray{}, "sizes"_a = dip::UnsignedArray{}, "spacing"_a = dip::UnsignedArray{}, "channels"_a = dip::Range{}, "mode"_a = "" );
   m.def( "ImageIsICS", &dip::ImageIsICS, "filename"_a );
   m.def( "ImageWriteICS", py::overload_cast< dip::Image const&, dip::String const&, dip::StringArray const&, dip::uint, dip::StringSet const& >( &dip::ImageWriteICS ),
          "image"_a, "filename"_a, "history"_a = dip::StringArray{}, "significantBits"_a = 0, "options"_a = dip::StringSet {} );

   m.def( "ImageReadTIFF", py::overload_cast< dip::String const&, dip::Range const& >( &dip::ImageReadTIFF ), "filename"_a, "imageNumbers"_a = dip::Range{ 0 } );
   m.def( "ImageReadTIFFSeries", py::overload_cast< dip::StringArray const& >( &dip::ImageReadTIFFSeries ), "filenames"_a );
   m.def( "ImageIsTIFF", &dip::ImageIsTIFF, "filename"_a );
   m.def( "ImageWriteTIFF", py::overload_cast< dip::Image const&, dip::String const&, dip::String const&, dip::uint >( &dip::ImageWriteTIFF ),
          "image"_a, "filename"_a, "compression"_a = "", "jpegLevel"_a = 80 );

   // diplib/generation.h
   m.def( "FillDelta", &dip::FillDelta, "out"_a, "origin"_a = "" );
   m.def( "CreateDelta", py::overload_cast< dip::Image const&, dip::String const& >( &dip::CreateDelta ), "in"_a, "origin"_a = "" );
   m.def( "SetBorder", &dip::SetBorder, "out"_a, "value"_a = dip::Image::Pixel{ 0 }, "size"_a = 1 );
   m.def( "FillRamp", &dip::FillRamp, "out"_a, "dimension"_a, "mode"_a = dip::StringSet{} );
   m.def( "CreateRamp", py::overload_cast< dip::Image const&, dip::uint, dip::StringSet const& >( &dip::CreateRamp ), "in"_a, "dimension"_a, "mode"_a = dip::StringSet{} );
   m.def( "FillXCoordinate", &dip::FillXCoordinate, "out"_a, "mode"_a = dip::StringSet{} );
   m.def( "CreateXCoordinate", py::overload_cast< dip::Image const&, dip::StringSet const& >( &dip::CreateXCoordinate ), "in"_a, "mode"_a = dip::StringSet{} );
   m.def( "FillYCoordinate", &dip::FillYCoordinate, "out"_a, "mode"_a = dip::StringSet{} );
   m.def( "CreateYCoordinate", py::overload_cast< dip::Image const&, dip::StringSet const& >( &dip::CreateYCoordinate ), "in"_a, "mode"_a = dip::StringSet{} );
   m.def( "FillZCoordinate", &dip::FillZCoordinate, "out"_a, "mode"_a = dip::StringSet{} );
   m.def( "CreateZCoordinate", py::overload_cast< dip::Image const&, dip::StringSet const& >( &dip::CreateZCoordinate ), "in"_a, "mode"_a = dip::StringSet{} );
   m.def( "FillRadiusCoordinate", &dip::FillRadiusCoordinate, "out"_a, "mode"_a = dip::StringSet{} );
   m.def( "CreateRadiusCoordinate", py::overload_cast< dip::Image const&, dip::StringSet const& >( &dip::CreateRadiusCoordinate ), "in"_a, "mode"_a = dip::StringSet{} );
   m.def( "FillRadiusSquareCoordinate", &dip::FillRadiusSquareCoordinate, "out"_a, "mode"_a = dip::StringSet{} );
   m.def( "CreateRadiusSquareCoordinate", py::overload_cast< dip::Image const&, dip::StringSet const& >( &dip::CreateRadiusSquareCoordinate ), "in"_a, "mode"_a = dip::StringSet{} );
   m.def( "FillPhiCoordinate", &dip::FillPhiCoordinate, "out"_a, "mode"_a = dip::StringSet{} );
   m.def( "CreatePhiCoordinate", py::overload_cast< dip::Image const&, dip::StringSet const& >( &dip::CreatePhiCoordinate ), "in"_a, "mode"_a = dip::StringSet{} );
   m.def( "FillThetaCoordinate", &dip::FillThetaCoordinate, "out"_a, "mode"_a = dip::StringSet{} );
   m.def( "CreateThetaCoordinate", py::overload_cast< dip::Image const&, dip::StringSet const& >( &dip::CreateThetaCoordinate ), "in"_a, "mode"_a = dip::StringSet{} );
   m.def( "FillCoordinates", &dip::FillCoordinates, "out"_a, "mode"_a = dip::StringSet{}, "system"_a = "" );
   m.def( "CreateCoordinates", py::overload_cast< dip::Image const&, dip::StringSet const&, dip::String const& >( &dip::CreateCoordinates ),
          "in"_a, "mode"_a = dip::StringSet{}, "system"_a = "" );
   m.def( "UniformNoise", []( dip::Image const& in, dip::dfloat lowerBound, dip::dfloat upperBound ){ return dip::UniformNoise( in, randomNumberGenerator, lowerBound, upperBound ); },
          "in"_a, "lowerBound"_a = 0.0, "upperBound"_a = 1.0 );
   m.def( "GaussianNoise", []( dip::Image const& in, dip::dfloat variance ){ return dip::GaussianNoise( in, randomNumberGenerator, variance ); },
          "in"_a, "variance"_a = 1.0 );
   m.def( "PoissonNoise", []( dip::Image const& in, dip::dfloat conversion ){ return dip::PoissonNoise( in, randomNumberGenerator, conversion ); },
          "in"_a, "conversion"_a = 1.0 );
   m.def( "BinaryNoise", []( dip::Image const& in, dip::dfloat p10, dip::dfloat p01 ){ return dip::BinaryNoise( in, randomNumberGenerator, p10, p01 ); },
          "in"_a, "p10"_a = 0.05, "p01"_a = 0.05 );
   m.def( "FillColoredNoise", []( dip::Image& out, dip::dfloat variance, dip::dfloat color ){ dip::FillColoredNoise( out, randomNumberGenerator, variance, color ); },
          "out"_a, "variance"_a = 1.0, "color"_a = -2.0 );
   m.def( "ColoredNoise", []( dip::Image const& in, dip::dfloat variance, dip::dfloat color  ){ return dip::ColoredNoise( in, randomNumberGenerator, variance, color ); },
          "in"_a, "variance"_a = 1.0, "color"_a = -2.0 );

   // diplib/geometry.h
   m.def( "Wrap", py::overload_cast< dip::Image const&, dip::IntegerArray const& >( &dip::Wrap ), "in"_a, "wrap"_a );
   m.def( "Subsampling", py::overload_cast< dip::Image const&, dip::UnsignedArray const& >( &dip::Subsampling ), "in"_a, "sample"_a );
   m.def( "Resampling", py::overload_cast< dip::Image const&, dip::FloatArray const&, dip::FloatArray const&, dip::String const&, dip::StringArray const& >( &dip::Resampling ),
         "in"_a, "zoom"_a = dip::FloatArray{ 1.0 }, "shift"_a = dip::FloatArray{ 0.0 }, "interpolationMethod"_a = "", "boundaryCondition"_a = dip::StringArray{} );
   m.def( "Shift", py::overload_cast< dip::Image const&, dip::FloatArray const&, dip::String const&, dip::StringArray const& >( &dip::Shift ),
         "in"_a, "shift"_a = dip::FloatArray{ 0.0 }, "interpolationMethod"_a = "ft", "boundaryCondition"_a = dip::StringArray{} );
   m.def( "Skew", py::overload_cast< dip::Image const&, dip::FloatArray, dip::uint, dip::String const&, dip::StringArray const& >( &dip::Skew ),
         "in"_a, "shearArray"_a, "axis"_a, "interpolationMethod"_a = "", "boundaryCondition"_a = dip::StringArray{} );
   m.def( "Skew", py::overload_cast< dip::Image const&, dip::dfloat, dip::uint, dip::uint, dip::String const&, dip::String const& >( &dip::Skew ),
         "in"_a, "shear"_a, "skew"_a, "axis"_a, "interpolationMethod"_a = "", "boundaryCondition"_a = "" );
   m.def( "Rotation", py::overload_cast< dip::Image const&, dip::dfloat, dip::uint, dip::uint, dip::String const&, dip::String const& >( &dip::Rotation ),
         "in"_a, "angle"_a, "dimension1"_a, "dimension2"_a, "interpolationMethod"_a = "", "boundaryCondition"_a = "add zeros" );
   m.def( "Rotation2d", py::overload_cast< dip::Image const&, dip::dfloat, dip::String const&, dip::String const& >( &dip::Rotation2d ),
         "in"_a, "angle"_a, "interpolationMethod"_a = "", "boundaryCondition"_a = "" );
   m.def( "Rotation3d", py::overload_cast< dip::Image const&, dip::dfloat, dip::uint, dip::String const&, dip::String const& >( &dip::Rotation3d ),
         "in"_a, "angle"_a, "axis"_a = 2, "interpolationMethod"_a = "", "boundaryCondition"_a = "" );
   m.def( "Rotation3d", py::overload_cast< dip::Image const&, dip::dfloat, dip::dfloat, dip::dfloat, dip::String const&, dip::String const& >( &dip::Rotation3d ),
         "in"_a, "alpha"_a, "beta"_a, "gamma"_a, "interpolationMethod"_a = "", "boundaryCondition"_a = "" );

   // diplib/histogram.h
   m.def( "Histogram", []( dip::Image const& input ){
      dip::Histogram histogram( input );
      dip::Image im = histogram.GetImage();
      std::vector< dip::FloatArray > bins( histogram.Dimensionality() );
      for( dip::uint ii = 0; ii < bins.size(); ++ii ) {
         bins[ ii ] = histogram.BinCenters( ii );
      }
      return py::make_tuple( im, bins ).release();
   }, "input"_a );
   m.def( "Histogram", []( dip::Image const& input1, dip::Image const& input2 ){
      dip::Histogram histogram( input1, input2 );
      dip::Image im = histogram.GetImage();
      std::vector< dip::FloatArray > bins( 2 );
      bins[ 0 ] = histogram.BinCenters( 0 );
      bins[ 1 ] = histogram.BinCenters( 1 );
      return py::make_tuple( im, bins ).release();
   }, "input1"_a, "input2"_a );

   // diplib/lookup_table.h
   m.def( "LookupTable", []( dip::Image const& in, dip::Image const& lut, dip::FloatArray const& index, dip::String const& interpolation,
                             dip::String const& mode, dip::dfloat lowerValue, dip::dfloat upperValue ) {
      dip::LookupTable lookupTable( lut, index );
      if( mode == "clamp" ) {
         lookupTable.ClampOutOfBoundsValues(); // is the default...
      } else if( mode == "values" ) {
         lookupTable.SetOutOfBoundsValue( lowerValue, upperValue );
      } else if( mode == "keep" ) {
         lookupTable.KeepInputValueOnOutOfBounds();
      } else {
         DIP_THROW( dip::E::INVALID_FLAG );
      }
      return lookupTable.Apply( in, interpolation );
   }, "in"_a, "lut"_a, "index"_a = dip::FloatArray{}, "interpolation"_a = "linear", "mode"_a = "clamp", "lowerValue"_a = 0.0, "upperValue"_a = 0.0
   );
}
