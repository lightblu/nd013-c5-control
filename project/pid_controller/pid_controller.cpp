/**********************************************
 * Self-Driving Car Nano-degree - Udacity
 *  Created on: December 11, 2020
 *      Author: Mathilde Badoual
 **********************************************/

#include "pid_controller.h"
#include <vector>
#include <iostream>
#include <math.h>
#include <algorithm>

using namespace std;

PID::PID(): errorP_{0.0}, errorI_{0.0}, errorD_{0.0} {}

PID::~PID() {}

void PID::Init(double Kpi, double Kii, double Kdi, double output_lim_maxi, double output_lim_mini) {
   /**
   * TODO: Initialize PID coefficients (and errors, if needed)
   **/
   kP_ = Kpi;
   kI_ = Kii;
   kD_ = Kdi;
   oMax_ = output_lim_maxi;
   oMin_ = output_lim_mini;
   errorP_ = 0.0;
   errorI_ = 0.0;
   errorD_ = 0.0;
}

void PID::UpdateError(double cte) {
   /**
   * TODO: Update PID errors based on cte.
   **/
   if(!dt_){
      // We would get a div by zero, and we have likely not been initialized.
      // If really someone set the time do 0.0 we can claim there is no progress and nothing to do, makes not much sense.
      return;
   }

   errorD_ = (cte - errorP_) / dt_;
   errorP_ = cte;
   errorI_ = cte * dt_;

}

double PID::TotalError() {
   /**
   * TODO: Calculate and return the total error
    * The code should return a value in the interval [output_lim_mini, output_lim_maxi]
   */
    double control{(kP_ * errorP_) + (kI_ * errorI_) + (kD_ * errorD_)};

    return std::min(oMax_, std::max(oMin_, control));
}

void PID::UpdateDeltaTime(double new_delta_time) {
   /**
   * TODO: Update the delta time with new value
   */
   dt_ = new_delta_time;
}

std::ostream& operator<<(std::ostream& os, const PID& pid)
{
   return os << "P=" << pid.kP_ << " D=" << pid.kD_ << " I=" << pid.kI_;
}
