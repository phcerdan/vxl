//-*- c++ -*-------------------------------------------------------------------
#ifdef __GNUC__
#pragma implementation "MovieFile.h"
#endif
//
// Class: MovieFile
// Author: Andrew W. Fitzgibbon, Oxford RRG
// Created: 26 Aug 98
// Modifications:
//   980826 AWF Initial version.
//
//-----------------------------------------------------------------------------

#include "MovieFile.h"

#include <vcl/vcl_vector.h>
#include <vbl/vbl_printf.h>

#include <oxp/SGIMovieFile.h>
#include <oxp/ImageSequenceMovieFile.h>

/////////////////////////////////////////////////////////////////////////////

// -- The moviefile constructor takes the given filename, and opens it at a movie
// file.  The possible values for filename are:
//   blah.mv                The name of an SGI movie file.
//   blah.###.jpg           The pattern for an image sequence.
//   blah                   Assumed to be the basename of a pattern such as
//                          blah.###.extension.   Various default extensions
//                          are searched for using blah.<start>.<ext> until
//                          one matches.
MovieFile::MovieFile(char const* filename, int start, int step, int end):
  filename_(filename),
  start_(start),
  step_(step),
  end_(end),
  qt(0),
  tmp_buf_(0)
{
#ifndef WIN32
  // Attempt to open for reading.  If it exists, assume it's a movie file.
  // not on windows, it will create it if it doesn't exist...
  ifstream fd(filename);
  if (fd.good()) 
    {
      qt = new SGIMovieFile(filename);
    }
  else
#endif
    {
      qt = new ImageSequenceMovieFile(filename, start_);
    }
}

// Destructor
MovieFile::~MovieFile()
{
  delete tmp_buf_;
  delete qt;
}

int MovieFile::index(int f)
{
  return start_ + f * step_;
}

int MovieFile::GetLength()
{
  return qt->GetLength();
}

int MovieFile::GetSizeX(int frame_index)
{ 
  return qt->GetSizeX(index(frame_index));
}

int MovieFile::GetSizeY(int frame_index)
{
  return qt->GetSizeY(index(frame_index));
}

int MovieFile::GetBitsPixel()
{
  return qt->GetBitsPixel();
}

bool MovieFile::HasFrame(int frame_index)
{
  return qt->HasFrame(index(frame_index));
}

void MovieFile::GetFrame(int frame_index, vil_memory_image_of<vil_rgb_byte >& frame)
{
  int sx = GetSizeX(frame_index);
  int sy = GetSizeY(frame_index);
  frame.resize(sx,sy);
  GetFrame(frame_index, (vil_rgb_byte*)frame.get_buffer());
}
  
void MovieFile::GetFrame(int frame_index, vil_memory_image_of<vil_byte>& frame)
{
  int sx = GetSizeX(frame_index);
  int sy = GetSizeY(frame_index);
  frame.resize(sx,sy);
  GetFrame(frame_index, (vil_byte*)frame.get_buffer());
}
  
void MovieFile::GetFrameRGB(int frame_index, vil_byte* frame)
{
  GetFrame(frame_index, (vil_rgb_byte *)frame);
}

void MovieFile::GetFrame(int frame_index, vil_rgb_byte * frame)
{
  if (qt->GetBitsPixel() == 24)
    qt->GetFrame(index(frame_index), frame);
  else {
    // Grab gray into first part and splay out into rgb
    qt->GetFrame(index(frame_index), frame);
    int sx = GetSizeX((frame_index));
    int sy = GetSizeY((frame_index));
    int size = sx * sy;
    vil_byte* base = (vil_byte*)frame;
    vil_byte* rgb_ptr = base + size*3;
    vil_byte* gray_ptr = base + size;
    do {
      --gray_ptr;
      *--rgb_ptr = *gray_ptr;
      *--rgb_ptr = *gray_ptr;
      *--rgb_ptr = *gray_ptr;
    } while (gray_ptr != base);
  }
}

void MovieFile::GetFrame(int frame_index, vil_byte* frame)
{
  if (qt->GetBitsPixel() == 8) {
    // Grab gray directly
    qt->GetFrame(index(frame_index), frame);
  } else {
    int sx = GetSizeX((frame_index));
    int sy = GetSizeY((frame_index));

    // Grab colour into temp buffer
    if (!tmp_buf_)
      tmp_buf_ = new vil_memory_image_of<vil_rgb_byte >(sx, sy);
    else 
      tmp_buf_->resize(sx, sy);
    
    qt->GetFrame(index(frame_index), (vil_byte*)tmp_buf_->get_buffer());
    int size = sx * sy;
    vil_rgb_byte * rgb_ptr = (vil_rgb_byte*)tmp_buf_->get_buffer();
    vil_byte* gray_ptr = frame;
    for(int i = 0; i < size; ++i)
      *gray_ptr++ = rgb_ptr++->grey();
  }
}
