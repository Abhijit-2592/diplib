/*
 * DIPlib 3.0
 * This file contains declarations and definitions for chain-code--based 2D measurements
 *
 * (c)2016-2017, Cris Luengo.
 * Based on original DIPlib code: (c)1995-2014, Delft University of Technology.
 */

#ifndef DIP_CHAIN_CODE_H
#define DIP_CHAIN_CODE_H

#include <vector>
#include <cmath>

#include "diplib.h"


/// \file
/// \brief Declares `dip::ChainCode`, `dip::Polygon`, `dip::ConvexHull`, and related functions.
/// \see measurement


namespace dip {


/// \addtogroup infrastructure
/// \{


/// \brief Contains the various %Feret diameters as returned by `dip::ConvexHull::Feret` and `dip::ChainCode::Feret`.
struct FeretValues {
   dfloat maxDiameter = 0.0;        ///< The maximum %Feret diameter
   dfloat minDiameter = 0.0;        ///< The minimum %Feret diameter
   dfloat maxPerpendicular = 0.0;   ///< The %Feret diameter perpendicular to `minDiameter`
   dfloat maxAngle = 0.0;           ///< The angle at which `maxDiameter` was measured
   dfloat minAngle = 0.0;           ///< The angle at which `minDiameter` was measured
};

/// \brief Holds the various output values of the `dip::RadiusStatistics` and `dip::ConvexHull::RadiusStatistics` function.
struct RadiusValues {
   dfloat mean = 0.0;   ///< Mean radius
   dfloat var = 0.0;    ///< Radius variance
   dfloat max = 0.0;    ///< Maximum radius
   dfloat min = 0.0;    ///< Minimum radius
   /// Computes a circularity measure given by the coefficient of variation of the radii of the object.
   dfloat Circularity() {
      return std::sqrt( var ) / mean;
   }
};


//
// Vertex of a polygon
//


/// \brief Encodes a location in a 2D image
template< typename T >
struct Vertex {
   T x;   ///< The x-coordinate
   T y;   ///< The y-coordinate

   /// Default constructor
   Vertex() : x( T( 0 )), y( T( 0 )) {}
   /// Constructor
   Vertex( T x, T y ) : x( x ), y( y ) {}

   /// Add a vertex
   template< typename V >
   Vertex& operator+=( Vertex< V > v ) {
      x += T( v.x );
      y += T( v.y );
      return *this;
   }
   /// Subtract a vertex
   template< typename V >
   Vertex& operator-=( Vertex< V > v ) {
      x -= T( v.x );
      y -= T( v.y );
      return *this;
   }
   /// Add a constant to both coordinate components
   Vertex& operator+=( T n ) {
      x += n;
      y += n;
      return *this;
   }
   /// Subtract a constant from both coordinate components
   Vertex& operator-=( T n ) {
      x -= n;
      y -= n;
      return *this;
   }
   /// Scale by a constant, isotropically
   Vertex& operator*=( dfloat n ) {
      x = T( dfloat( x ) * n );
      y = T( dfloat( y ) * n );
      return *this;
   }
   /// Scale by the invese of a constant, isotropically
   Vertex& operator/=( dfloat n ) {
      x = T( dfloat( x ) / n );
      y = T( dfloat( y ) / n );
      return *this;
   }
};

/// \brief Compare two vertices
template< typename T >
inline bool operator==( Vertex< T > v1, Vertex< T > v2 ) {
   return ( v1.x == v2.x ) && ( v1.y == v2.y );
}

template< typename T >
/// \brief Compare two vertices
inline bool operator!=( Vertex< T > v1, Vertex< T > v2 ) {
   return !( v1 == v2 );
}

/// \brief The norm of the vector v2-v1.
template< typename T >
dfloat Distance( Vertex< T > const& v1, Vertex< T > const& v2 ) {
   Vertex< T > v = v2 - v1;
   return std::hypot( v.x, v.y );
}

/// \brief The square norm of the vector v2-v1.
template< typename T >
dfloat DistanceSquare( Vertex< T > const& v1, Vertex< T > const& v2 ) {
   Vertex< T > v = v2 - v1;
   return v.x * v.x + v.y * v.y;
}

/// \brief The angle of the vector v2-v1.
template< typename T >
dfloat Angle( Vertex< T > const& v1, Vertex< T > const& v2 ) {
   Vertex< T > v = v2 - v1;
   return std::atan2( v.y, v.x );
}

/// \brief Compute the z component of the cross product of vectors v1 and v2
template< typename T >
dfloat CrossProduct( Vertex< T > const& v1, Vertex< T > const& v2 ) {
   return v1.x * v2.y - v1.y * v2.x;
}

/// \brief Compute the z component of the cross product of vectors v2-v1 and v3-v1
template< typename T >
dfloat ParallelogramSignedArea( Vertex< T > const& v1, Vertex< T > const& v2, Vertex< T > const& v3 ) {
   return CrossProduct( v2 - v1, v3 - v1 );
}

/// \brief Compute the area of the triangle formed by vertices v1, v2 and v3
template< typename T >
dfloat TriangleArea( Vertex< T > const& v1, Vertex< T > const& v2, Vertex< T > const& v3 ) {
   return std::abs( ParallelogramSignedArea< T >( v1, v2, v3 ) / 2.0 );
}

/// \brief Compute the height of the triangle formed by vertices v1, v2 and v3, with v3 the tip
template< typename T >
dfloat TriangleHeight( Vertex< T > const& v1, Vertex< T > const& v2, Vertex< T > const& v3 ) {
   return std::abs( ParallelogramSignedArea< T >( v1, v2, v3 ) / Distance< T >( v1, v2 ));
}

using VertexFloat = Vertex< dfloat >;        ///< A vertex with floating-point coordinates
using VertexInteger = Vertex< dip::sint >;   ///< A vertex with integer coordinates

/// \brief Add two vertices together, with identical types
template< typename T >
Vertex< T > operator+( Vertex< T > lhs, Vertex< T > const& rhs ) {
   lhs += rhs;
   return lhs;
}

/// \brief Add two vertices together, where the LHS is floating-point and the RHS is integer
inline VertexFloat operator+( VertexFloat lhs, VertexInteger const& rhs ) {
   lhs += rhs;
   return lhs;
}

/// \brief Add two vertices together, where the LHS is integer and the RHS is floating-point
inline VertexFloat operator+( VertexInteger const& lhs, VertexFloat rhs ) {
   rhs += lhs;
   return rhs;
}

/// \brief Subtract two vertices from each other
template< typename T >
Vertex< T > operator-( Vertex< T > lhs, Vertex< T > const& rhs ) {
   lhs -= rhs;
   return lhs;
}

/// \brief Subtract two vertices from each other, where the LHS is floating-point and the RHS is integer
inline VertexFloat operator-( VertexFloat lhs, VertexInteger const& rhs ) {
   lhs -= rhs;
   return lhs;
}

/// \brief Subtract two vertices from each other, where the LHS is integer and the RHS is floating-point
inline VertexFloat operator-( VertexInteger const& lhs, VertexFloat const& rhs ) {
   VertexFloat out{ static_cast< dfloat >( lhs.x ),
                    static_cast< dfloat >( lhs.y ) };
   out -= rhs;
   return out;
}

/// \brief Add a vertex and a constant
template< typename T >
Vertex< T > operator+( Vertex< T > v, T n ) {
   v += n;
   return v;
}

/// \brief Subtract a vertex and a constant
template< typename T >
Vertex< T > operator-( Vertex< T > v, T n ) {
   v -= n;
   return v;
}

/// \brief Multiply a vertex and a constant
template< typename T >
Vertex< T > operator*( Vertex< T > v, dfloat n ) {
   v *= n;
   return v;
}

/// \brief Divide a vertex by a constant
template< typename T >
Vertex< T > operator/( Vertex< T > v, dfloat n ) {
   v /= n;
   return v;
}


//
// Covariance matrix
//


/// \brief A 2D covariance matrix for computation with 2D vertices.
///
/// The matrix is real, symmetric, positive semidefinite. See `dip::Polygon::CovarianceMatrix`
/// for how to create a covariance matrix.
///
/// The elements stored are `xx`, `xy` and `yy`, with `xx` the top-left element, and `xy` both
/// the off-diagonal elements, which are equal by definition.
class CovarianceMatrix {
   public:
      /// \brief Default-initialized covariance matrix is all zeros
      CovarianceMatrix() : xx_( 0 ), xy_( 0 ), yy_( 0 ) {}
      /// \brief Construct a covariance matrix as the outer product of a vector and itself
      CovarianceMatrix( VertexFloat v ) : xx_( v.x * v.x ), xy_( v.x * v.y ), yy_( v.y * v.y ) {}
      /// \brief Read matrix element
      dfloat xx() const { return xx_; }
      /// \brief Read matrix element
      dfloat xy() const { return xy_; }
      /// \brief Read matrix element
      dfloat yy() const { return yy_; }
      /// \brief Compute determinant of matrix
      dfloat Det() const { return xx_ * yy_ - xy_ * xy_; }
      /// \brief Compute inverse of matrix
      CovarianceMatrix Inv() const {
         dfloat d = Det();
         CovarianceMatrix out;
         out.xx_ =  yy_ / d;
         out.xy_ = -xy_ / d;
         out.yy_ =  xx_ / d;
         return out;
      }
      /// \brief Add other matrix to this matrix
      CovarianceMatrix& operator+=( CovarianceMatrix const& other ) {
         xx_ += other.xx_;
         xy_ += other.xy_;
         yy_ += other.yy_;
         return * this;
      }
      /// \brief Scale matrix
      CovarianceMatrix& operator*=( dfloat d ) {
         xx_ *= d;
         xy_ *= d;
         yy_ *= d;
         return * this;
      }
      /// \brief Scale matrix
      CovarianceMatrix& operator/=( dfloat d ) {
         return operator*=( 1.0 / d );
      }
      /// \brief Computes v' * C * v, with v' the transpose of v.
      /// This is a positive scalar if v is non-zero, because C (this matrix) is positive semidefinite.
      dfloat Project( VertexFloat const& v ) {
         return v.x * v.x * xx_ + 2 * v.x * v.y * xy_ + v.y * v.y * yy_;
      }

      /// \brief Container for matrix eigenvalues
      struct Eigenvalues {
         dfloat largest;   ///< Largest eigenvalue
         dfloat smallest;  ///< Smallest eigenvalue
         /// \brief Computes eccentricity using the two eigenvalues of the covarance matrix.
         dfloat Eccentricity() {
            // Eccentricity according to https://en.wikipedia.org/wiki/Image_moment
            if( largest <= 0 ) {    // largest == 0 really, it cannot be negative.
               return 0;            // if largest == 0, then smallest == 0 also.
            } else {
               return std::sqrt( 1 - smallest / largest );
            }
         }
      };
      /// \brief Compute eigenvalues of matrix
      Eigenvalues Eig() {
         // Eigenvalue calculation according to e.g. http://www.math.harvard.edu/archive/21b_fall_04/exhibits/2dmatrices/index.html
         dfloat mmu2 = ( xx_ + yy_ ) / 2.0;
         dfloat dmu2 = ( xx_ - yy_ ) / 2.0;
         dfloat sqroot = std::sqrt( xy_ * xy_ + dmu2 * dmu2 );
         Eigenvalues lambda;
         lambda.largest = mmu2 + sqroot;
         lambda.smallest = mmu2 - sqroot;
         return lambda;
      }

      /// \brief Container for ellipse parameters
      struct EllipseParameters {
         dfloat majorAxis;    ///< Major axis length
         dfloat minorAxis;    ///< Minor axis length
         dfloat orientation;  ///< Orientation of major axis
      };
      /// \brief Compute parameters of ellipse with same covariance matrix.
      EllipseParameters Ellipse() {
         // Eigenvector calculation according to e.g. http://www.math.harvard.edu/archive/21b_fall_04/exhibits/2dmatrices/index.html
         Eigenvalues lambda = Eig();
         EllipseParameters out;
         out.majorAxis = 4.0 * std::sqrt( lambda.largest );
         out.minorAxis = 4.0 * std::sqrt( lambda.smallest );
         out.orientation = std::atan2( lambda.largest - xx_, xy_ ); // eigenvector is {xy, lambda.largest - xx}
         return out;
      }

   private:
      dfloat xx_, xy_, yy_;
};


//
// Polygon, convex hull
//


class ConvexHull; // Forward declaration

/// \brief A polygon with floating-point vertices.
struct Polygon {

   std::vector< VertexFloat > vertices;  ///< The vertices

   /// \brief Computes the area of a polygon
   dfloat Area() const {
      if( vertices.size() < 3 ) {
         return 0; // Should we generate an error instead?
      }
      dfloat sum = CrossProduct( vertices.back(), vertices[ 0 ] );
      for( dip::uint ii = 1; ii < vertices.size(); ++ii ) {
         sum += CrossProduct( vertices[ ii - 1 ], vertices[ ii ] );
      }
      return sum / 2.0;
   }

   /// \brief Computes the centroid of the polygon
   VertexFloat Centroid() const {
      if( vertices.size() < 3 ) {
         return { 0, 0 }; // Should we generate an error instead?
      }
      dfloat v = CrossProduct( vertices.back(), vertices[ 0 ] );
      dfloat sum = v;
      dfloat xsum = ( vertices.back().x + vertices[ 0 ].x ) * v;
      dfloat ysum = ( vertices.back().y + vertices[ 0 ].y ) * v;
      for( dip::uint ii = 1; ii < vertices.size(); ++ii ) {
         v = CrossProduct( vertices[ ii - 1 ], vertices[ ii ] );
         sum += v;
         xsum += ( vertices[ ii - 1 ].x + vertices[ ii ].x ) * v;
         ysum += ( vertices[ ii - 1 ].y + vertices[ ii ].y ) * v;
      }
      return VertexFloat{ xsum, ysum } / ( 3 * sum );
   }

   /// \brief Returns the covariance matrix for the vertices of the polygon, using centroid `g`.
   dip::CovarianceMatrix CovarianceMatrix( VertexFloat const& g ) const {
      dip::CovarianceMatrix C;
      if( vertices.size() >= 3 ) {
         for( auto& v : vertices ) {
            C += dip::CovarianceMatrix( v - g );
         }
         C /= vertices.size();
      }
      return C;
   }
   /// \brief Returns the covariance matrix for the vertices of the polygon.
   dip::CovarianceMatrix CovarianceMatrix() const {
      return this->CovarianceMatrix( Centroid() );
   }

   /// \brief Computes the length of a polygon (i.e. perimeter). If the polygon represents a pixelated object,
   /// this function will overestimate the object's perimeter. Use `dip::ChainCode::Length` instead.
   dfloat Length() const {
      if( vertices.size() < 2 ) {
         return 0; // Should we generate an error instead?
      }
      dfloat sum = Distance( vertices.back(), vertices[ 0 ] );
      for( dip::uint ii = 1; ii < vertices.size(); ++ii ) {
         sum += Distance( vertices[ ii - 1 ], vertices[ ii ] );
      }
      return sum;
   }

   /// \brief Returns statistics on the radii of the polygon. The radii are the distances between the centroid
   /// and each of the vertices.
   RadiusValues RadiusStatistics() const;

   /// \brief Compares a polygon to the ellipse with the same covariance matrix, returning the coeffient of
   /// variation of the distance of vertices to the ellipse.
   ///
   /// See: M. Yang, K. Kpalma and J. Ronsin, "A Survey of Shape Feature Extraction Techniques",
   /// in: Pattern Recognition Techniques, Technology and Applications, P.Y. Yin (Editor), I-Tech, 2008.
   dfloat EllipseVariance() const;

   /// \brief Returns the convex hull of the polygon.
   dip::ConvexHull ConvexHull() const;
};

/// \brief A convex hull as a sequence of vertices (i.e. a closed polygon).
class ConvexHull {
   public:

      /// Default-constructed ConvexHull (without vertices)
      ConvexHull() {};

      /// Constructs a convex hull of a polygon
      ConvexHull( dip::Polygon const&& polygon );

      /// Retrive the vertices that represent the convex hull
      std::vector< VertexFloat > const& Vertices() const {
         return vertices_.vertices;
      }

      /// Returns the area of the convex hull
      dfloat Area() const {
         return vertices_.Area();
      }

      /// Returns the perimeter of the convex hull
      dfloat Perimeter() const {
         return vertices_.Length();
      }

      /// Returns the %Feret diameters of the convex hull
      FeretValues Feret() const;

      /// Returns the centroid of the convex hull
      VertexFloat Centroid() const {
         return vertices_.Centroid();
      }

      /// Returns statistics on the radii of the polygon, see `dip::Polygon::RadiusStatistics`.
      RadiusValues RadiusStatistics() const {
         return vertices_.RadiusStatistics();
      }

      /// \brief Returns the coeffient of variation of the distance of vertices to the ellipse with identical
      /// covarianace matrix, see `dip::Polygon::EllipseVariance`.
      dfloat EllipseVariance() const {
         return vertices_.EllipseVariance();
      }

      /// Returns the polygon representing the convex hull
      dip::Polygon const& Polygon() const {
          return vertices_;
      }

   private:
      dip::Polygon vertices_;
};

// This function cannot be written inside the dip::Polygon class because it needs to know about the dip::ConvexHull
// class, which in turn needs to know about the dip::Polygon class.
inline dip::ConvexHull Polygon::ConvexHull() const {
   Polygon copy = *this;
   return dip::ConvexHull( std::move( copy ));
}


//
// Chain code
//


/// \brief The contour of an object as a chain code sequence.
struct ChainCode {

   /// \brief Encodes a single chain code, as used by `dip::ChainCode`. Chain codes are between 0 and 3 for connectivity = 1,
   /// and between 0 and 7 for connectivity = 2. The border flag marks pixels at the border of the image.
   class Code {
      public:
         /// Default constructor
         Code() : value( 0 ) {}
         /// Constructor
         Code( int code, bool border = false ) { value = static_cast< dip::uint8 >( code & 7 ) | (( static_cast< dip::uint8 >( border )) << 3 ); }
         /// Returns whether the border flag is set
         bool IsBorder() const { return static_cast< bool >( value & 8 ); }
         /// Returns the chain code
         operator int() const { return value & 7; }
         /// Is it an even code?
         bool IsEven() const { return !(value & 1); }
         /// Is it an off code?
         bool IsOdd() const { return !IsEven(); }
         /// Compare codes
         bool operator==( Code const& c2 ) const {
            return ( value & 7 ) == ( c2.value & 7 );
         }
         /// Compare codes
         bool operator!=( Code const& c2 ) const {
            return !( *this == c2 );
         }
      private:
         dip::uint8 value;
   };

   std::vector< Code > codes;  ///< The chain codes
   VertexInteger start = { 0, 0 };  ///< The coordinates of the start pixel
   dip::uint objectID;              ///< The label of the object from which this chaincode is taken
   bool is8connected = true;        ///< Is false when connectivity = 1, true when connectivity = 2

   /// Adds a code to the end of the chain.
   void Push( Code const& code ) { codes.push_back( code ); }

   /// \brief Returns the length of the chain code using the method by Vossepoel and Smeulders. If the chain code
   /// represents the closed contour of an object, add pi to the result to determine the object's perimeter.
   dfloat Length() const;

   /// \brief Returns the %Feret diameters, using an angular step size in radian of `angleStep`.
   /// It is better to use `this->ConvexHull().Feret()`.
   FeretValues Feret( dfloat angleStep ) const;

   /// Computes the bending energy.
   dfloat BendingEnergy() const;

   /// \brief Computes the area of the solid object described by the chain code. Uses the result of
   /// `dip::ChainCode::Polygon`, so if you plan to do multiple similar measures, extract the polygon and
   /// compute the measures on that.
   dfloat Area() const {
      // There's another algorithm to compute this, that doesn't depend on the polygon. Should we implement that?
      return Polygon().Area() + 0.5;
   }

   /// \brief Computes the centroid of the solid object described by the chain code. Uses the result of
   /// `dip::ChainCode::Polygon`, so if you plan to do multiple similar measures, extract the polygon and
   /// compute the measures on that.
   VertexFloat Centroid() const {
      // There's another algorithm to compute this, that doesn't depend on the polygon. Should we implement that?
      return Polygon().Centroid();
   }

   /// Returns the length of the longest run of identical chain codes.
   dip::uint LongestRun() const;

   /// \brief Returns a polygon representation of the object.
   ///
   /// Creates a polygon by joining the mid-points between an object pixel and a background pixel that are
   /// edge-connected neighbors. The polygon follows the "crack" between pixels, but without the biases
   /// one gets when joining pixel vertices into a polygon. The polygon always has an area exactly half a
   /// pixel smaller than the binary object it represents.
   ///
   /// This idea comes from Steve Eddins:
   /// http://blogs.mathworks.com/steve/2011/10/04/binary-image-convex-hull-algorithm-notes/
   dip::Polygon Polygon() const;

   /// Returns the convex hull of the object, see `dip::ChainCode::Polygon`.
   dip::ConvexHull ConvexHull() const {
      return dip::ConvexHull( Polygon() );
   }
};

/// \brief A collection of object contours
using ChainCodeArray = std::vector< ChainCode >;

/// \brief Returns the set of chain codes sequences that encode the contours of the given objects in a labelled image.
/// Note that only the first closed contour for each label is found; if an object has multiple connected components,
/// only part of the object is found.
ChainCodeArray GetImageChainCodes(
      Image const& labels,
      UnsignedArray const& objectIDs,
      dip::uint connectivity = 2
);


/// \}

} // namespace dip

#endif // DIP_CHAIN_CODE_H
