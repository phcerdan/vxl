// <begin copyright notice>
// ---------------------------------------------------------------------------
//
//                   Copyright (c) 1997 TargetJr Consortium
//               GE Corporate Research and Development (GE CRD)
//                             1 Research Circle
//                            Niskayuna, NY 12309
//                            All Rights Reserved
//              Reproduction rights limited as described below.
//                               
//      Permission to use, copy, modify, distribute, and sell this software
//      and its documentation for any purpose is hereby granted without fee,
//      provided that (i) the above copyright notice and this permission
//      notice appear in all copies of the software and related documentation,
//      (ii) the name TargetJr Consortium (represented by GE CRD), may not be
//      used in any advertising or publicity relating to the software without
//      the specific, prior written permission of GE CRD, and (iii) any
//      modifications are clearly marked and summarized in a change history
//      log.
//       
//      THE SOFTWARE IS PROVIDED "AS IS" AND WITHOUT WARRANTY OF ANY KIND,
//      EXPRESS, IMPLIED OR OTHERWISE, INCLUDING WITHOUT LIMITATION, ANY
//      WARRANTY OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
//      IN NO EVENT SHALL THE TARGETJR CONSORTIUM BE LIABLE FOR ANY SPECIAL,
//      INCIDENTAL, INDIRECT OR CONSEQUENTIAL DAMAGES OF ANY KIND OR ANY
//      DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
//      WHETHER OR NOT ADVISED OF THE POSSIBILITY OF SUCH DAMAGES, OR ON
//      ANY THEORY OF LIABILITY ARISING OUT OF OR IN CONNECTION WITH THE
//      USE OR PERFORMANCE OF THIS SOFTWARE.
//
// ---------------------------------------------------------------------------
// <end copyright notice>
#ifdef __GNUG__
#pragma implementation
#endif
//--------------------------------------------------------------
//
// Class : HomgPoint2D
//
// Modifications : see HomgPoint2D.h
//
//-----------------------------------------------------------------------------

#include <vcl/vcl_iostream.h>
////////////////#include <Geometry/IUPoint.h>
#include <vnl/vnl_math.h>
#include <vnl/vnl_double_2.h>
#include <mvl/HomgPoint2D.h>

//--------------------------------------------------------------
//
// -- @{ Return true if the point is at infinity (an ideal point).
// The method checks that $|w| < \mbox{infinitesimal\_tol} \times \mbox{min}\{|x|, |y|\}$ @}
bool
HomgPoint2D::check_infinity(double tol) const
{
  return vnl_math_abs(get_w()) < tol * vnl_math_min(vnl_math_abs(get_x()), vnl_math_abs(get_y()));
}

//--------------------------------------------------------------
//
// -- Return the non-homogeneous coordinates of the point.
// If the point is at infinity, return false.
bool
HomgPoint2D::get_nonhomogeneous(double& ex, double& ey) const
{
  double x = get_x();
  double y = get_y();
  double z = get_w();

  if (z == 0)
    return false;

  // double scale = 1.0 / z;
  ex = x / z;
  ey = y / z;
  return true;
}

//-----------------------------------------------------------------------------
//
// -- returns a non-homogeneous coolvector of length 2.
vnl_double_2 HomgPoint2D::get_double2() const 
{
  double scale = 1.0 / (*this)[2];
  return vnl_double_2((*this)[0] * scale, (*this)[1] * scale);
}

//-----------------------------------------------------------------------------
//
// -- returns a unit-norm scaled copy of this.
HomgPoint2D HomgPoint2D::get_unitized() const
{
  double x = get_x();
  double y = get_y();
  double z = get_w();

  double norm = sqrt (vnl_math_sqr(x) + vnl_math_sqr(y) + vnl_math_sqr(z));
  
  if (norm == 0.0) {
    cerr << "HomgPoint2D::get_unitized() -- Zero length vector\n";
    return *this;
  }
  
  norm = 1.0/norm;
  return HomgPoint2D(x*norm, y*norm, z*norm);
}

//-----------------------------------------------------------------------------
//
// -- Print to ostream as <HomgPoint2D x y z>
ostream& operator<<(ostream& s, const HomgPoint2D& p)
{
  return s << "<HomgPoint2D " << p.get_vector() << ">";
}

//-----------------------------------------------------------------------------
//
// -- Read from ASCII istream.  Assumes points are stored in homogeneous form as 3 reals.
istream& operator>>(istream& is, HomgPoint2D& p)
{
  double x, y, z;
  is >> x >> y >> z;
  if (is.good())
    p.set(x,y,z);
  return is;
}

//-----------------------------------------------------------------------------
//
// -- read from istream.  Default is to assume that points are nonhomogeneous
// 2D, set is_homogeneous to true if points are already in homogeneous form.
HomgPoint2D HomgPoint2D::read(istream& is, bool is_homogeneous)
{
  if (is_homogeneous) {
    HomgPoint2D ret;
    is >> ret;
    return ret;
  }
  double x, y;
  is >> x >> y;
  return HomgPoint2D(x, y, 1.0);
}
