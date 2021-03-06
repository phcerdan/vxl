#include <vector>
#include <set>
#include <iostream>
#include <fstream>
#include <string>
#include <cstdlib>
#include <bwm/bwm_observer_cam.h>
#include <bwm/video/bwm_video_cam_ostream.h>
#include <bwm/video/bwm_video_corr_sptr.h>
#include <bwm/video/bwm_video_corr.h>
#include <bwm/video/bwm_video_site_io.h>

#include <vcl_cassert.h>
#include <vul/vul_arg.h>
#include <vul/vul_file.h>
#include <vgl/algo/vgl_fit_plane_3d.h>
#include <vgl/vgl_distance.h>
#include <vgl/vgl_point_3d.h>
#include <vgl/vgl_vector_3d.h>
#include <vgl/vgl_box_3d.h>
#include <vgl/algo/vgl_orient_box_3d.h>
#include <vgl/algo/vgl_rotation_3d.h>
#include <vnl/vnl_double_3.h>
#include <vnl/vnl_matrix_fixed.h>
#include <vnl/vnl_quaternion.h>
#include <vnl/algo/vnl_cholesky.h>
#include <vnl/algo/vnl_svd_fixed.h>
#include <vpgl/vpgl_perspective_camera.h>
#include <vpgl/vpgl_calibration_matrix.h>
#include <vsph/vsph_camera_bounds.h>

#include <vidl/vidl_image_list_istream.h>
#include <vidl/vidl_convert.h>

#include <vil/vil_image_view.h>
#include <vil/vil_save.h>
#include <vcl_compiler.h>
#include <bwm/bwm_site_mgr.h>

#include <bxml/bxml_write.h>

static void write_vrml_header(std::ofstream& str)
{
  str << "#VRML V2.0 utf8\n"
      << "Background {\n"
      << "  skyColor [ 0 0 0 ]\n"
      << "  groundColor [ 0 0 0 ]\n"
      << "}\n";
}

static void
write_vrml_cameras(std::ofstream& str,std::vector<vpgl_perspective_camera<double> > & cams, double rad, std::set<int> const& bad_cams)
{
  str << "#VRML V2.0 utf8\n"
      << "Background {\n"
      << "  skyColor [ 0 0 0 ]\n"
      << "  groundColor [ 0 0 0 ]\n"
      << "}\n";
  int n = cams.size();
  for (int i =0; i<n; i++) {
    vgl_point_3d<double> cent =  cams[i].get_camera_center();
    str << "Transform {\n"
        << "translation " << cent.x() << ' ' << cent.y() << ' '
        << ' ' << cent.z() << '\n'
        << "children [\n"
        << "Shape {\n"
        << " appearance Appearance{\n"
        << "   material Material\n"
        << "    {\n";
    if ( bad_cams.count(i) )
        str << "      diffuseColor " << 1.0 << ' ' << 0.0 << ' ' << 0.0 << '\n';
    else
        str << "     diffuseColor " << 1.0 << ' ' << 1.0 << ' ' << 0.0 << '\n';
    str << "      transparency " << 0.0 << '\n'
        << "    }\n"
        << "  }\n"
        << " geometry Sphere\n"
        <<   "{\n"
        << "  radius " << rad << '\n'
        <<  "   }\n"
        <<  "  }\n"
        <<  " ]\n"
        << "}\n";
    vgl_vector_3d<double> r = cams[i].principal_axis();
    vnl_double_3 yaxis(0.0, 1.0, 0.0), pvec(r.x(), r.y(), r.z());
    vgl_rotation_3d<double> rot(yaxis, pvec);
    vnl_quaternion<double> q = rot.as_quaternion();

    vnl_double_3 axis = q.axis();
    double ang = q.angle();
    str <<  "Transform {\n"
        << " translation " << cent.x()+6*rad*r.x() << ' ' << cent.y()+6*rad*r.y()
        << ' ' << cent.z()+6*rad*r.z() << '\n'
        << " rotation " << axis[0] << ' ' << axis[1] << ' ' << axis[2] << ' ' <<  ang << '\n'
        << "children [\n"
        << " Shape {\n"
        << " appearance Appearance{\n"
        << "  material Material\n"
        << "   {\n"
        << "     diffuseColor 1 0 0\n"
        << "     transparency 0\n"
        << "    }\n"
        << "  }\n"
        << " geometry Cylinder\n"
        << "{\n"
        << " radius "<<rad/3<<'\n'
        << " height " << 12*rad << '\n'
        << " }\n"
        << " }\n"
        << "]\n"
        << "}\n";
  }
}

static void write_vrml_points(std::ofstream& str,
                              std::vector<vgl_point_3d<double> > const& pts3d, double rad=2.0)
{
  int n = pts3d.size();
  str<<"Shape {\n"
     <<"geometry PointSet {\n"
     <<"coord Coordinate {\n"
     <<"point [\n";
  for (int i =0; i<n; i++)
    str<<pts3d[i].x()<<' '<<pts3d[i].y()<<' '<< pts3d[i].z() <<",\n";

  str<<"]\n"
     <<"}\n"
     <<"color Color {\n"
     <<"color [\n";
  for (int i =0; i<n; i++)
    str<<0<<' '<<1.0<<' '<< 0.0 <<",\n";
  str<<"]\n"
     <<"}\n"
     <<"}\n"
     <<"}\n";
}

static void write_vrml_box(std::ofstream& os,
                           vgl_box_3d<double> const& bounding_box,
                           vnl_vector_fixed<double,3> const& color, float const& transparency)
{
    os << "Transform {\n"
       << "  translation " << bounding_box.centroid_x() << ' ' << bounding_box.centroid_y() << ' ' << bounding_box.centroid_z() << '\n'
       << "     children[\n"
       << "     Shape {\n"
       << "         appearance Appearance{\n"
       << "                         material Material{\n"
       << "                                      diffuseColor " << color[0] << ' ' << color[1] << ' ' << color[2] << '\n'
       << "                                      transparency " << transparency << '\n'
       << "                                          }\n" //end material
       << "                     }\n" //end appearance
       << "         geometry Box {\n"
       << "         size " << bounding_box.width() << ' ' << bounding_box.height() << ' ' << bounding_box.depth() << '\n'
       << "         }\n" //end box
       << "     }\n" //end Shape
       << "     ]\n" //end children
       << "}\n"; //end Transform
}

bool fit_plane_ransac(std::vector<vgl_homg_point_3d<double> > & points, vgl_homg_plane_3d<double>  & plane)
{
  unsigned int nchoose=3;
  unsigned int nsize=points.size();
  unsigned int max_its = 500;
  double err=10.0;
  double inlier_dist = 0.01;
  std::vector<int> best_inliers;
  for (unsigned i=0;i<max_its;++i)
  {
    std::cout << '.';
    std::vector<vgl_homg_point_3d<double> > subset;
    std::vector<int> inliers;
    for (unsigned j=0;j<nchoose;++j)
      subset.push_back(points[std::rand()%nsize]);
    std::cout<<subset.size();std::cout.flush();
    vgl_fit_plane_3d<double> fit_plane(subset);
    if (fit_plane.fit(err, &std::cerr))
    {
      vgl_homg_plane_3d<double> plane=fit_plane.get_plane();
      for (unsigned j=0;j<nsize;++j)
      {
        double dist=vgl_distance<double>(points[j],plane);
        if (dist*dist<inlier_dist*inlier_dist)
        {
          inliers.push_back(j);
        }
      }
      if (inliers.size()>best_inliers.size())
        best_inliers=inliers;
    }
  }
  vgl_fit_plane_3d<double> fit_plane_inliers;

  for (unsigned i=0;i<best_inliers.size();++i)
  {
    fit_plane_inliers.add_point(points[best_inliers[i]]);
  }
  std::cout<<"Inliers "<<best_inliers.size()<<std::endl;
  if (fit_plane_inliers.fit(23.0, &std::cerr))
  {
    plane=fit_plane_inliers.get_plane();
    return true;
  }
  else
    return false;
}

bool axis_align_scene(std::vector<bwm_video_corr_sptr> & corrs,
                      std::vector<vpgl_perspective_camera<double> > & cams)
{
  std::vector<vgl_homg_point_3d<double> > points;
  for (unsigned i=0;i<corrs.size();++i)
  {
    vgl_homg_point_3d<double> homg_world_pt(corrs[i]->world_pt());
    points.push_back(homg_world_pt);
  }

  // fit the plane
  vgl_fit_plane_3d<double> fit_plane(points);
  if (!fit_plane.fit(1e6, &std::cerr))
    return false;

  vgl_homg_plane_3d<double> plane=fit_plane.get_plane();
  vgl_rotation_3d<double> rot_scene(plane.normal(),vgl_vector_3d<double>(0,0,1));

  double sumx=0,sumy=0,sumz=0;
  for (unsigned i=0;i<corrs.size();++i)
  {
    vgl_homg_point_3d<double> p(corrs[i]->world_pt());
    vgl_homg_point_3d<double> pnew=rot_scene*p;
    if (!pnew.ideal())
    {
      vgl_point_3d<double> pnew_nonhomg(pnew.x()/pnew.w(),pnew.y()/pnew.w(),pnew.z()/pnew.w());
      sumx+=pnew_nonhomg.x();sumy+=pnew_nonhomg.y();sumz+=pnew_nonhomg.z();
      corrs[i]->set_world_pt(pnew_nonhomg);
    }
  }
  vgl_point_3d<double> center(sumx/corrs.size(),sumy/corrs.size(),sumz/corrs.size());
  vnl_vector_fixed<double,3> tr(center.x(),center.y(),center.z());
  std::vector<vgl_homg_point_3d<double> > xformed_points;
  for (unsigned i=0;i<corrs.size();++i)
  {
    vgl_point_3d<double> p(corrs[i]->world_pt());
    corrs[i]->set_world_pt(vgl_point_3d<double>(p.x()-center.x(),p.y()-center.y(),p.z()-center.z()));
    xformed_points.push_back(vgl_homg_point_3d<double>(p.x()-center.x(),p.y()-center.y(),p.z()-center.z()));
  }
  std::vector<vpgl_perspective_camera<double> > new_cams;
  unsigned int up=0;
  for (unsigned i=0;i<cams.size();++i)
  {
    // new rotation
    vgl_rotation_3d<double> rot_cami=cams[i].get_rotation()*rot_scene.inverse();

    // new translation
    vnl_vector_fixed<double,3> newtranslation=rot_cami.as_matrix()*tr;
    vgl_vector_3d<double> translation_vec(newtranslation[0],newtranslation[1],newtranslation[2]);
    vgl_vector_3d<double> tr_cami=cams[i].get_translation()+translation_vec;

    new_cams.push_back(vpgl_perspective_camera<double>(cams[i].get_calibration(),rot_cami,tr_cami));
    if (new_cams[i].get_camera_center().z()>0)
      ++up;
  }

  if (2*up<cams.size())// flip the world
  {
    vnl_quaternion<double> q(vnl_math::pi,0,0);
    vgl_rotation_3d<double> rotx_pi(q);
    for (unsigned i=0;i<corrs.size();++i)
    {
      vgl_homg_point_3d<double> p(corrs[i]->world_pt());
      vgl_homg_point_3d<double> pnew=rotx_pi*p;
      if (!pnew.ideal())
      {
        vgl_point_3d<double> pnew_nonhomg(pnew.x()/pnew.w(),pnew.y()/pnew.w(),pnew.z()/pnew.w());
        corrs[i]->set_world_pt(pnew_nonhomg);
      }
    }
    for (unsigned i=0;i<cams.size();++i)
    {
      // new rotation
      vgl_rotation_3d<double> rot_cami=new_cams[i].get_rotation()*rotx_pi.inverse();
      new_cams[i]=vpgl_perspective_camera<double>(new_cams[i].get_calibration(),rot_cami,new_cams[i].get_translation());
    }
  }
  for (unsigned i=0;i<cams.size();++i)
    cams[i]=new_cams[i];

  vgl_fit_plane_3d<double> fit_plane1(xformed_points);
  if (!fit_plane1.fit(1e6, &std::cerr))
      return false;

  vgl_homg_plane_3d<double> plane1=fit_plane1.get_plane();
  // TODO: do something with this plane1 !!!
  return true;
}

vnl_vector_fixed<double,3> stddev( std::vector<vgl_point_3d<double> > const& v)
{
  unsigned n = v.size();
  assert(n>0);
  vnl_vector_fixed<double,3> m(0.0f), stddev(0.0f);

  for (unsigned i = 0; i < n; ++i)
  {
    m[0]+=v[i].x();
    m[1]+=v[i].y();
    m[2]+=v[i].z();
  }
  for (unsigned i = 0; i < 3; ++i)
    m[i]/=n;

  for (unsigned i = 0; i < n; ++i)
  {
    stddev[0] += (v[i].x()-m[0])*(v[i].x()-m[0]);
    stddev[1] += (v[i].y()-m[1])*(v[i].y()-m[1]);
    stddev[2] += (v[i].z()-m[2])*(v[i].z()-m[2]);
  }

  for (unsigned i = 0; i < 3; ++i)
    stddev[i] = std::sqrt(stddev[i]/(n-1));

  return stddev;
}

// An executable that read bundler file and convert it into video site.
int main(int argc, char** argv)
{
  //Get Inputs

  vul_arg<std::string> bundlerfile   ("-bundler", "Output file of bundler",  "");
  vul_arg<std::string> cam_dir       ("-cam_dir",      "directory to store cams", "");
  vul_arg<std::string> img_dir       ("-img_dir",     "list of images filenames", "");
  vul_arg<std::string> site_name     ("-site_name",    "Name of the site", "");
  vul_arg<std::string> site_directory("-site_dir", "Directory for the site", "");
  vul_arg<std::string> cam_txt_dir   ("-cam_txt_dir",      "directory to store txt cams", "");
  vul_arg<std::string> vrml_file     ("-vrml_file",      "vrml file", "");
  vul_arg<std::string> xml_file      ("-xml_file",      "xml file", "");
  vul_arg<bool>       draw_box      ("-draw_box", "Draw Bounding Box around points within 2*(standard deviation) from the center of scene",true);
  vul_arg<bool>       filter        ("-filter_cams", "Filter camera based on Reprojection error of 3d correspondences", false);
  vul_arg<float>      filter_thresh ("-filter_thresh", "Threshold for average rms value for a given view. Units are pixels", .75);

  vul_arg_parse(argc, argv);

  // open the bundler file
  std::ifstream bfile( bundlerfile().c_str() );
  if (!bfile)
  {
    std::cout<<"Error Opening Bundler output file"<<std::endl;
    return -1;
  }

  // verify image dir
  if (!vul_file::is_directory(img_dir().c_str()))
  {
    std::cout<<"Image directory does not exist"<<std::endl;
    return -1;
  }

  vidl_image_list_istream imgstream(img_dir()+"/*");
  //vidl_image_list_istream imgstream(img_dir()+"/*.jpg");

  if (!imgstream.is_open())
  {
    std::cout<<"Invalid image stream"<<std::endl;
    return -1;
  }

  // get image size
  unsigned ni=imgstream.width();
  unsigned nj=imgstream.height();

  // central point of the image
  vgl_point_2d<double> principal_point((double)ni/2,(double)nj/2);

  char buffer[1024];
  bfile.getline(buffer,1024); // read the header line

  if (bfile.eof())
  {
    std::cout<<"File Missing data"<<std::endl;
    return -1;
  }

  unsigned num_cams=0, num_pts=0;
  bfile>>num_cams>>num_pts; // reading number of cameras and number of 3-d pts

  // read the cams from bundler and write it to a directory
  std::vector<vpgl_perspective_camera<double> > cams;
  std::set<int> bad_cams;

  // reading the cameras from bundler
  for (unsigned i=0;i<num_cams;++i)
  {
    double f,k1,k2;
    vnl_matrix_fixed<double,3,3> R;
    vnl_vector_fixed<double,3> T;

    bfile>>f>>k1>>k2;
    bfile>>R>>T;

    // negating to convert bundlers camera facing towards -ve z to positive
    R(2,0) = -R(2,0); R(2,1) = -R(2,1);R(2,2) = -R(2,2);
    //R(1,0) = -R(1,0);R(1,1) = -R(1,1);R(1,2) = -R(1,2);
    vgl_rotation_3d<double> rot(R);

    // negating to convert bundlers camera facing towards -ve z to positive
    //vgl_vector_3d<double> t(T(0),-T(1),-T(2));
    vgl_vector_3d<double> t(T(0),T(1),-T(2));

    if (f>0.0)
    {
      vpgl_calibration_matrix<double> K(f,principal_point) ;
      vpgl_perspective_camera<double> cam(K,rot,t);
      cams.push_back(cam);
    }
    else
    {
      vpgl_perspective_camera<double> cam;
      cams.push_back(cam);
      bad_cams.insert(i);
    }
  }

  std::map<unsigned,double> view_error_map;
  std::map<unsigned,unsigned> view_count_map;
  std::vector<bwm_video_corr_sptr> corrs;
  //  Projection error
  double err=0;double cnt=0;
  double maxerr=0;
  int maxerrcam=-1;
  // read the correspondence and 3-d points
  for (unsigned i=0;i<num_pts;++i)
  {
    bwm_video_corr_sptr  corr=new bwm_video_corr();
    double x,y,z;

    // read the 3-d point
    bfile>>x>>y>>z;

    corr->set_world_pt(vgl_point_3d<double>(x,y,z));

    vgl_homg_point_3d<double> homg_world_pt(corr->world_pt());

    int r,g,b;
    // read the intenstiy but don't do anything with it right now.
    bfile>>r>>g>>b;

    unsigned num_views;
    bfile>>num_views;

    for (unsigned j=0;j<num_views;++j)
    {
      unsigned view_number, key_number;
      double img_x,img_y;

      bfile>>view_number>>key_number>>img_x>>img_y;

      img_x=img_x+principal_point.x();
      img_y=(img_y+principal_point.y());
      double u=0,v=0;

      //project point onto image plane
      cams[view_number].project(x,y,z,u,v);
      vgl_ray_3d<double> ray = cams[view_number].backproject_ray(u,v);
      vgl_vector_3d<double> point_dir = vgl_homg_point_3d<double>(x,y,z) - cams[view_number].camera_center();
      double dp = dot_product(point_dir, ray.direction());
      std::cout<<"dot product: "<<dp<<std::endl;
      if (dp < 0.0) {
        bad_cams.insert(view_number);
      }
#if 0
      if (cams[view_number].is_behind_camera(homg_world_pt))
        bad_cams.insert(view_number);
#endif
      else
      {
        cams[view_number].project(x,y,z,u,v);
        double rms=std::sqrt((u-img_x)*(u-img_x)+(v-img_y)*(v-img_y));
        //std::cout<<view_number<<":: U: "<<u<<", X: "<<img_x<<" ... V: "<<v<<", Y: "<<img_y<<std::endl;
        err+=rms;
        ++cnt;

        std::map<unsigned,double>::iterator ve_itr = view_error_map.find(view_number);
        if (ve_itr == view_error_map.end())
        {
          view_error_map[view_number]=rms;
          view_count_map[view_number]=1;
        }
        else
        {
          view_error_map[view_number]+=rms;
          view_count_map[view_number]+=1;
        }
      }
      corr->add(view_number,vgl_point_2d<double>(img_x,img_y));
    }
    corrs.push_back(corr);
  }
  std::cout<<"Avg Error per projection "<<err/cnt<<'\n'
          <<"Max Error"<<maxerr<<" is in camera # "<<maxerrcam<<std::endl;

  // save it to a site
  if (!axis_align_scene(corrs,cams))
    return -1;
  bwm_video_cam_ostream camstream(cam_dir());
  for (unsigned i=0;i<num_cams;++i)
    camstream.write_camera(&cams[i]);
  camstream.close();

  char filename[1024];
  if (vul_file::is_directory(cam_txt_dir().c_str()))
  {
    for (unsigned i=0;i<num_cams;++i)
    {
      std::sprintf(filename,"%s/camera%05d.txt",cam_txt_dir().c_str(),i);
      std::ofstream ofile(filename);
      double u1,v1;
      cams[i].project(0,0,0,u1,v1);
      if (ofile)
      {
        ofile<<cams[i].get_calibration().get_matrix()<<'\n'
             <<cams[i].get_rotation().as_matrix()<<'\n'
             <<cams[i].get_translation().x()<<' '<<cams[i].get_translation().y()<<' '<<cams[i].get_translation().z()<<'\n';
      }
    }
  }

  if ( filter() )
  {
    std::map<unsigned,double>::iterator ve_itr = view_error_map.begin(), ve_end = view_error_map.end();
    for (;ve_itr!=ve_end;++ve_itr)
    {
      ve_itr->second/=view_count_map[ve_itr->first];
      if (ve_itr->second>filter_thresh())
        bad_cams.insert(ve_itr->first);
    }

    //save reprojection rms to a file
    std::string rms_file=site_directory()+"/rms_error.txt";
    std::ofstream ofstr(rms_file.c_str());
    if (ofstr)
    {
      std::map<unsigned,double>::iterator ve_itr = view_error_map.begin(), ve_end = view_error_map.end();
      for (; ve_itr != ve_end; ++ve_itr)
        ofstr << "View: " << ve_itr->first << "\t RMS Error: " << ve_itr->second << std::endl;
    }
    else
        std::cout << "ERROR SAVING RMS FILE!" << std::endl;
    ofstr.close();

    std::string img_dir = site_directory()+"/frames";
    if ( !vul_file::is_directory(img_dir) ) { vul_file::make_directory(img_dir); }
    unsigned cnt=0;
    for (unsigned i = 0; i < num_cams; ++i)
    {
      if ( !bad_cams.count(i) )
      {
        vil_image_view<vxl_byte> curr_img;
        imgstream.seek_frame(i);
        vidl_convert_to_view(*imgstream.current_frame(),curr_img);
        char filename[1024];

        std::sprintf(filename,"%s/frame%05d.png",img_dir.c_str(),cnt);
        vil_save(curr_img,filename);
        ++cnt;
      }
    }//end frame iteration


    std::string cam_dir = site_directory() + "/cameras_KRT";
    if ( !vul_file::is_directory(cam_dir) )
    {
      vul_file::make_directory(cam_dir);
    }
    else
      vul_file::delete_file_glob(std::string(cam_dir+"/*.txt"));
    unsigned cnt2 = 0;
    std::vector<vpgl_perspective_camera<double> > cams2 = cams;

    for (unsigned i = 0; i < num_cams; ++i)
    {
      if ( !bad_cams.count(i) )
      {
        char filename[1024];
        std::sprintf(filename,"%s/camera%05d.txt",cam_dir.c_str(),cnt2);
        std::ofstream ofile(filename);
        double u1,v1;
        cams2[i].project(0,0,0,u1,v1);
        if (ofile)
        {
          ofile<<cams2[i].get_calibration().get_matrix()<<'\n'
               <<cams2[i].get_rotation().as_matrix()<<'\n'
               <<cams2[i].get_translation().x()<<' '
               <<cams2[i].get_translation().y()<<' '
               <<cams2[i].get_translation().z()<<'\n';
        }
        ++cnt2;
      }
    }//end camera iteration
  }

  std::vector<vgl_point_3d<double> > pts_3d;
  vgl_box_3d<double> bounding_box;
  for (unsigned i=0;i<corrs.size();++i)
  {
    bounding_box.add(corrs[i]->world_pt());
    pts_3d.push_back(corrs[i]->world_pt());
  }
  // Dimensions of the World
  std::cout<<"Bounding Box "<<bounding_box<<std::endl;
  vgl_point_3d<double> c = centre(pts_3d);
  std::cout<<"Center       "<<c<<std::endl;
  vnl_vector_fixed<double,3> sigma = stddev(pts_3d);
  std::cout<<"Stddev       "<< sigma <<std::endl;

  // Define dimensions to be used for a boxm scene
  // Note: x-y dimensions are kind of a good approximation
  // the z-dimension however suffers because most points tend to be on the ground and the average miss represents points off the gound
  double minx=-3.0f*sigma[0], miny=-3.0f*sigma[1], minz=-1.0f*sigma[2];
  double maxx=3.0f*sigma[0], maxy=3.0f*sigma[1], maxz=5.0f*sigma[2];

  vgl_box_3d<double> bounding_box2(minx, miny, minz, maxx, maxy,maxz);

  //write bounding boxm to xml file
  bxml_document doc;
  bxml_element *root = new bxml_element("bwm_info_for_boxm2");
  doc.set_root_element(root);
  root->append_text("\n");

  bxml_element* bbox_elm = new bxml_element("bbox");
  bbox_elm->append_text("\n");
  bbox_elm->set_attribute("minx", minx );
  bbox_elm->set_attribute("miny", miny);
  bbox_elm->set_attribute("minz", minz );
  bbox_elm->set_attribute("maxx", maxx );
  bbox_elm->set_attribute("maxy", maxy );
  bbox_elm->set_attribute("maxz", maxz );
  root->append_data(bbox_elm);
  root->append_text("\n");

  std::cout << "Bounding Box containing points which are [-3,3]sigma about x and y and [-1,5]-z_sigma about the scene center: " <<bounding_box2<<'\n'
           << "min_x = " << bounding_box2.min_x() << '\n'
           << "min_y = " << bounding_box2.min_y() << '\n'
           << "min_z = " << bounding_box2.min_z() << '\n'
           << "max_x = " << bounding_box2.max_x() << '\n'
           << "max_y = " << bounding_box2.max_y() << '\n'
           << "max_z = " << bounding_box2.max_z() << '\n'
           << "width = " << bounding_box2.width() << '\n'
           << "depth = " << bounding_box2.depth() << '\n'
           << "height= " << bounding_box2.height() << std::endl;

  // Determining the resolution of the cells
  vgl_ray_3d<double> cone_axis;
  double cone_half_angle;
  double solid_angle;

  vsph_camera_bounds::pixel_solid_angle(cams[0],ni/4,nj/4,cone_axis,cone_half_angle,solid_angle);

  vgl_point_3d<double> cc=cams[0].camera_center();
  double res=2*(cc-centre(pts_3d)).length()*cone_half_angle;

  std::cout<<"Resolution     "<<res<<std::endl;

  //write resolution to xml
  bxml_element* res_elm = new bxml_element("resolution");
  res_elm->append_text("\n");
  res_elm->set_attribute("val", res);
  root->append_data(res_elm);
  root->append_text("\n");

  //write to disk
  std::ofstream xml_os(xml_file().c_str());
  bxml_write(xml_os, doc);
  xml_os.close();

  std::ofstream os(vrml_file().c_str());
  if (os)
  {
    write_vrml_header(os);
    write_vrml_points(os,pts_3d,res);
    write_vrml_cameras(os,cams,res*10,bad_cams);
    if (draw_box())
      write_vrml_box(os,bounding_box2,vnl_vector_fixed<double,3>(0.0,0.0,1.0),0.6f);
  else
    std::cout<<"ERROR OPENING  "<< vrml_file() <<std::endl;
  }

  bwm_video_site_io site;
  site.set_name(site_name());
  site.set_corrs(corrs);
  site.set_site_directory(site_directory());
  site.set_video_path(img_dir()+"/*");
  site.set_camera_path(cam_dir()+"/*");

  std::string xml_filename=site_directory()+"/"+site_name()+".xml";
  site.x_write(xml_filename.c_str());

  std::cout<<"Bad cameras "<<bad_cams.size()<<" :";
  std::set<int>::iterator iter=bad_cams.begin();
  for (;iter!=bad_cams.end();++iter)
    std::cout<<*iter<<' ';
}
