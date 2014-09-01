/** 
 * \page The MAV'RIC license
 *
 * The MAV'RIC Framework
 *
 * Copyright © 2011-2014
 *
 * Laboratory of Intelligent Systems, EPFL
 */
 
 
/**
 * \file stabilisation.h
 *
 * Executing the PID controllers for stabilization
 */


#ifndef STABILISATION_H_
#define STABILISATION_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "imu.h"
#include "pid_control.h"
#include "scheduler.h"
#include "mavlink_communication.h"

/**
 * \brief	The control mode enum
 */
typedef enum 
{
	VELOCITY_COMMAND_MODE, 
	ATTITUDE_COMMAND_MODE, 
	RATE_COMMAND_MODE
} control_mode_t;

/**
 * \brief	The yaw control mode enum
 */
typedef enum     
{
	YAW_RELATIVE, 
	YAW_ABSOLUTE, 
	YAW_COORDINATED
} yaw_mode_t;

/**
 * \brief	The control command typedef
 */
typedef struct 
{
	float rpy[3];								///< roll pitch yaw rates/angles
	float thrust;								///< thrust
	float tvel[3];								///< target velocity in m/s
	float theading;								///< absolute target heading	
	control_mode_t control_mode;				///< control mode
	yaw_mode_t     yaw_mode;					///< yaw mode
	
	const mavlink_stream_t* mavlink_stream;		///< The pointer to the mavlink stream
} control_command_t;

/**
 * \brief	The structure used to control the vehicle with 4 PIDs
 */
typedef struct {
	pid_controller_t rpy_controller[3];			///< roll pitch yaw  controllers
	pid_controller_t thrust_controller;			///< thrust controller
	control_command_t output;					///< output
	
	const mavlink_stream_t* mavlink_stream;		///< The pointer to the mavlink stream
} stabiliser_t;

/**
 * \brief	Initialisation of the stabilisation module
 * \param	command				The pointer to the command structure
 * \param	mavlink_stream		The pointer to the mavlink stream
 */
void stabilisation_init(control_command_t *controls, const mavlink_stream_t* mavlink_stream);

/**
 * \brief				Execute the PID controllers used for stabilization
 * 
 * \param	stabiliser	Pointer to the structure containing the PID controllers
 * \param	dt			Timestep
 * \param	errors		Array containing the errors of the controlling variables
 */
void stabilisation_run(stabiliser_t *stabiliser, float dt, float errors[]);

/**
 * \brief	Task to send the mavlink roll, pitch, yaw angular speeds and thrust setpoints message
 *
 * \param	stabiliser	Pointer to the structure containing the PID controller
 * 
 * \return	The status of execution of the task
 */
task_return_t stabilisation_send_rpy_speed_thrust_setpoint(stabiliser_t* stabiliser);

/**
 * \brief	Task to send the mavlink roll, pitch and yaw errors message
 * 
 * \param	stabiliser	Pointer to the structure containing the PID controller
 *
 * \return	The status of execution of the task
 */
task_return_t stabilisation_send_rpy_rates_error(stabiliser_t* stabiliser);

/**
 * \brief	Task to send the mavlink roll, pitch, yaw and thrust setpoints message
 *
 * \param	stabiliser	Pointer to the structure containing the PID controller
 * 
 * \return	The status of execution of the task
 */
task_return_t stabilisation_send_rpy_thrust_setpoint(control_command_t* controls);

#ifdef __cplusplus
}
#endif

#endif /* STABILISATION_H_ */