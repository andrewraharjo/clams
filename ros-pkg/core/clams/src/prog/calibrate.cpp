#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <clams/slam_calibrator.h>

using namespace std;
using namespace Eigen;
using namespace clams;

int main(int argc, char** argv)
{
  namespace bpo = boost::program_options;
  namespace bfs = boost::filesystem;
  bpo::options_description opts_desc("Allowed options");
  bpo::positional_options_description p;

  string workspace;
  opts_desc.add_options()
    ("help,h", "produce help message")
    ("workspace", bpo::value(&workspace)->required(), "CLAMS workspace.")
    ;

  p.add("workspace", 1);
  
  bpo::variables_map opts;
  bool badargs = false;
  try {
    bpo::store(bpo::command_line_parser(argc, argv).options(opts_desc).positional(p).run(), opts);
    bpo::notify(opts);
  }
  catch(...) { badargs = true; }
  if(opts.count("help") || badargs) {
    cout << "Usage: " << bfs::basename(argv[0]) << " [ OPTS ] CLAMS_WORKSPACE " << endl;
    cout << "  This program will calibrate using all slam results in CLAMS_WORKSPACE/." << endl;
    cout << endl;
    cout << opts_desc << endl;
    return 1;
  }

  // -- Check for existence of CLAMS_WORKSPACE/slam_results.
  string sequences_path = workspace + "/sequences";
  string results_path = workspace + "/slam_results";
  ROS_ASSERT(bfs::exists(results_path));

  // -- Get names of sequences that have corresponding results.
  vector<string> sseq_names;
  bfs::directory_iterator it(results_path), eod;
  BOOST_FOREACH(const bfs::path& p, make_pair(it, eod)) {
    string path = results_path + "/" + p.leaf().string();
    if(bfs::is_directory(path))
      sseq_names.push_back(p.leaf().string());
  }
  sort(sseq_names.begin(), sseq_names.end());

  // -- Construct sseqs with corresponding trajectories.
  vector<StreamSequenceBase::ConstPtr> sseqs;
  vector<Trajectory> trajs;
  for(size_t i = 0; i < sseq_names.size(); ++i) { 
    string sseq_path = sequences_path + "/" + sseq_names[i];
    string traj_path = results_path + "/" + sseq_names[i] + "/trajectory";

    cout << "Log " << i << endl;
    cout << "  StreamSequence:" << sseq_path << endl;
    cout << "  Trajectory: " << traj_path << endl;

    sseqs.push_back(StreamSequenceBase::initializeFromDirectory(sseq_path));
    Trajectory traj;
    traj.load(traj_path);
    trajs.push_back(traj);
  }

  // -- Run the calibrator.
  SlamCalibrator::Ptr calibrator(new SlamCalibrator(sseqs[0]->proj_));
  cout << "Using " << calibrator->max_range_ << " as max range." << endl;
  calibrator->trajectories_ = trajs;
  calibrator->sseqs_ = sseqs;
  
  DiscreteDepthDistortionModel model = calibrator->calibrate();
  string output_path = workspace + "/distortion_model";
  model.save(output_path);
  cout << "Saved distortion model to " << output_path << endl;

  string vis_dir = output_path + "-visualization";
  model.visualize(vis_dir);
  cout << "Saved visualization of distortion model to " << vis_dir << endl;
    
  return 0;
}
