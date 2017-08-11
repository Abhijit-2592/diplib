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
#include "diplib/morphology.h"
#include "diplib/binary.h"       // TODO: include functions from diplib/binary.h

namespace {

dip::String StructuringElementRepr( dip::StructuringElement const& s ) {
   std::ostringstream os;
   os << "<";
   if( s.IsCustom() ) {
      os << "Custom StructuringElement";
      if( s.IsFlat() ) {
         os << ", flat";
      } else {
         os << ", grey-valued";
      }
   } else {
      switch( s.Shape() ) {
         case dip::StructuringElement::ShapeCode::RECTANGULAR:
            os << "Rectangular";
            break;
         case dip::StructuringElement::ShapeCode::ELLIPTIC:
            os << "Elliptic";
            break;
         case dip::StructuringElement::ShapeCode::DIAMOND:
            os << "Diamond";
            break;
         case dip::StructuringElement::ShapeCode::OCTAGONAL:
            os << "Octagonal";
            break;
         case dip::StructuringElement::ShapeCode::LINE:
            os << "Line";
            break;
         case dip::StructuringElement::ShapeCode::FAST_LINE:
            os << "Fast line";
            break;
         case dip::StructuringElement::ShapeCode::PERIODIC_LINE:
            os << "Periodic line";
            break;
         case dip::StructuringElement::ShapeCode::DISCRETE_LINE:
            os << "Discrete line";
            break;
         case dip::StructuringElement::ShapeCode::INTERPOLATED_LINE:
            os << "Interpolated line";
            break;
         case dip::StructuringElement::ShapeCode::PARABOLIC:
            os << "Parabolic";
            break;
         default:
            os << "Unknown";
            break;
      }
      os << " StructuringElement with parameters " << s.Params();
   }
   if( s.IsMirrored() ) {
      os << ", mirrored";
   }
   os << ">";
   return os.str();
}

} // namespace

void init_morphology( py::module& m ) {
   auto se = py::class_< dip::StructuringElement >( m, "SE", "Represents the structuring element to use in morphological operations\n(dip::StructuringElement in DIPlib)." );
   // Constructors
   se.def( py::init<>() );
   se.def( py::init< dip::Image const& >(), "image"_a );
   se.def( py::init< dip::String const& >(), "shape"_a );
   se.def( py::init< dip::dfloat, dip::String const& >(), "param"_a, "shape"_a = "elliptic" );
   se.def( py::init< dip::FloatArray, dip::String const& >(), "param"_a, "shape"_a = "elliptic" );
   se.def( "Mirror", &dip::StructuringElement::Mirror );
   se.def( "__repr__", &StructuringElementRepr );
   py::implicitly_convertible< py::buffer, dip::StructuringElement >();
   py::implicitly_convertible< py::str, dip::StructuringElement >();
   py::implicitly_convertible< py::float_, dip::StructuringElement >();
   py::implicitly_convertible< py::int_, dip::StructuringElement >();
   py::implicitly_convertible< py::list, dip::StructuringElement >();

   m.def( "Dilation", py::overload_cast<
                dip::Image const&,
                dip::StructuringElement const&,
                dip::StringArray const& >( &dip::Dilation ),
          "in"_a,
          "se"_a = dip::StructuringElement{},
          "boundaryCondition"_a = dip::StringArray{} );

   m.def( "Erosion", py::overload_cast<
                dip::Image const&,
                dip::StructuringElement const&,
                dip::StringArray const& >( &dip::Erosion ),
          "in"_a,
          "se"_a = dip::StructuringElement{},
          "boundaryCondition"_a = dip::StringArray{} );

   m.def( "Closing", py::overload_cast<
                dip::Image const&,
                dip::StructuringElement const&,
                dip::StringArray const& >( &dip::Closing ),
          "in"_a,
          "se"_a = dip::StructuringElement{},
          "boundaryCondition"_a = dip::StringArray{} );

   m.def( "Opening", py::overload_cast<
                dip::Image const&,
                dip::StructuringElement const&,
                dip::StringArray const& >( &dip::Opening ),
          "in"_a,
          "se"_a = dip::StructuringElement{},
          "boundaryCondition"_a = dip::StringArray{} );

   m.def( "Tophat", py::overload_cast<
                dip::Image const&,
                dip::StructuringElement const&,
                dip::String const&,
                dip::String const&,
                dip::StringArray const& >( &dip::Tophat ),
          "in"_a,
          "se"_a = dip::StructuringElement{},
          "edgeType"_a = "texture",
          "polarity"_a = "white",
          "boundaryCondition"_a = dip::StringArray{} );

   m.def( "MorphologicalGradientMagnitude", py::overload_cast<
                dip::Image const&,
                dip::StructuringElement const&,
                dip::StringArray const& >( &dip::MorphologicalGradientMagnitude ),
          "in"_a,
          "se"_a = dip::StructuringElement{},
          "boundaryCondition"_a = dip::StringArray{} );

   m.def( "Lee", py::overload_cast<
                dip::Image const&,
                dip::StructuringElement const&,
                dip::String const&,
                dip::String const&,
                dip::StringArray const& >( &dip::Lee ),
          "in"_a,
          "se"_a = dip::StructuringElement{},
          "edgeType"_a = "texture",
          "sign"_a = "unsigned",
          "boundaryCondition"_a = dip::StringArray{} );

   m.def( "Watershed", py::overload_cast<
                dip::Image const&,
                dip::Image const&,
                dip::uint,
                dip::dfloat,
                dip::uint,
                dip::StringSet const& >( &dip::Watershed ),
          "in"_a,
          "mask"_a = dip::Image{},
          "connectivity"_a = 1,
          "maxDepth"_a = 1.0,
          "maxSize"_a = 0,
          "flags"_a = dip::StringSet{} );

   m.def( "SeededWatershed", py::overload_cast<
                dip::Image const&,
                dip::Image const&,
                dip::Image const&,
                dip::uint,
                dip::dfloat,
                dip::uint,
                dip::StringSet const& >( &dip::SeededWatershed ),
          "in"_a,
          "seeds"_a,
          "mask"_a = dip::Image{},
          "connectivity"_a = 1,
          "maxDepth"_a = 1.0,
          "maxSize"_a = 0,
          "flags"_a = dip::StringSet{} );

   m.def( "Maxima", py::overload_cast<
                dip::Image const&,
                dip::Image const&,
                dip::uint,
                dip::String const& >( &dip::Maxima ),
          "in"_a,
          "mask"_a = dip::Image{},
          "connectivity"_a = 1,
          "output"_a = "binary" );

   m.def( "Minima", py::overload_cast<
                dip::Image const&,
                dip::Image const&,
                dip::uint,
                dip::String const& >( &dip::Minima ),
          "in"_a,
          "mask"_a = dip::Image{},
          "connectivity"_a = 1,
          "output"_a = "binary" );

   m.def( "WatershedMinima", py::overload_cast<
                dip::Image const&,
                dip::Image const&,
                dip::uint,
                dip::dfloat,
                dip::uint,
                dip::String const& >( &dip::WatershedMinima ),
          "in"_a,
          "mask"_a = dip::Image{},
          "connectivity"_a = 1,
          "maxDepth"_a = 1,
          "maxSize"_a = 0,
          "output"_a = "binary" );

   m.def( "WatershedMaxima", py::overload_cast<
                dip::Image const&,
                dip::Image const&,
                dip::uint,
                dip::dfloat,
                dip::uint,
                dip::String const& >( &dip::WatershedMaxima ),
          "in"_a,
          "mask"_a = dip::Image{},
          "connectivity"_a = 1,
          "maxDepth"_a = 1,
          "maxSize"_a = 0,
          "output"_a = "binary" );

   m.def( "MorphologicalReconstruction", py::overload_cast<
                dip::Image const&,
                dip::Image const&,
                dip::uint,
                dip::String const& >( &dip::MorphologicalReconstruction ),
          "marker"_a,
          "in"_a,
          "connectivity"_a = 1,
          "direction"_a = "dilation" );

   m.def( "HMinima", py::overload_cast<
                dip::Image const&,
                dip::dfloat,
                dip::uint >( &dip::HMinima ),
          "in"_a,
          "h"_a,
          "connectivity"_a = 1 );

   m.def( "HMaxima", py::overload_cast<
                dip::Image const&,
                dip::dfloat,
                dip::uint >( &dip::HMaxima ),
          "in"_a,
          "h"_a,
          "connectivity"_a = 1 );

   m.def( "AreaOpening", py::overload_cast<
                dip::Image const&,
                dip::Image const&,
                dip::uint,
                dip::uint,
                dip::String const& >( &dip::AreaOpening ),
          "in"_a,
          "mask"_a = dip::Image{},
          "filterSize"_a,
          "connectivity"_a = 1,
          "polarity"_a = "opening" );

   m.def( "AreaClosing", py::overload_cast<
                dip::Image const&,
                dip::Image const&,
                dip::uint,
                dip::uint >( &dip::AreaClosing ),
          "in"_a,
          "mask"_a = dip::Image{},
          "filterSize"_a,
          "connectivity"_a = 1 );

   m.def( "PathOpening", py::overload_cast<
                dip::Image const&,
                dip::Image const&,
                dip::uint,
                dip::String const&,
                dip::String const& >( &dip::PathOpening ),
          "in"_a,
          "mask"_a = dip::Image{},
          "length"_a = 7,
          "polarity"_a = "opening",
          "mode"_a = "normal" );

   m.def( "DirectedPathOpening", py::overload_cast<
                dip::Image const&,
                dip::Image const&,
                dip::IntegerArray const&,
                dip::String const&,
                dip::String const& >( &dip::DirectedPathOpening ),
          "in"_a,
          "mask"_a = dip::Image{},
          "filterParam"_a = dip::IntegerArray{},
          "polarity"_a = "opening",
          "mode"_a = "normal" );

   m.def( "OpeningByReconstruction", py::overload_cast<
                dip::Image const&,
                dip::StructuringElement const&,
                dip::uint,
                dip::StringArray const& >( &dip::OpeningByReconstruction ),
          "in"_a,
          "se"_a = dip::StructuringElement{},
          "connectivity"_a = 1,
          "boundaryCondition"_a = dip::StringArray{} );

   m.def( "ClosingByReconstruction", py::overload_cast<
                dip::Image const&,
                dip::StructuringElement const&,
                dip::uint,
                dip::StringArray const& >( &dip::ClosingByReconstruction ),
          "in"_a,
          "se"_a = dip::StructuringElement{},
          "connectivity"_a = 1,
          "boundaryCondition"_a = dip::StringArray{} );

}
