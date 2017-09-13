#undef DIP__ENABLE_DOCTEST

#include <diplib.h>
#include <diplib/file_io.h>
#include <diplib/generation.h>

#include <diplib/viewer/glut.h>
#include <diplib/viewer/glfw.h>
#include <diplib/viewer/image.h>
#include <diplib/viewer/slice.h>

int main() {
#ifdef DIP__HAS_GLFW
   GLFWManager manager;
#else
   GLUTManager manager;
#endif

   dip::Image image3 = dip::ImageReadICS( "../test/chromo3d.ics" );
   manager.createWindow( WindowPtr( new SliceViewer( image3, "chromo3d" )));

   dip::Image image2{ dip::UnsignedArray{ 50, 40 }, 3, dip::DT_UINT8 };
   dip::Image tmp = image2[ 0 ];
   dip::FillXCoordinate( tmp, { "corner" } );
   tmp = image2[ 1 ];
   dip::FillYCoordinate( tmp, { "corner" } );
   tmp = image2[ 2 ];
   dip::FillRadiusCoordinate( tmp );
   image2 *= 5;
   manager.createWindow( WindowPtr( new ImageViewer( image2 )));

   while( manager.activeWindows()) {
      // Only necessary for GLFW
      manager.processEvents();
      usleep( 10 );
   }

   return 0;
}
