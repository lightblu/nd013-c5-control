/**********************************************
 * Self-Driving Car Nano-degree - Udacity
 *  Created on: December 11, 2020
 *      Author: Mathilde Badoual
 **********************************************/

#ifndef PID_CONTROLLER_H
#define PID_CONTROLLER_H

#include <iostream>

class PID {

private:
   /**
   * TODO: Create the PID class
   **/

    /*
    * Errors
    */
   double errorP_;
   double errorI_;
   double errorD_;

    /*
    * Coefficients
    */
   double kP_;
   double kI_;
   double kD_;

    /*
    * Output limits
    */
   double oMax_;
   double oMin_;
  
    /*
    * Delta time
    */
   double dt_;

public:

    /*
    * Constructor
    */
    PID();

    /*
    * Destructor.
    */
    virtual ~PID();

    /*
    * Initialize PID.
    */
    void Init(double Kp, double Ki, double Kd, double output_lim_max, double output_lim_min);

    /*
    * Update the PID error variables given cross track error.
    */
    void UpdateError(double cte);

    /*
    * Calculate the total PID error.
    */
    double TotalError();
  
    /*
    * Update the delta time.
    */
    void UpdateDeltaTime(double new_delta_time);

    friend std::ostream& operator<<(std::ostream& os, const PID& pid);
};

#endif //PID_CONTROLLER_H


