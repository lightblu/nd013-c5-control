/**********************************************
 * Self-Driving Car Nano-degree - Udacity
 *  Created on: September 20, 2020
 *      Author: Munir Jojo-Verge
 				Aaron Brown
 **********************************************/

/**
 * @file main.cpp
 **/

#include <string>
#include <array>
#include <cfloat>
#include <chrono>
#include <cmath>
#include <iostream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <tuple>
#include <vector>
#include <iostream>
#include <fstream>
#include <typeinfo>

#include "json.hpp"
#include <carla/client/ActorBlueprint.h>
#include <carla/client/BlueprintLibrary.h>
#include <carla/client/Client.h>
#include <carla/client/Map.h>
#include <carla/client/Sensor.h>
#include <carla/client/TimeoutException.h>
#include <carla/client/World.h>
#include <carla/geom/Transform.h>
#include <carla/image/ImageIO.h>
#include <carla/image/ImageView.h>
#include <carla/sensor/data/Image.h>
#include "Eigen/QR"
#include "behavior_planner_FSM.h"
#include "motion_planner.h"
#include "planning_params.h"
#include "utils.h"
#include "pid_controller.h"

#include <limits>
#include <iostream>
#include <fstream>
#include <uWS/uWS.h>
#include <math.h>
#include <vector>
#include <cmath>
#include <time.h>

using namespace std;
using json = nlohmann::json;

#define _USE_MATH_DEFINES

string hasData(string s) {
  auto found_null = s.find("null");
    auto b1 = s.find_first_of("{");
    auto b2 = s.find_first_of("}");
    if (found_null != string::npos) {
      return "";
    }
    else if (b1 != string::npos && b2 != string::npos) {
      return s.substr(b1, b2 - b1 + 1);
    }
    return "";
}


template <typename T> int sgn(T val) {
    return (T(0) < val) - (val < T(0));
}

double angle_between_points(double x1, double y1, double x2, double y2){
  return atan2(y2-y1, x2-x1);
}

BehaviorPlannerFSM behavior_planner(
      P_LOOKAHEAD_TIME, P_LOOKAHEAD_MIN, P_LOOKAHEAD_MAX, P_SPEED_LIMIT,
      P_STOP_THRESHOLD_SPEED, P_REQ_STOPPED_TIME, P_REACTION_TIME,
      P_MAX_ACCEL, P_STOP_LINE_BUFFER);

// Decalre and initialized the Motion Planner and all its class requirements
MotionPlanner motion_planner(P_NUM_PATHS, P_GOAL_OFFSET, P_ERR_TOLERANCE);

bool have_obst = false;
vector<State> obstacles;

void path_planner(vector<double>& x_points, vector<double>& y_points, vector<double>& v_points, double yaw, double velocity, State goal, bool is_junction, string tl_state, vector< vector<double> >& spirals_x, vector< vector<double> >& spirals_y, vector< vector<double> >& spirals_v, vector<int>& best_spirals){

  State ego_state;

  ego_state.location.x = x_points[x_points.size()-1];
  ego_state.location.y = y_points[y_points.size()-1];
  ego_state.velocity.x = velocity;

  if( x_points.size() > 1 ){
  	ego_state.rotation.yaw = angle_between_points(x_points[x_points.size()-2], y_points[y_points.size()-2], x_points[x_points.size()-1], y_points[y_points.size()-1]);
  	ego_state.velocity.x = v_points[v_points.size()-1];
  	if(velocity < 0.01)
  		ego_state.rotation.yaw = yaw;

  }

  Maneuver behavior = behavior_planner.get_active_maneuver();

  goal = behavior_planner.state_transition(ego_state, goal, is_junction, tl_state);

  if(behavior == STOPPED){

  	int max_points = 20;
  	double point_x = x_points[x_points.size()-1];
  	double point_y = y_points[x_points.size()-1];
  	while( x_points.size() < max_points ){
  	  x_points.push_back(point_x);
  	  y_points.push_back(point_y);
  	  v_points.push_back(0);

  	}
  	return;
  }

  auto goal_set = motion_planner.generate_offset_goals(goal);

  auto spirals = motion_planner.generate_spirals(ego_state, goal_set);

  auto desired_speed = utils::magnitude(goal.velocity);

  State lead_car_state;  // = to the vehicle ahead...

  if(spirals.size() == 0){
  	cout << "Error: No spirals generated " << endl;
  	return;
  }

  for(int i = 0; i < spirals.size(); i++){

    auto trajectory = motion_planner._velocity_profile_generator.generate_trajectory( spirals[i], desired_speed, ego_state,
                                                                                    lead_car_state, behavior);

    vector<double> spiral_x;
    vector<double> spiral_y;
    vector<double> spiral_v;
    for(int j = 0; j < trajectory.size(); j++){
      double point_x = trajectory[j].path_point.x;
      double point_y = trajectory[j].path_point.y;
      double velocity = trajectory[j].v;
      spiral_x.push_back(point_x);
      spiral_y.push_back(point_y);
      spiral_v.push_back(velocity);
    }

    spirals_x.push_back(spiral_x);
    spirals_y.push_back(spiral_y);
    spirals_v.push_back(spiral_v);

  }

  best_spirals = motion_planner.get_best_spiral_idx(spirals, obstacles, goal);
  int best_spiral_idx = -1;

  if(best_spirals.size() > 0)
  	best_spiral_idx = best_spirals[best_spirals.size()-1];

  int index = 0;
  int max_points = 20;
  int add_points = spirals_x[best_spiral_idx].size();
  while( x_points.size() < max_points && index < add_points ){
    double point_x = spirals_x[best_spiral_idx][index];
    double point_y = spirals_y[best_spiral_idx][index];
    double velocity = spirals_v[best_spiral_idx][index];
    index++;
    x_points.push_back(point_x);
    y_points.push_back(point_y);
    v_points.push_back(velocity);
  }


}

void set_obst(vector<double> x_points, vector<double> y_points, vector<State>& obstacles, bool& obst_flag){

	for( int i = 0; i < x_points.size(); i++){
		State obstacle;
		obstacle.location.x = x_points[i];
		obstacle.location.y = y_points[i];
		obstacles.push_back(obstacle);
	}
	obst_flag = true;
}

int main ()
{
  cout << "starting server" << endl;
  uWS::Hub h;

  double new_delta_time;
  int i = 0;

  time_t prev_timer;
  time_t timer;
  time(&prev_timer);

  // initialize pid steer & pid throttle
  
  /**
  * TODO (Step 1): create pid (pid_steer) for steer command and initialize values
  **/
  /**
  * TODO (Step 1): create pid (pid_throttle) for throttle command and initialize values
  **/
  PID pid_steer = PID();
  PID pid_throttle = PID();

  // Via instructions: The output of the steer controller should be inside [-1.2, 1.2], throttle inside [-1, 1].
  // Not good style follows with two statements on one line, but for easier testing and documenting throguh parameter attempts..

  //pid_steer.Init(0.0, 0.00, 0.00, 1.2, -1.2); pid_throttle.Init(0.01, 0.00, 0.00, 1, -1); // #01 => much too little throttle, no movement
  //pid_steer.Init(0.0, 0.00, 0.00, 1.2, -1.2); pid_throttle.Init(0.1, 0.00, 0.00, 1, -1); // #02 => some osciallations, try to dampen then critically next
  //pid_steer.Init(0.0, 0.00, 0.00, 1.2, -1.2); pid_throttle.Init(0.1, 0.00, 0.01, 1, -1); // #03 => start 
  //pid_steer.Init(0.0, 0.00, 0.00, 1.2, -1.2); pid_throttle.Init(0.1, 0.00, 0.1, 1, -1); // #04 => more dampening
  //pid_steer.Init(0.0, 0.00, 0.00, 1.2, -1.2); pid_throttle.Init(0.2, 0.00, 0.15, 1, -1); // #05 => still no effect.. increase p and also d a bit more?
  //pid_steer.Init(0.0, 0.00, 0.00, 1.2, -1.2); pid_throttle.Init(0.1, 0.05, 0.1, 1, -1); // #06 => more dampening
  //pid_steer.Init(0.2, 0.05, 0.1, 1.2, -1.2); pid_throttle.Init(0.15, 0.05, 0.1, 1, -1); // #07 => try together with streeing, throttle aloen seems to introduce other problems => REACHES END OF ROAD!!
  //pid_steer.Init(0.2, 0.01, 0.05, 1.2, -1.2); pid_throttle.Init(0.15, 0.02, 0.15, 1, -1); // #07 => try together with streeing, throttle aloen seems to introduce other problems => REACHES END OF ROAD!!

  
  //pid_steer.Init(0.2, 0.0011, 0.1, 1.2, -1.2); pid_throttle.Init(0.3, 0.001, 0.15, 1.0, -1.0); // #8
  pid_steer.Init(0.5, 0.00001, 0.07, 0.6, -0.6); pid_throttle.Init(0.25, 0.00001, 0.12, 0.5, -0.5); // #8
  
    
  //pid_steer.Init(0.3, 0.01, 0.1, 1.2, -1.2); pid_throttle.Init(0.2, 0.02, 0.11, 1, -1); // #07 => try together with streeing, throttle aloen seems to introduce other problems => REACHES END OF ROAD!!

  // Clear outputfiles and put PID parameters as first comment line into them
  fstream file_steer;
  file_steer.open("steer_pid_data.txt", std::ofstream::out | std::ofstream::trunc);
  file_steer << "#" << pid_steer << std::endl;
  file_steer.close();
  fstream file_throttle;
  file_throttle.open("throttle_pid_data.txt", std::ofstream::out | std::ofstream::trunc);
  file_throttle << "#" << pid_throttle << std::endl;
  file_throttle.close();
  
  
  h.onMessage([&pid_steer, &pid_throttle, &new_delta_time, &timer, &prev_timer, &i, &prev_timer](uWS::WebSocket<uWS::SERVER> ws, char *data, size_t length, uWS::OpCode opCode)
  {
        auto s = hasData(data);

        if (s != "") {

          auto data = json::parse(s);

          // create file to save values
          fstream file_steer;
          file_steer.open("steer_pid_data.txt", std::ios_base::app);
          fstream file_throttle;
          file_throttle.open("throttle_pid_data.txt", std::ios_base::app);

          vector<double> x_points = data["traj_x"];
          vector<double> y_points = data["traj_y"];
          vector<double> v_points = data["traj_v"];
          double yaw = data["yaw"];
          double velocity = data["velocity"];
          double sim_time = data["time"];
          double waypoint_x = data["waypoint_x"];
          double waypoint_y = data["waypoint_y"];
          double waypoint_t = data["waypoint_t"];
          bool is_junction = data["waypoint_j"];
          string tl_state = data["tl_state"];

          double x_position = data["location_x"];
          double y_position = data["location_y"];
          double z_position = data["location_z"];

          if(!have_obst){
          	vector<double> x_obst = data["obst_x"];
          	vector<double> y_obst = data["obst_y"];
          	set_obst(x_obst, y_obst, obstacles, have_obst);
          }

          State goal;
          goal.location.x = waypoint_x;
          goal.location.y = waypoint_y;
          goal.rotation.yaw = waypoint_t;

          vector< vector<double> > spirals_x;
          vector< vector<double> > spirals_y;
          vector< vector<double> > spirals_v;
          vector<int> best_spirals;

          path_planner(x_points, y_points, v_points, yaw, velocity, goal, is_junction, tl_state, spirals_x, spirals_y, spirals_v, best_spirals);

          // Save time and compute delta time
          time(&timer);
          new_delta_time = difftime(timer, prev_timer);
          prev_timer = timer;

          ////////////////////////////////////////
          // Steering control
          ////////////////////////////////////////

          /**
          * TODO (step 3): uncomment these lines
          **/
         // Update the delta time with the previous command
         pid_steer.UpdateDeltaTime(new_delta_time);

          // Compute steer error
          double error_steer;
          double steer_output;

          /**
          * TODO (step 3): compute the steer error (error_steer) from the position and the desired trajectory
          **/
          // This was the most unclear part from the instructions, as for the velocities instruction said:
          // "The last point of v_points vector contains the velocity computed by the path planner."
          // However from the forum, and the following output, figured out that we should figure out which point
          // from the 20 planned ones is actually the next. When slow, this is usually always 0?!
          // We do this by looking for the one with the smallest distance that is in front of us!
          size_t best_idx = 0;
          size_t best_dist = HUGE_VAL;
          
          cout << "\n*** " << i << "[dt=" << new_delta_time << "] ***" << "x=" << x_position << " y=" << y_position << " yaw=" << yaw << " v=" << velocity << "\n";
          for(size_t idx=0; idx<x_points.size(); idx++)
          {
            double distance = sqrt(pow((x_position - x_points[idx]), 2) + pow((y_position - y_points[idx]), 2));
            double angle = angle_between_points(x_position, y_position, x_points[idx],  y_points[idx]);
            cout << "  [" << idx << "] x=" << x_points[idx] << " y=" << y_points[idx] << " dist=" << distance << " angle=" << (angle-yaw) << "(" << (angle-yaw)*(180.0/3.141592653589793238463) << ")" << " v=" << v_points[idx] << "\n";

            // We only want to consider those points that lie "forward" of us, so within the possible steering of -68deg til 68deg (1.2 in radians), then we want the closest point
            if((-1.2 <= (angle-yaw)) && ((angle-yaw) <= 1.2) && distance < best_dist && distance > 5)
            {
          		best_idx = idx;
                best_dist = distance;
            }
          }
          //best_i = 18;  // Or fix it with large lookahead?
          cout << " The best next point from the planner is at index: " << best_idx << "\n";
          
          
          // The desired
          double wanted_angle = angle_between_points(x_position, y_position, x_points[best_idx], y_points[best_idx]);
          error_steer = wanted_angle - yaw;

          /**
          * TODO (step 3): uncomment these lines
          **/
          // Compute control to apply
          pid_steer.UpdateError(error_steer);
          steer_output = pid_steer.TotalError();

          // Save data
          file_steer  << i ;
          file_steer  << " " << error_steer;
          file_steer  << " " << steer_output << endl;
          cout << "+++ " << i << " +++" << "steer error_steer=" << error_steer << " steer=" << steer_output << std::endl;


          ////////////////////////////////////////
          // Throttle control
          ////////////////////////////////////////

          /**
          * TODO (step 2): uncomment these lines
          **/
          // Update the delta time with the previous command
          pid_throttle.UpdateDeltaTime(new_delta_time);

          // Compute error of speed
          double error_throttle;
          /**
          * TODO (step 2): compute the throttle error (error_throttle) from the position and the desired speed
          **/
          // modify the following line for step 2

          // Instructions: "The last point of v_points vector contains the velocity computed by the path planner."
          // So our error is the difference between that and the current velocity.
          
          // Proposal from instructions:
          //error_throttle = v_points[v_points.size()-1] - velocity;
          // But as described above, we also take our best_i we found here!
          //error_throttle = v_points[best_idx] - velocity;
          // The planned speed is too much!
          error_throttle = (v_points[best_idx]) - velocity;

          double throttle_output;
          double brake_output;

          /**
          * TODO (step 2): uncomment these lines
          **/
          // Compute control to apply
          pid_throttle.UpdateError(error_throttle);
          double throttle = pid_throttle.TotalError();

          // Adapt the negative throttle to break
          if (throttle > 0.0) {
           throttle_output = throttle;
           brake_output = 0;
          } else {
           throttle_output = 0;
           brake_output = -throttle;
          }

          // Save data
          file_throttle  << i ;
          file_throttle  << " " << error_throttle;
          file_throttle  << " " << brake_output;
          file_throttle  << " " << throttle_output << endl;

          cout << "+++ " << i << " +++" << "throttle error=" << error_throttle << " throttle=" << throttle_output << " break=" << brake_output << std::endl << endl;

          // Send control
          json msgJson;
          msgJson["brake"] = brake_output;
          msgJson["throttle"] = throttle_output;
          msgJson["steer"] = steer_output;

          msgJson["trajectory_x"] = x_points;
          msgJson["trajectory_y"] = y_points;
          msgJson["trajectory_v"] = v_points;
          msgJson["spirals_x"] = spirals_x;
          msgJson["spirals_y"] = spirals_y;
          msgJson["spirals_v"] = spirals_v;
          msgJson["spiral_idx"] = best_spirals;
          msgJson["active_maneuver"] = behavior_planner.get_active_maneuver();

          //  min point threshold before doing the update
          // for high update rate use 19 for slow update rate use 4
          msgJson["update_point_thresh"] = 16;

          auto msg = msgJson.dump();

          i = i + 1;
          file_steer.close();
          file_throttle.close();

      ws.send(msg.data(), msg.length(), uWS::OpCode::TEXT);

    }

  });


  h.onConnection([](uWS::WebSocket<uWS::SERVER> ws, uWS::HttpRequest req)
  {
      cout << "Connected!!!" << endl;
    });


  h.onDisconnection([&h](uWS::WebSocket<uWS::SERVER> ws, int code, char *message, size_t length)
    {
      ws.close();
      cout << "Disconnected" << endl;
    });

  int port = 4567;
  if (h.listen("0.0.0.0", port))
    {
      cout << "Listening to port " << port << endl;
      h.run();
    }
  else
    {
      cerr << "Failed to listen to port" << endl;
      return -1;
    }


}
