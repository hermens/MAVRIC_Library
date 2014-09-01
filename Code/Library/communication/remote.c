/**
 * \page The MAV'RIC License
 *
 * The MAV'RIC Framework
 *
 * Copyright © 2011-2014
 *
 * Laboratory of Intelligent Systems, EPFL
 */


/**
* \file remote.c
*
* This file is the driver for the remote control
*/

#include "remote.h"
#include "time_keeper.h"


//------------------------------------------------------------------------------
// PRIVATE FUNCTIONS DECLARATION
//------------------------------------------------------------------------------

static mode_flag_armed_t get_armed_flag(remote_t* remote);


//------------------------------------------------------------------------------
// PRIVATE FUNCTIONS IMPLEMENTATION
//------------------------------------------------------------------------------

static mode_flag_armed_t get_armed_flag(remote_t* remote)
{
	const remote_mode_t* remote_mode = &remote->mode;
	mode_flag_armed_t armed = remote_mode->current_desired_mode.ARMED;

	// Get armed flag
	if( remote_get_throttle(remote) < -0.95f && 
		remote_get_yaw(remote) > 0.9f && 
		remote_get_pitch(remote) > 0.9f && 
		remote_get_roll(remote) > 0.9f )
	{
		// Both sticks in bottom right corners => arm
		armed = ARMED_ON;
	}
	else if ( remote_get_throttle(remote) < -0.95f && 
			remote_get_yaw(remote) < -0.9f && 
			remote_get_pitch(remote) > 0.9f && 
			remote_get_roll(remote) < -0.9f )
	{
		// Both sticks in bottom left corners => disarm 
		armed = ARMED_OFF;
	}
	else
	{
		// Keep current flag
	}

	return armed;
}


//------------------------------------------------------------------------------
// PUBLIC FUNCTIONS IMPLEMENTATION
//------------------------------------------------------------------------------


void remote_init(remote_t* remote, const remote_conf_t* config, const mavlink_stream_t* mavlink_stream)
{
	// Init dependencies
	remote->mavlink_stream = mavlink_stream;
	remote->sat = spektrum_satellite_get_pointer();

	// Init mode from remote
	remote_mode_init( &remote->mode, &config->mode_config );

	// Init parameters according to remote type
	remote->type = config->type;
	switch ( remote->type )
	{
		case REMOTE_TURNIGY:
			remote->scale = 1.0f / 880.0f;
			remote->deadzone = 30;

			remote->channel_inv[CHANNEL_THROTTLE] = NORMAL;
			remote->channel_inv[CHANNEL_ROLL]     = NORMAL;
			remote->channel_inv[CHANNEL_PITCH]    = INVERTED;
			remote->channel_inv[CHANNEL_YAW]      = NORMAL;
			remote->channel_inv[CHANNEL_GEAR]     = INVERTED;
			remote->channel_inv[CHANNEL_FLAPS]    = INVERTED;
			remote->channel_inv[CHANNEL_AUX1]     = NORMAL;
			remote->channel_inv[CHANNEL_AUX2]     = NORMAL;
			break;

		case REMOTE_SPEKTRUM:
			remote->scale = 1.0f / 700.0f;
			remote->deadzone = 30;

			remote->channel_inv[CHANNEL_THROTTLE] = NORMAL;
			remote->channel_inv[CHANNEL_ROLL]     = INVERTED;
			remote->channel_inv[CHANNEL_PITCH]    = NORMAL;
			remote->channel_inv[CHANNEL_YAW]      = NORMAL;
			remote->channel_inv[CHANNEL_GEAR]     = NORMAL;
			remote->channel_inv[CHANNEL_FLAPS]    = NORMAL;
			remote->channel_inv[CHANNEL_AUX1]     = NORMAL;
			remote->channel_inv[CHANNEL_AUX2]     = NORMAL;
			break;
	}

	// Init 
	remote->signal_quality = SIGNAL_GOOD;
	for ( uint8_t i = 0; i < REMOTE_CHANNEL_COUNT; i++)
	{
		remote->channels[i] = 0.0f;
		remote->trims[i] = 0.0f;
	}
}


task_return_t remote_update(remote_t* remote)
{
	uint32_t now = time_keeper_get_time_ticks() ;
	float raw;
	
	if ( remote->sat->new_data_available == true )
	{
		// Check signal quality
		if ( remote->sat->dt < 100000) 
		{
			// ok
			remote->signal_quality = SIGNAL_GOOD;
		} 
		else if ( remote->sat->dt < 1500000 )
		{
			// warning
			remote->signal_quality = SIGNAL_BAD;
		}

		// Retrieve and scale channels
		for (uint8_t i = 0; i < REMOTE_CHANNEL_COUNT; ++i)
		{
			raw = remote->sat->channels[i];
			if ( raw < remote->deadzone && raw > -remote->deadzone )
			{
				remote->channels[i] = 0.0f;	
			}
			else
			{
				remote->channels[i] = raw * remote->scale * remote->channel_inv[i] - remote->trims[i];		
			}
		}

		// Indicate that data was handled
		remote->sat->new_data_available = false;
	}
	else
	{
		// Check for signal loss
		if ( ( now - remote->sat->last_update ) > 1500000 )
		{
			// CRITICAL: Set all channels to failsafe
			remote->channels[CHANNEL_THROTTLE] = -1.0f;
			for (uint8_t i = 1; i < REMOTE_CHANNEL_COUNT; i++) 
			{
				remote->channels[i] = 0.0f;
			}

			remote->signal_quality = SIGNAL_LOST;
		}
	}
	
	return TASK_RUN_SUCCESS;
}


signal_quality_t remote_check(const remote_t* remote)
{
	return remote->signal_quality;
}


void remote_calibrate(remote_t* remote, remote_channel_t channel)
{
	remote->trims[channel] = remote->channels[channel] - remote->trims[channel];
}


float remote_get_throttle(const remote_t* remote)
{	
	return remote->channels[CHANNEL_THROTTLE];
}


float remote_get_roll(const remote_t* remote)
{
	return remote->channels[CHANNEL_ROLL];
}


float remote_get_pitch(const remote_t* remote)
{
	return remote->channels[CHANNEL_PITCH];
}


float remote_get_yaw(const remote_t* remote)
{
	return remote->channels[CHANNEL_YAW];
}


void remote_mode_init(remote_mode_t* remote_mode, const remote_mode_conf_t* config)
{
	// Init parameters
	remote_mode->safety_channel			= config->safety_channel;				 
	remote_mode->safety_mode			= config->safety_mode;				 
	remote_mode->mode_switch_channel	= config->mode_switch_channel;		 
	remote_mode->mode_switch_up			= config->mode_switch_up;				 
	remote_mode->mode_switch_middle		= config->mode_switch_middle;			 
	remote_mode->mode_switch_down		= config->mode_switch_down;			 
	remote_mode->use_custom_switch		= config->use_custom_switch;			 
	remote_mode->custom_switch_channel 	= config->custom_switch_channel;		 
	remote_mode->use_test_switch		= config->use_test_switch;			 
	remote_mode->test_switch_channel	= config->test_switch_channel;
	remote_mode->use_disable_remote_mode_switch	= config->use_disable_remote_mode_switch;			 
	remote_mode->disable_remote_mode_channel		= config->disable_remote_mode_channel;

	// Init state to safety state, disarmed
	remote_mode->current_desired_mode 		= remote_mode->safety_mode;	
	remote_mode->current_desired_mode.ARMED = ARMED_OFF;
}


void remote_mode_update(remote_t* remote)
{
	remote_mode_t* remote_mode = &remote->mode;
	bool do_update = false;


	// Check whether modes should be updated
	if ( remote_mode->use_disable_remote_mode_switch == true )
	{
		if ( remote->channels[remote_mode->disable_remote_mode_channel] >= 0.5f )
		{
			do_update = true;
		}
		else
		{
			do_update = false;
		}
	}
	else
	{
		do_update = true;
	}

	// Do update if required
	if( do_update == true )
	{
		// Fallback to safety
		mav_mode_t new_desired_mode = remote_mode->safety_mode;

		// Get armed flag from stick combinaison
		mode_flag_armed_t flag_armed = get_armed_flag(remote);

		if ( remote->channels[remote_mode->safety_channel] > 0 )
		{
			// Safety switch UP => Safety mode ON
			new_desired_mode = remote_mode->safety_mode;

			// Allow arm and disarm in safety mode
			new_desired_mode.ARMED = flag_armed;
		}
		else
		{
			// Normal mode

			// Get base mode
			if ( remote->channels[remote_mode->mode_switch_channel] >= 0.5f )
			{
				// Mode switch UP
				new_desired_mode = remote_mode->mode_switch_up;
			}
			else if ( 	remote->channels[remote_mode->mode_switch_channel] < 0.5f &&
						remote->channels[remote_mode->mode_switch_channel] > -0.5f )
			{
				// Mode switch MIDDLE
				new_desired_mode = remote_mode->mode_switch_middle;	
			}
			else if ( remote->channels[remote_mode->mode_switch_channel] <= -0.5f )
			{
				// Mode switch DOWN
				new_desired_mode = remote_mode->mode_switch_down;
			}


			// Apply custom flag
			if ( remote_mode->use_custom_switch == true )
			{
				if ( remote->channels[remote_mode->custom_switch_channel] > 0.0f )
				{
					// Custom channel at 100% => CUSTOM_ON;
					new_desired_mode.CUSTOM = CUSTOM_ON;
				}
				else
				{
					// Custom channel at -100% => CUSTOM_OFF;
					new_desired_mode.CUSTOM = CUSTOM_OFF;
				}
			}


			// Apply test flag
			if ( remote_mode->use_test_switch == true )
			{
				if ( remote->channels[remote_mode->test_switch_channel] > 0.0f )
				{
					// Test channel at 100% => TEST_ON
					new_desired_mode.TEST = TEST_ON;
				}
				else
				{
					// Test channel at -100% => TEST_OFF;
					new_desired_mode.TEST = TEST_OFF;
				}
			}


			// Allow only disarm in normal mode
			if ( flag_armed == ARMED_OFF )
			{
				new_desired_mode.ARMED = ARMED_OFF;
			}
			else
			{
				// Keep current armed flag
				new_desired_mode.ARMED = remote_mode->current_desired_mode.ARMED;
			}
		}

		// Store desired mode
		remote_mode->current_desired_mode = new_desired_mode;
	}
}


mav_mode_t remote_mode_get(const remote_t* remote)
{
	return remote->mode.current_desired_mode;
}


task_return_t remote_send_raw(const remote_t* remote)
{
	mavlink_message_t msg;
	mavlink_msg_rc_channels_raw_pack(	remote->mavlink_stream->sysid,
										remote->mavlink_stream->compid,
										&msg,
										time_keeper_get_millis(),
										0,
										remote->sat->channels[0] + 1024,
										remote->sat->channels[1] + 1024,
										remote->sat->channels[2] + 1024,
										remote->sat->channels[3] + 1024,
										remote->sat->channels[4] + 1024,
										remote->sat->channels[5] + 1024,
										remote->sat->channels[6] + 1024,
										remote->sat->channels[7] + 1024,
										// remote->mode.current_desired_mode.byte);
										remote->signal_quality	);
	
	mavlink_stream_send(remote->mavlink_stream, &msg);
	
	return TASK_RUN_SUCCESS;
}


task_return_t remote_send_scaled(const remote_t* remote)
{
	mavlink_message_t msg;
	mavlink_msg_rc_channels_scaled_pack(	remote->mavlink_stream->sysid,
											remote->mavlink_stream->compid,
											&msg,
											time_keeper_get_millis(),
											0,
											remote->channels[0] * 10000.0f,
											remote->channels[1] * 10000.0f,
											remote->channels[2] * 10000.0f,
											remote->channels[3] * 10000.0f,
											remote->channels[4] * 10000.0f,
											remote->channels[5] * 10000.0f,
											remote->channels[6] * 10000.0f,
											remote->channels[7] * 10000.0f,
											remote->mode.current_desired_mode.byte );
											// remote->signal_quality	);
	
	mavlink_stream_send(remote->mavlink_stream, &msg);
	
	return TASK_RUN_SUCCESS;	
}