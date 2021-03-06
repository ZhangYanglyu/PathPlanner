#include <PathPlanner.h>
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <memory>
#include <docopt/docopt.h>

using PathPlanner = yaqwsx::PathPlanner<int>;

static const char usage[] =
R"(PathPlanner demo.

    Usage:
      pathplanner <trajectory> <output_dir> [options]
      pathplanner (-h | --help)
      pathplanner --version

    Options:
      -h --help               Show this screen.
      --envir=<file>          Draws environment as XY plot
      --path_alpha=<coef>     Weight of control control_points
      --path_beta=<coef>      Weight of smoothness
      --speed_alpha=<coef>    Weight of control control_points
      --speed_beta=<coef>     Weight of smoothness
      --robot_width=<width>   Robot width 
      --time_step=<step>      Time step
      --max_speed=<x>         Max speed
      --max_acceleration=<x>  Max acceleration
      --dist_step=<x>         Smallest distance step
      --speed_step_mult=<n>   Each n-th step speed limits are calculated
      --final_acc=<x>         Soft start and stop take x seconds
      --smooth_pass=<n>       n smooth passes are applied to trajectory
)";

using Path = PathPlanner::Path;

void gnuplot_output(std::ostream& o, const Path& p) {
    for (const auto& x : p)
        o << x.x << "\t" << x.y << "\n";
}

Path load_trajectory(std::istream& i) {
    Path res;
    double x, y;
    while (i >> x >> y) {
        res.push_back(PathPlanner::Position::make_tagged(0, x, y));
    }
    return res;
}

PathPlanner::Params get_params(std::map<std::string, docopt::value>& args) {
    PathPlanner::Params params;
    try {
        auto path_alpha = args["--path_alpha"];
        if (path_alpha.isString())
            params.path_alpha = std::stod(path_alpha.asString());

        auto path_beta = args["--path_beta"];
        if (path_beta.isString())
            params.path_beta = std::stod(path_beta.asString());

        auto speed_alpha = args["--speed_alpha"];
        if (speed_alpha.isString())
            params.speed_alpha = std::stod(speed_alpha.asString());

        auto speed_beta = args["--speed_beta"];
        if (speed_beta.isString())
            params.speed_beta = std::stod(speed_beta.asString());

        auto robot_width = args["--robot_width"];
        if (robot_width.isString())
            params.robot_width = std::stod(robot_width.asString());

        auto time_step = args["--time_step"];
        if (time_step.isString())
            params.time_step = std::stod(time_step.asString());

        auto max_speed = args["--max_speed"];
        if (max_speed.isString())
            params.max_speed = std::stod(max_speed.asString());

        auto max_acceleration = args["--max_acceleration"];
        if (max_acceleration.isString())
            params.max_acceleration = std::stod(max_acceleration.asString());

        auto dist_step = args["--dist_step"];
        if (dist_step.isString())
            params.dist_step = std::stod(dist_step.asString());

        auto speed_step_mul = args["--speed_step_mult"];
        if (speed_step_mul.isString())
            params.speed_step_mult = std::stod(speed_step_mul.asString());

        auto final_acc = args["--final_acc"];
        if (final_acc.isString())
            params.final_acc_time = std::stod(final_acc.asString());

        auto smooth_pass = args["--smooth_pass"];
        if (smooth_pass.isString())
            params.traj_smooth_pass = std::stoi(smooth_pass.asString());
    }
    catch(std::runtime_error& e) {
        throw std::runtime_error(std::string("Invalid values for parameters! ") + e.what());
    }
    catch(std::invalid_argument& e) {
        throw std::runtime_error(std::string("Invalid values for parameters! ") + e.what());
    }

    return params;
}

template <class T>
std::unique_ptr<T> open_file(const std::string& name) {
    std::unique_ptr<T> stream(new T(name));
    if (!stream->is_open())
        throw std::runtime_error("Cannot open output file: " + name + "!");
    return stream;
}

std::unique_ptr<std::ofstream> open_out_file(const std::string& name) {
    return open_file<std::ofstream>(name);
}

std::unique_ptr<std::ifstream> open_in_file(const std::string& name) {
    return open_file<std::ifstream>(name);
}


int main(int argc, char** argv) {
    std::map<std::string, docopt::value> args =
        docopt::docopt(usage, { argv + 1, argv + argc }, true, "PathPlanner demo");

    for(auto const& arg : args) {
        std::cout << arg.first << ": " << arg.second << std::endl;
    }

    try {
        auto ctrl_p = open_in_file(args["<trajectory>"].asString());
        Path traj = load_trajectory(*ctrl_p);
        traj[traj.size() - 2].tag = 1;
        traj.back().tag = 1;

        auto params = get_params(args);

        params.dump(std::cout);
        PathPlanner p(params);
        p.control_points() = traj;
        p.compute();

        // Output
        std::string output_dir = args["<output_dir>"].asString();
        if (output_dir.back() != '/')
            output_dir.push_back('/');

        {
            auto control_points = open_out_file(output_dir + "control_points.txt");
            auto trajectory = open_out_file(output_dir + "trajectory_points.txt");
            auto envir = open_out_file(output_dir + "environment_points.txt");
            auto left = open_out_file(output_dir + "left_points.txt");
            auto right = open_out_file(output_dir + "right_points.txt");
            auto center_vel = open_out_file(output_dir + "velocity.txt");
            auto left_vel = open_out_file(output_dir + "left_velocity.txt");
            auto right_vel = open_out_file(output_dir + "right_velocity.txt");
            auto recon = open_out_file(output_dir + "reconstructed_points.txt");

            gnuplot_output(*control_points, p.control_points());
            gnuplot_output(*trajectory, p.get_path());
            gnuplot_output(*left, p.get_left());
            gnuplot_output(*right, p.get_right());
            gnuplot_output(*center_vel, p.get_velocity());
            gnuplot_output(*left_vel, p.get_left_velocity());
            gnuplot_output(*right_vel, p.get_right_velocity());
            gnuplot_output(*recon, p.get_reconstructed());

            auto environment_file = args["--envir"];
            Path environment;
            if (environment_file) {
                std::ifstream f(environment_file.asString());
                environment = load_trajectory(f);
            }

            gnuplot_output(*envir, environment);
        }

        #ifdef ENABLE_GNUPLOT
            std::string cmd = GNUPLOT_BIN;
            cmd += " -e \"dir='" + output_dir + "'\" style.gnuplot";
            std::cout << cmd << "\n";
            system(cmd.c_str());
        #endif
    }
    catch(std::runtime_error& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }

    return 0;
}