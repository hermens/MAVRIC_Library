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
 * \file mavlink_waypoint_handler.c
 * 
 * The mavlink waypoint handler
 */


#include "mavlink_waypoint_handler.h"
#include "print_util.h"
#include "remote_controller.h"
#include "time_keeper.h"
#include "maths.h"

//------------------------------------------------------------------------------
// PRIVATE FUNCTIONS DECLARATION
//------------------------------------------------------------------------------

/**
 * \brief	Sets a circle scenario, where two waypoints are set at opposite side of the circle
 *
 * \param	waypoint_handler		The pointer to the structure of the mavlink waypoint handler
 * \param	packet					The pointer to the structure of the mavlink command message long
 */
static void waypoint_handler_set_circle_scenario(mavlink_waypoint_handler_t* waypoint_handler, mavlink_command_long_t* packet);

/**
 * \brief	Sends the number of onboard waypoint to mavlink when asked by ground station
 *
 * \param	waypoint_handler		The pointer to the waypoint handler structure
 * \param	rec						The pointer to the received mavlink message structure asking the send count
 */
static void waypoint_handler_send_count(mavlink_waypoint_handler_t* waypoint_handler, mavlink_received_t* rec);

/**
 * \brief	Sends a given waypoint via a mavlink message
 *
 * \param	waypoint_handler		The pointer to the waypoint handler structure
 * \param	rec						The pointer to the received mavlink message structure asking for a waypoint
 */
static void waypoint_handler_send_waypoint(mavlink_waypoint_handler_t* waypoint_handler, mavlink_received_t* rec);

/**
 * \brief	Receives a acknowledge message from mavlink
 *
 * \param	waypoint_handler		The pointer to the waypoint handler structure
 * \param	rec						The received mavlink message structure
 */
static void waypoint_handler_receive_ack_msg(mavlink_waypoint_handler_t* waypoint_handler, mavlink_received_t* rec);

/**
 * \brief	Receives the number of waypoints that the ground station is sending
 *
 * \param	waypoint_handler		The pointer to the waypoint handler structure
 * \param	rec						The received mavlink message structure with the total number of waypoint
 */
static void waypoint_handler_receive_count(mavlink_waypoint_handler_t* waypoint_handler, mavlink_received_t* rec);

/**
 * \brief	Receives a given waypoint and stores it in the local structure
 *
 * \param	waypoint_handler		The pointer to the waypoint handler structure
 * \param	rec						The received mavlink message structure with the waypoint
 */
static void waypoint_handler_receive_waypoint(mavlink_waypoint_handler_t* waypoint_handler, mavlink_received_t* rec);

/**
 * \brief	Sets the current waypoint to num_of_waypoint
 *
 * \param	waypoint_handler		The pointer to the waypoint handler structure
 * \param	rec						The received mavlink message structure with the number of the current waypoint
 */
static void waypoint_handler_set_current_waypoint(mavlink_waypoint_handler_t* waypoint_handler, mavlink_received_t* rec);

/**
 * \brief	Set the current waypoint to new_current
 *
 * \param	waypoint_handler		The pointer to the waypoint handler
 * \param	packet					The pointer to the decoded mavlink message long
 */
static void waypoint_handler_set_current_waypoint_from_parameter(mavlink_waypoint_handler_t* waypoint_handler, mavlink_command_long_t* packet);

/**
 * \brief	Clears the waypoint list
 *
 * \param	waypoint_handler		The pointer to the waypoint handler structure
 * \param	rec						The received mavlink message structure with the clear command
 */
static void waypoint_handler_clear_waypoint_list(mavlink_waypoint_handler_t* waypoint_handler, mavlink_received_t* rec);

/**
 * \brief	Set a new home position, origin of the local frame
 *
 * \param	waypoint_handler		The pointer to the waypoint handler
 * \param	rec						The received mavlink message structure with the new home position
 */
static void waypoint_handler_set_home(mavlink_waypoint_handler_t* waypoint_handler, mavlink_received_t* rec);

/**
 * \brief	Drives the auto landing procedure from the MAV_CMD_NAV_LAND message long
 *
 * \param	waypoint_handler		The pointer to the structure of the mavlink waypoint handler
 * \param	packet					The pointer to the structure of the mavlink command message long
 */
static void waypoint_handler_auto_landing(mavlink_waypoint_handler_t* waypoint_handler, mavlink_command_long_t* packet);

/**
 * \brief	Set the next waypoint as current waypoint
 *
 * \param	waypoint_handler		The pointer to the structure of the mavlink waypoint handler
 * \param	packet					The pointer to the structure of the mavlink command message long
 */
static void waypoint_handler_continue_to_next_waypoint(mavlink_waypoint_handler_t* waypoint_handler, mavlink_command_long_t* packet);

/**
 * \brief	Sets auto-takeoff procedure from a mavlink command message MAV_CMD_NAV_TAKEOFF
 *
 * \param	waypoint_handler		The pointer to the structure of the mavlink waypoint handler
 * \param	packet					The pointer to the structure of the mavlink command message long
 */
static void waypoint_handler_set_auto_takeoff(mavlink_waypoint_handler_t *waypoint_handler, mavlink_command_long_t* packet);

// TODO: Add code in the function :)
//static void set_stream_scenario(waypoint_struct waypoint_list[], uint16_t* number_of_waypoints, float circle_radius, float num_of_vhc);

//------------------------------------------------------------------------------
// PRIVATE FUNCTIONS IMPLEMENTATION
//------------------------------------------------------------------------------

static void waypoint_handler_set_circle_scenario(mavlink_waypoint_handler_t* waypoint_handler, mavlink_command_long_t* packet)
{
	float circle_radius = packet->param1;
	float num_of_vhc = packet->param2;
	
	float angle_step = 2.0 * PI / num_of_vhc;
	
	waypoint_struct waypoint;
	
	local_coordinates_t waypoint_transfo;
	global_position_t waypoint_global;
	
	waypoint_handler->number_of_waypoints = 2;
	waypoint_handler->current_waypoint_count = -1;
	
	waypoint_transfo.origin = waypoint_handler->position_estimator->local_position.origin;
	
	// Start waypoint
	waypoint_transfo.pos[X] = circle_radius * cos(angle_step * (waypoint_handler->mavlink_stream->sysid-1));
	waypoint_transfo.pos[Y] = circle_radius * sin(angle_step * (waypoint_handler->mavlink_stream->sysid-1));
	waypoint_transfo.pos[Z] = -20.0f;
	waypoint_global = coord_conventions_local_to_global_position(waypoint_transfo);
	
	print_util_dbg_print("Circle departure(x100): (");
	print_util_dbg_print_num(waypoint_transfo.pos[X]*100.0f,10);
	print_util_dbg_print(", ");
	print_util_dbg_print_num(waypoint_transfo.pos[Y]*100.0f,10);
	print_util_dbg_print(", ");
	print_util_dbg_print_num(waypoint_transfo.pos[Z]*100.0f,10);
	print_util_dbg_print("). For system:");
	print_util_dbg_print_num(waypoint_handler->mavlink_stream->sysid,10);
	print_util_dbg_print(".\n");
	waypoint.x = waypoint_global.latitude;
	waypoint.y = waypoint_global.longitude;
	waypoint.z = waypoint_global.altitude;
	
	waypoint.autocontinue = 0;
	waypoint.current = 0;
	waypoint.frame = MAV_FRAME_GLOBAL;
	waypoint.waypoint_id = MAV_CMD_NAV_WAYPOINT;
	
	waypoint.param1 = 10; // Hold time in decimal seconds
	waypoint.param2 = 4; // Acceptance radius in meters
	waypoint.param3 = 0; //  0 to pass through the WP, if > 0 radius in meters to pass by WP. Positive value for clockwise orbit, negative value for counter-clockwise orbit. Allows trajectory control.
	waypoint.param4 = rad_to_deg(maths_calc_smaller_angle(PI + angle_step * (waypoint_handler->mavlink_stream->sysid-1))); // Desired yaw angle at MISSION (rotary wing)
	
	waypoint_handler->waypoint_list[0] = waypoint;
	
	// End waypoint
	waypoint_transfo.pos[X] = circle_radius * cos(angle_step * (waypoint_handler->mavlink_stream->sysid-1) + PI);
	waypoint_transfo.pos[Y] = circle_radius * sin(angle_step * (waypoint_handler->mavlink_stream->sysid-1) + PI);
	waypoint_transfo.pos[Z] = -20.0f;
	waypoint_global = coord_conventions_local_to_global_position(waypoint_transfo);
	
	print_util_dbg_print("Circle destination(x100): (");
	print_util_dbg_print_num(waypoint_transfo.pos[X]*100.0f,10);
	print_util_dbg_print(", ");
	print_util_dbg_print_num(waypoint_transfo.pos[Y]*100.0f,10);
	print_util_dbg_print(", ");
	print_util_dbg_print_num(waypoint_transfo.pos[Z]*100.0f,10);
	print_util_dbg_print("). For system:");
	print_util_dbg_print_num(waypoint_handler->mavlink_stream->sysid,10);
	print_util_dbg_print(".\n");
	
	waypoint.x = waypoint_global.latitude;
	waypoint.y = waypoint_global.longitude;
	waypoint.z = waypoint_global.altitude;
	
	waypoint.autocontinue = 0;
	waypoint.current = 0;
	waypoint.frame = MAV_FRAME_GLOBAL;
	waypoint.waypoint_id = MAV_CMD_NAV_WAYPOINT;
	
	waypoint.param1 = 10; // Hold time in decimal seconds
	waypoint.param2 = 4; // Acceptance radius in meters
	waypoint.param3 = 0; //  0 to pass through the WP, if > 0 radius in meters to pass by WP. Positive value for clockwise orbit, negative value for counter-clockwise orbit. Allows trajectory control.
	waypoint.param4 = rad_to_deg(angle_step * (waypoint_handler->mavlink_stream->sysid-1)); // Desired yaw angle at MISSION (rotary wing)
	
	waypoint_handler->waypoint_list[1] = waypoint;
	
	waypoint_handler->state->nav_plan_active = false;

	mavlink_message_t msg;
	mavlink_msg_command_ack_pack(waypoint_handler->mavlink_stream->sysid,
								waypoint_handler->mavlink_stream->compid,
								&msg, 
								MAV_CMD_CONDITION_LAST, 
								MAV_RESULT_ACCEPTED);
	mavlink_stream_send(waypoint_handler->mavlink_stream, &msg);
}

/*
void static set_stream_scenario(waypoint_struct waypoint_list[], uint16_t* number_of_waypoints, float circle_radius, float num_of_vhc)
{
	waypoint_struct waypoint;
	
	// TODO: Add code here :)
}
*/

static void waypoint_handler_send_count(mavlink_waypoint_handler_t* waypoint_handler, mavlink_received_t* rec)
{
	mavlink_communication_suspend_downstream(waypoint_handler->mavlink_communication,500000);
	
	mavlink_mission_request_list_t packet;
	
	mavlink_msg_mission_request_list_decode(&rec->msg,&packet);
	
	// Check if this message is for this system and subsystem
	if (((uint8_t)packet.target_system == (uint8_t)waypoint_handler->mavlink_stream->sysid)
	&& ((uint8_t)packet.target_component == (uint8_t)MAV_COMP_ID_MISSIONPLANNER))
	{
		mavlink_message_t msg;
		mavlink_msg_mission_count_pack(	waypoint_handler->mavlink_stream->sysid,
										waypoint_handler->mavlink_stream->compid,
										&msg,
										rec->msg.sysid,
										rec->msg.compid, 
										waypoint_handler->number_of_waypoints);
		mavlink_stream_send(waypoint_handler->mavlink_stream, &msg);
		
		if (waypoint_handler->number_of_waypoints != 0)
		{
			waypoint_handler->waypoint_sending = true;
			waypoint_handler->waypoint_receiving = false;
			waypoint_handler->start_timeout = time_keeper_get_millis();
		}
		
		waypoint_handler->sending_waypoint_num = 0;
		print_util_dbg_print("Will send ");
		print_util_dbg_print_num(waypoint_handler->number_of_waypoints,10);
		print_util_dbg_print(" waypoints\n");
	}
}

static void waypoint_handler_send_waypoint(mavlink_waypoint_handler_t* waypoint_handler, mavlink_received_t* rec)
{
	if (waypoint_handler->waypoint_sending)
	{
		mavlink_mission_request_t packet;
		
		mavlink_msg_mission_request_decode(&rec->msg,&packet);
		
		print_util_dbg_print("Asking for waypoint number ");
		print_util_dbg_print_num(packet.seq,10);
		print_util_dbg_print("\n");
		
		// Check if this message is for this system and subsystem
		if (((uint8_t)packet.target_system == (uint8_t)waypoint_handler->mavlink_stream->sysid)
		&& ((uint8_t)packet.target_component == (uint8_t)MAV_COMP_ID_MISSIONPLANNER))
		{
			waypoint_handler->sending_waypoint_num = packet.seq;
			if (waypoint_handler->sending_waypoint_num < waypoint_handler->number_of_waypoints)
			{
				//	Prototype of the function "mavlink_msg_mission_item_send" found in mavlink_msg_mission_item.h :
				// mavlink_msg_mission_item_send (	mavlink_channel_t chan, uint8_t target_system, uint8_t target_component, uint16_t seq,
				//									uint8_t frame, uint16_t command, uint8_t current, uint8_t autocontinue, float param1,
				//									float param2, float param3, float param4, float x, float y, float z)
				mavlink_message_t msg;
				mavlink_msg_mission_item_pack(	waypoint_handler->mavlink_stream->sysid,
												waypoint_handler->mavlink_stream->compid,
												&msg,
												rec->msg.sysid, 
												rec->msg.compid, 
												packet.seq,
												waypoint_handler->waypoint_list[waypoint_handler->sending_waypoint_num].frame,	
												waypoint_handler->waypoint_list[waypoint_handler->sending_waypoint_num].waypoint_id,
												waypoint_handler->waypoint_list[waypoint_handler->sending_waypoint_num].current,	
												waypoint_handler->waypoint_list[waypoint_handler->sending_waypoint_num].autocontinue,
												waypoint_handler->waypoint_list[waypoint_handler->sending_waypoint_num].param1,	
												waypoint_handler->waypoint_list[waypoint_handler->sending_waypoint_num].param2,
												waypoint_handler->waypoint_list[waypoint_handler->sending_waypoint_num].param3,	
												waypoint_handler->waypoint_list[waypoint_handler->sending_waypoint_num].param4,
												waypoint_handler->waypoint_list[waypoint_handler->sending_waypoint_num].x,		
												waypoint_handler->waypoint_list[waypoint_handler->sending_waypoint_num].y,
												waypoint_handler->waypoint_list[waypoint_handler->sending_waypoint_num].z);
				mavlink_stream_send(waypoint_handler->mavlink_stream, &msg);
										
				print_util_dbg_print("Sending waypoint ");
				print_util_dbg_print_num(waypoint_handler->sending_waypoint_num, 10);
				print_util_dbg_print("\n");
				
				waypoint_handler->start_timeout = time_keeper_get_millis();
			}
		}
	}
}

static void waypoint_handler_receive_ack_msg(mavlink_waypoint_handler_t* waypoint_handler, mavlink_received_t* rec)
{
	mavlink_mission_ack_t packet;
	
	mavlink_msg_mission_ack_decode(&rec->msg, &packet);
	
	// Check if this message is for this system and subsystem
	if (((uint8_t)packet.target_system == (uint8_t)waypoint_handler->mavlink_stream->sysid)
	&& ((uint8_t)packet.target_component == (uint8_t)MAV_COMP_ID_MISSIONPLANNER))
	{
		waypoint_handler->waypoint_sending = false;
		waypoint_handler->sending_waypoint_num = 0;
		print_util_dbg_print("Acknowledgment received, end of waypoint sending.\n");
	}
}

static void waypoint_handler_receive_count(mavlink_waypoint_handler_t* waypoint_handler, mavlink_received_t* rec)
{
	mavlink_communication_suspend_downstream(waypoint_handler->mavlink_communication,500000);
	
	mavlink_mission_count_t packet;
	
	mavlink_msg_mission_count_decode(&rec->msg, &packet);
	
	print_util_dbg_print("Count:");
	print_util_dbg_print_num(packet.count,10);
	print_util_dbg_print("\n");
	
	// Check if this message is for this system and subsystem
	if (((uint8_t)packet.target_system == (uint8_t)waypoint_handler->mavlink_stream->sysid)
	&& ((uint8_t)packet.target_component == (uint8_t)MAV_COMP_ID_MISSIONPLANNER))
	{
		if (waypoint_handler->waypoint_receiving == false)
		{
			// comment these lines if you want to add new waypoints to the list instead of overwriting them
			waypoint_handler->num_waypoint_onboard = 0;
			waypoint_handler->number_of_waypoints =0;
			//---//
			
			if ((packet.count + waypoint_handler->number_of_waypoints) > MAX_WAYPOINTS)
			{
				packet.count = MAX_WAYPOINTS - waypoint_handler->number_of_waypoints;
			}
			waypoint_handler->number_of_waypoints =  packet.count + waypoint_handler->number_of_waypoints;
			print_util_dbg_print("Receiving ");
			print_util_dbg_print_num(packet.count,10);
			print_util_dbg_print(" new waypoints. ");
			print_util_dbg_print("New total number of waypoints:");
			print_util_dbg_print_num(waypoint_handler->number_of_waypoints,10);
			print_util_dbg_print("\n");
			
			waypoint_handler->waypoint_receiving   = true;
			waypoint_handler->waypoint_sending     = false;
			waypoint_handler->waypoint_request_number = 0;
			
			
			waypoint_handler->start_timeout = time_keeper_get_millis();
		}
		
		mavlink_message_t msg;
		mavlink_msg_mission_request_pack(	waypoint_handler->mavlink_stream->sysid,
											waypoint_handler->mavlink_stream->compid, 
											&msg,
											rec->msg.sysid,
											rec->msg.compid,
											waypoint_handler->waypoint_request_number);
		mavlink_stream_send(waypoint_handler->mavlink_stream, &msg);
		
		print_util_dbg_print("Asking for waypoint ");
		print_util_dbg_print_num(waypoint_handler->waypoint_request_number,10);
		print_util_dbg_print("\n");
	}
	
}

static void waypoint_handler_receive_waypoint(mavlink_waypoint_handler_t* waypoint_handler, mavlink_received_t* rec)
{
	mavlink_communication_suspend_downstream(waypoint_handler->mavlink_communication,500000);
	
	mavlink_mission_item_t packet;
	
	mavlink_msg_mission_item_decode(&rec->msg,&packet);
	
	// Check if this message is for this system and subsystem
	if (((uint8_t)packet.target_system == (uint8_t)waypoint_handler->mavlink_stream->sysid)
	&& ((uint8_t)packet.target_component == (uint8_t)MAV_COMP_ID_MISSIONPLANNER))
	{
		waypoint_handler->start_timeout = time_keeper_get_millis();
		
		waypoint_struct new_waypoint;
		
		new_waypoint.waypoint_id = packet.command;
		
		new_waypoint.x = packet.x; // longitude
		new_waypoint.y = packet.y; // latitude
		new_waypoint.z = packet.z; // altitude
		
		new_waypoint.autocontinue = packet.autocontinue;
		new_waypoint.frame = packet.frame;
		
		new_waypoint.current = packet.current;
		
		new_waypoint.param1 = packet.param1;
		new_waypoint.param2 = packet.param2;
		new_waypoint.param3 = packet.param3;
		new_waypoint.param4 = packet.param4;
		
		print_util_dbg_print("New waypoint received ");
		//print_util_dbg_print("(");
		//print_util_dbg_print_num(new_waypoint.x,10);
		//print_util_dbg_print(", ");
		//print_util_dbg_print_num(new_waypoint.y,10);
		//print_util_dbg_print(", ");
		//print_util_dbg_print_num(new_waypoint.z,10);
		//print_util_dbg_print(") Autocontinue:");
		//print_util_dbg_print_num(new_waypoint.autocontinue,10);
		//print_util_dbg_print(" Frame:");
		//print_util_dbg_print_num(new_waypoint.frame,10);
		//print_util_dbg_print(" Current :");
		//print_util_dbg_print_num(packet.current,10);
		//print_util_dbg_print(" Seq :");
		//print_util_dbg_print_num(packet.seq,10);
		//print_util_dbg_print(" command id :");
		//print_util_dbg_print_num(packet.command,10);
		print_util_dbg_print(" requested num :");
		print_util_dbg_print_num(waypoint_handler->waypoint_request_number,10);
		print_util_dbg_print(" receiving num :");
		print_util_dbg_print_num(packet.seq,10);
		//print_util_dbg_print(" is it receiving :");
		//print_util_dbg_print_num(waypoint_handler->waypoint_receiving,10); // boolean value
		print_util_dbg_print("\n");
		
		//current = 2 is a flag to tell us this is a "guided mode" waypoint and not for the mission
		if(packet.current == 2)
		{
			// verify we received the command;
			mavlink_message_t msg;
			mavlink_msg_mission_ack_pack(	waypoint_handler->mavlink_stream->sysid,
											waypoint_handler->mavlink_stream->compid,
											&msg,
											rec->msg.sysid,
											rec->msg.compid,
											MAV_CMD_ACK_ERR_NOT_SUPPORTED);
			mavlink_stream_send(waypoint_handler->mavlink_stream, &msg);
		}
		else
		{
			//current = 3 is a flag to tell us this is a alt change only
			if(packet.current == 3)
			{
				// verify we received the command
				mavlink_message_t msg;
				mavlink_msg_mission_ack_pack(	waypoint_handler->mavlink_stream->sysid,
												waypoint_handler->mavlink_stream->compid,
												&msg,
												rec->msg.sysid,
												rec->msg.compid, 
												MAV_CMD_ACK_ERR_NOT_SUPPORTED);
				mavlink_stream_send(waypoint_handler->mavlink_stream, &msg);
			}
			else
			{
				// Check if receiving waypoints
				if (waypoint_handler->waypoint_receiving)
				{

					// check if this is the requested waypoint
					if (packet.seq == waypoint_handler->waypoint_request_number)
					{
						print_util_dbg_print("Receiving good waypoint, number ");
						print_util_dbg_print_num(waypoint_handler->waypoint_request_number,10);
						print_util_dbg_print(" of ");
						print_util_dbg_print_num(waypoint_handler->number_of_waypoints - waypoint_handler->num_waypoint_onboard,10);
						print_util_dbg_print("\n");
						
						waypoint_handler->waypoint_list[waypoint_handler->num_waypoint_onboard + waypoint_handler->waypoint_request_number] = new_waypoint;
						waypoint_handler->waypoint_request_number++;
						
						if ((waypoint_handler->num_waypoint_onboard + waypoint_handler->waypoint_request_number) == waypoint_handler->number_of_waypoints)
						{
							
							uint8_t type = MAV_CMD_ACK_OK;	//MAV_CMD_ACK_ERR_FAIL;
							
							mavlink_message_t msg;
							mavlink_msg_mission_ack_pack( waypoint_handler->mavlink_stream->sysid,
															waypoint_handler->mavlink_stream->compid,
															&msg,
															rec->msg.sysid,
															rec->msg.compid,type);
							mavlink_stream_send(waypoint_handler->mavlink_stream, &msg);
							
							print_util_dbg_print("flight plan received!\n");
							waypoint_handler->waypoint_receiving = false;
							waypoint_handler->num_waypoint_onboard = waypoint_handler->number_of_waypoints;
							waypoint_handler->state->nav_plan_active = false;
							waypoint_handler_nav_plan_init(waypoint_handler);
						}
						else
						{
							mavlink_message_t msg;
							mavlink_msg_mission_request_pack( 	waypoint_handler->mavlink_stream->sysid,
																waypoint_handler->mavlink_stream->compid,
																&msg,
																rec->msg.sysid,
																rec->msg.compid,
																waypoint_handler->waypoint_request_number);
							mavlink_stream_send(waypoint_handler->mavlink_stream, &msg);
							
							print_util_dbg_print("Asking for waypoint ");
							print_util_dbg_print_num(waypoint_handler->waypoint_request_number,10);
							print_util_dbg_print("\n");
						}
					}
				}
				else
				{
					uint8_t type = MAV_CMD_ACK_OK;	//MAV_CMD_ACK_ERR_FAIL;
					print_util_dbg_print("Ack not received!");
					
					mavlink_message_t msg;
					mavlink_msg_mission_ack_pack(	waypoint_handler->mavlink_stream->sysid,
													waypoint_handler->mavlink_stream->compid,
													&msg,
													rec->msg.sysid,
													rec->msg.compid,
													type	);
					mavlink_stream_send(waypoint_handler->mavlink_stream, &msg);
				}
			}
		}
	}
}

static void waypoint_handler_set_current_waypoint(mavlink_waypoint_handler_t* waypoint_handler, mavlink_received_t* rec)
{
	mavlink_mission_set_current_t packet;
	
	mavlink_msg_mission_set_current_decode(&rec->msg,&packet);
	
	// Check if this message is for this system and subsystem
	if (((uint8_t)packet.target_system == (uint8_t)waypoint_handler->mavlink_stream->sysid)
	&& ((uint8_t)packet.target_component == (uint8_t)MAV_COMP_ID_MISSIONPLANNER))
	{
		if (packet.seq < waypoint_handler->number_of_waypoints)
		{
			int32_t i;
			for (i=0;i<waypoint_handler->number_of_waypoints;i++)
			{
				waypoint_handler->waypoint_list[i].current = 0;
			}
			
			waypoint_handler->waypoint_list[packet.seq].current = 1;
			
			mavlink_message_t msg;
			mavlink_msg_mission_current_pack( 	waypoint_handler->mavlink_stream->sysid,
												waypoint_handler->mavlink_stream->compid,
												&msg,
												packet.seq);
			mavlink_stream_send(waypoint_handler->mavlink_stream, &msg);
			
			print_util_dbg_print("Set current waypoint to number");
			print_util_dbg_print_num(packet.seq,10);
			print_util_dbg_print("\n");
			
			waypoint_handler->state->nav_plan_active = false;
			waypoint_handler_nav_plan_init(waypoint_handler);
		}
		else
		{
			mavlink_message_t msg;
			mavlink_msg_mission_ack_pack(	waypoint_handler->mavlink_stream->sysid,
											waypoint_handler->mavlink_stream->compid,
											&msg,
											rec->msg.sysid,
											rec->msg.compid,
											MAV_CMD_ACK_ERR_ACCESS_DENIED);
			mavlink_stream_send(waypoint_handler->mavlink_stream, &msg);
		}
	}
}

static void waypoint_handler_set_current_waypoint_from_parameter(mavlink_waypoint_handler_t* waypoint_handler, mavlink_command_long_t* packet)
{
	uint8_t i;
	
	uint16_t new_current = 0;
	
	print_util_dbg_print("All MAVs: Return to first waypoint. \n");
	
	if (new_current < waypoint_handler->number_of_waypoints)
	{

		for (i=0;i<waypoint_handler->number_of_waypoints;i++)
		{
			waypoint_handler->waypoint_list[i].current = 0;
		}
		waypoint_handler->waypoint_list[new_current].current = 1;
		
		mavlink_message_t msg;
		mavlink_msg_mission_current_pack( 	waypoint_handler->mavlink_stream->sysid,
											waypoint_handler->mavlink_stream->compid,
											&msg,
											new_current);
		mavlink_stream_send(waypoint_handler->mavlink_stream, &msg);
		
		print_util_dbg_print("Set current waypoint to number");
		print_util_dbg_print_num(new_current,10);
		print_util_dbg_print("\n");
		
		waypoint_handler->state->nav_plan_active = false;
		waypoint_handler_nav_plan_init(waypoint_handler);

		mavlink_msg_command_ack_pack( 		waypoint_handler->mavlink_stream->sysid,
											waypoint_handler->mavlink_stream->compid,
											&msg, 
											MAV_CMD_NAV_RETURN_TO_LAUNCH,
											MAV_RESULT_ACCEPTED);
		mavlink_stream_send(waypoint_handler->mavlink_stream, &msg);
	}
	else
	{
		mavlink_message_t msg;
		mavlink_msg_command_ack_pack( 	waypoint_handler->mavlink_stream->sysid,
										waypoint_handler->mavlink_stream->compid,
										&msg, 
										MAV_CMD_NAV_RETURN_TO_LAUNCH, 
										MAV_RESULT_DENIED);
		mavlink_stream_send(waypoint_handler->mavlink_stream, &msg);
		// mavlink_msg_mission_ack_send(MAVLINK_COMM_0,mavlink_system.sysid,MAV_COMP_ID_MISSIONPLANNER,MAV_CMD_ACK_ERR_NOT_SUPPORTED);
	}
}

static void waypoint_handler_clear_waypoint_list(mavlink_waypoint_handler_t* waypoint_handler, mavlink_received_t* rec)
{
	mavlink_mission_clear_all_t packet;
	
	mavlink_msg_mission_clear_all_decode(&rec->msg,&packet);
	
	// Check if this message is for this system and subsystem
	if (((uint8_t)packet.target_system == (uint8_t)waypoint_handler->mavlink_stream->sysid)
	&& ((uint8_t)packet.target_component == (uint8_t)MAV_COMP_ID_MISSIONPLANNER))
	{
		if (waypoint_handler->number_of_waypoints > 0)
		{
			waypoint_handler->number_of_waypoints = 0;
			waypoint_handler->num_waypoint_onboard = 0;
			waypoint_handler->state->nav_plan_active = 0;
			//navigation_waypoint_hold_init(waypoint_handler, waypoint_handler->position_estimator->local_position);
			waypoint_handler->state->nav_plan_active = false;
			waypoint_handler->hold_waypoint_set = false;
		
			mavlink_message_t msg;
			mavlink_msg_mission_ack_pack( 	waypoint_handler->mavlink_stream->sysid,
											waypoint_handler->mavlink_stream->compid,
											&msg,
											rec->msg.sysid,
											rec->msg.compid,
											MAV_CMD_ACK_OK);
			mavlink_stream_send(waypoint_handler->mavlink_stream, &msg);
								
			print_util_dbg_print("Cleared Waypoint list.\n");
		}
	}
}

static void waypoint_handler_set_home(mavlink_waypoint_handler_t* waypoint_handler, mavlink_received_t* rec)
{
	mavlink_set_gps_global_origin_t packet;
	
	mavlink_msg_set_gps_global_origin_decode(&rec->msg,&packet);
	
	// Check if this message is for this system and subsystem
	// Due to possible bug from QGroundControl, no check of target_component and compid
	if ((uint8_t)packet.target_system == (uint8_t)waypoint_handler->mavlink_stream->sysid)
	{
		print_util_dbg_print("Set new home location.\n");
		waypoint_handler->position_estimator->local_position.origin.latitude = (double) packet.latitude / 10000000.0f;
		waypoint_handler->position_estimator->local_position.origin.longitude = (double) packet.longitude / 10000000.0f;
		waypoint_handler->position_estimator->local_position.origin.altitude = (float) packet.altitude / 1000.0f;
		
		print_util_dbg_print("New Home location: (");
		print_util_dbg_print_num(waypoint_handler->position_estimator->local_position.origin.latitude*10000000.0f,10);
		print_util_dbg_print(", ");
		print_util_dbg_print_num(waypoint_handler->position_estimator->local_position.origin.longitude*10000000.0f,10);
		print_util_dbg_print(", ");
		print_util_dbg_print_num(waypoint_handler->position_estimator->local_position.origin.altitude*1000.0f,10);
		print_util_dbg_print(")\n");
		
		mavlink_message_t msg;
		mavlink_msg_gps_global_origin_pack( waypoint_handler->mavlink_stream->sysid,
											waypoint_handler->mavlink_stream->compid,
											&msg,
											waypoint_handler->position_estimator->local_position.origin.latitude*10000000.0f,
											waypoint_handler->position_estimator->local_position.origin.longitude*10000000.0f,
											waypoint_handler->position_estimator->local_position.origin.altitude*1000.0f);
		mavlink_stream_send(waypoint_handler->mavlink_stream, &msg);
	}
}

static void waypoint_handler_auto_landing(mavlink_waypoint_handler_t* waypoint_handler, mavlink_command_long_t* packet)
{
	// TODO: implement this! Separate message receiving from handling auto-landing procedure
	float rel_pos[3];
	uint8_t i;
	
	local_coordinates_t local_position;
	
	print_util_dbg_print("Auto-landing procedure initialised.\n");
	
	switch(waypoint_handler->auto_landing_behavior)
	{
		case DESCENT_TO_SMALL_ALTITUDE:
			local_position = waypoint_handler->position_estimator->local_position;
			local_position.pos[Z] = -2.0f;
			
			//navigation_waypoint_hold_init(waypoint_handler, local_position);
			break;
		case DESCENT_TO_GND:
			local_position = waypoint_handler->position_estimator->local_position;
			local_position.pos[Z] = 0.0f;
		
			// TODO: replace this function
			//navigation_waypoint_hold_init(waypoint_handler, local_position);
			break;
	}
	
	for (i=0;i<3;i++)
	{
		rel_pos[i] = waypoint_handler->waypoint_critical_coordinates.pos[i] - waypoint_handler->position_estimator->local_position.pos[i];
	}
	waypoint_handler->dist2wp_sqr = vectors_norm_sqr(rel_pos);
	
	if (waypoint_handler->dist2wp_sqr < 0.5f)
	{
		switch(waypoint_handler->auto_landing_behavior)
		{
			case DESCENT_TO_SMALL_ALTITUDE:
				print_util_dbg_print("Automatic-landing: descent_to_GND\n");
				waypoint_handler->auto_landing_behavior = DESCENT_TO_GND;
				break;
			case DESCENT_TO_GND:
				break;
		}
	}
	
	mavlink_message_t msg;
	mavlink_msg_command_ack_pack( 	waypoint_handler->mavlink_stream->sysid,
									waypoint_handler->mavlink_stream->compid,
									&msg, 
									MAV_CMD_NAV_LAND, 
									MAV_RESULT_ACCEPTED);
	mavlink_stream_send(waypoint_handler->mavlink_stream, &msg);
}

static void waypoint_handler_continue_to_next_waypoint(mavlink_waypoint_handler_t* waypoint_handler, mavlink_command_long_t* packet)
{
	print_util_dbg_print("All vehicles: Navigating to next waypoint. \n");
	
	if ((waypoint_handler->number_of_waypoints>0)&&(!waypoint_handler->state->nav_plan_active))
	{
		waypoint_handler->waypoint_list[waypoint_handler->current_waypoint_count].current = 0;
		
		print_util_dbg_print("Continuing towards waypoint Nr");
		
		if (waypoint_handler->current_waypoint_count == (waypoint_handler->number_of_waypoints-1))
		{
			waypoint_handler->current_waypoint_count = 0;
		}
		else
		{
			waypoint_handler->current_waypoint_count++;
		}
		print_util_dbg_print_num(waypoint_handler->current_waypoint_count,10);
		print_util_dbg_print("\n");
		waypoint_handler->waypoint_list[waypoint_handler->current_waypoint_count].current = 1;
		waypoint_handler->current_waypoint = waypoint_handler->waypoint_list[waypoint_handler->current_waypoint_count];
		waypoint_handler->waypoint_coordinates = waypoint_handler_set_waypoint_from_frame(waypoint_handler, waypoint_handler->position_estimator->local_position.origin);
			
		mavlink_message_t msg;
		mavlink_msg_mission_current_pack( 	waypoint_handler->mavlink_stream->sysid,
											waypoint_handler->mavlink_stream->compid,
											&msg, 
											waypoint_handler->current_waypoint_count);
		mavlink_stream_send(waypoint_handler->mavlink_stream, &msg);
		
		waypoint_handler->state->nav_plan_active = true;

		mavlink_msg_command_ack_pack( 	waypoint_handler->mavlink_stream->sysid,
										waypoint_handler->mavlink_stream->compid,
										&msg,  
										MAV_CMD_MISSION_START, 
										MAV_RESULT_ACCEPTED);
		mavlink_stream_send(waypoint_handler->mavlink_stream, &msg);
	}
	else
	{
		mavlink_message_t msg;
		mavlink_msg_command_ack_pack( 	waypoint_handler->mavlink_stream->sysid,
										waypoint_handler->mavlink_stream->compid,
										&msg,  
										MAV_CMD_MISSION_START, 
										MAV_RESULT_TEMPORARILY_REJECTED);
		mavlink_stream_send(waypoint_handler->mavlink_stream, &msg);
		
		print_util_dbg_print("Not ready to switch to next waypoint. Either no waypoint loaded or flying towards one\n");
	}
}

void waypoint_handler_set_auto_takeoff(mavlink_waypoint_handler_t *waypoint_handler, mavlink_command_long_t* packet)
{
	print_util_dbg_print("Starting automatic take-off from button\n");
	waypoint_handler->state->in_the_air = true;

	mavlink_message_t msg;
	mavlink_msg_command_ack_pack( 	waypoint_handler->mavlink_stream->sysid,
									waypoint_handler->mavlink_stream->compid,
									&msg, 
									MAV_CMD_NAV_TAKEOFF, 
									MAV_RESULT_ACCEPTED);
	mavlink_stream_send(waypoint_handler->mavlink_stream, &msg);
}

//------------------------------------------------------------------------------
// PUBLIC FUNCTIONS IMPLEMENTATION
//------------------------------------------------------------------------------

void waypoint_handler_init(mavlink_waypoint_handler_t* waypoint_handler, position_estimator_t* position_estimator, const ahrs_t* ahrs, state_t* state, mavlink_communication_t* mavlink_communication, const mavlink_stream_t* mavlink_stream)
{
	waypoint_handler->start_timeout = time_keeper_get_millis();
	waypoint_handler->timeout_max_waypoint = 10000;
	
	waypoint_handler->position_estimator = position_estimator;
	waypoint_handler->ahrs = ahrs;
	waypoint_handler->state = state;
	
	waypoint_handler->mavlink_communication = mavlink_communication;
	waypoint_handler->mavlink_stream = mavlink_stream;

	// init waypoint navigation
	waypoint_handler->number_of_waypoints = 0;
	waypoint_handler->num_waypoint_onboard = 0;
	
	waypoint_handler->sending_waypoint_num = 0;
	waypoint_handler->waypoint_request_number = 0;
	
	waypoint_handler->hold_waypoint_set = false;
	
	waypoint_handler->critical_behavior = CLIMB_TO_SAFE_ALT;
	waypoint_handler->auto_landing_behavior = DESCENT_TO_SMALL_ALTITUDE;
	waypoint_handler->critical_landing = false;
	waypoint_handler->automatic_landing = false;
	
	waypoint_handler->waypoint_sending = false;
	waypoint_handler->waypoint_receiving = false;
	
	// Add callbacks for waypoint handler messages requests
	mavlink_message_handler_msg_callback_t callback;

	callback.message_id 	= MAVLINK_MSG_ID_MISSION_ITEM; // 39
	callback.sysid_filter 	= MAVLINK_BASE_STATION_ID;
	callback.compid_filter 	= MAV_COMP_ID_ALL;
	callback.function 		= (mavlink_msg_callback_function_t)	&waypoint_handler_receive_waypoint;
	callback.module_struct 	= (handling_module_struct_t)		waypoint_handler;
	mavlink_message_handler_add_msg_callback( &mavlink_communication->message_handler, &callback );
	
	callback.message_id 	= MAVLINK_MSG_ID_MISSION_REQUEST; // 40
	callback.sysid_filter 	= MAVLINK_BASE_STATION_ID;
	callback.compid_filter 	= MAV_COMP_ID_ALL;
	callback.function 		= (mavlink_msg_callback_function_t)	&waypoint_handler_send_waypoint;
	callback.module_struct 	= (handling_module_struct_t)		waypoint_handler;
	mavlink_message_handler_add_msg_callback( &mavlink_communication->message_handler, &callback );
	
	callback.message_id 	= MAVLINK_MSG_ID_MISSION_SET_CURRENT; // 41
	callback.sysid_filter 	= MAVLINK_BASE_STATION_ID;
	callback.compid_filter 	= MAV_COMP_ID_ALL;
	callback.function 		= (mavlink_msg_callback_function_t)	&waypoint_handler_set_current_waypoint;
	callback.module_struct 	= (handling_module_struct_t)		waypoint_handler;
	mavlink_message_handler_add_msg_callback( &mavlink_communication->message_handler, &callback );
	
	callback.message_id 	= MAVLINK_MSG_ID_MISSION_REQUEST_LIST; // 43
	callback.sysid_filter 	= MAVLINK_BASE_STATION_ID;
	callback.compid_filter 	= MAV_COMP_ID_ALL;
	callback.function 		= (mavlink_msg_callback_function_t)	&waypoint_handler_send_count;
	callback.module_struct 	= (handling_module_struct_t)		waypoint_handler;
	mavlink_message_handler_add_msg_callback( &mavlink_communication->message_handler, &callback );
	
	callback.message_id 	= MAVLINK_MSG_ID_MISSION_COUNT; // 44
	callback.sysid_filter 	= MAVLINK_BASE_STATION_ID;
	callback.compid_filter 	= MAV_COMP_ID_ALL;
	callback.function 		= (mavlink_msg_callback_function_t)	&waypoint_handler_receive_count;
	callback.module_struct 	= (handling_module_struct_t)		waypoint_handler;
	mavlink_message_handler_add_msg_callback( &mavlink_communication->message_handler, &callback );
	
	callback.message_id 	= MAVLINK_MSG_ID_MISSION_CLEAR_ALL; // 45
	callback.sysid_filter 	= MAVLINK_BASE_STATION_ID;
	callback.compid_filter 	= MAV_COMP_ID_ALL;
	callback.function 		= (mavlink_msg_callback_function_t)	&waypoint_handler_clear_waypoint_list;
	callback.module_struct 	= (handling_module_struct_t)		waypoint_handler;
	mavlink_message_handler_add_msg_callback( &mavlink_communication->message_handler, &callback );
	
	callback.message_id 	= MAVLINK_MSG_ID_MISSION_ACK; // 47
	callback.sysid_filter 	= MAVLINK_BASE_STATION_ID;
	callback.compid_filter 	= MAV_COMP_ID_ALL;
	callback.function 		= (mavlink_msg_callback_function_t)	&waypoint_handler_receive_ack_msg;
	callback.module_struct 	= (handling_module_struct_t)		waypoint_handler;
	mavlink_message_handler_add_msg_callback( &mavlink_communication->message_handler, &callback );
	
	callback.message_id 	= MAVLINK_MSG_ID_SET_GPS_GLOBAL_ORIGIN; // 48
	callback.sysid_filter 	= MAVLINK_BASE_STATION_ID;
	callback.compid_filter 	= MAV_COMP_ID_ALL;
	callback.function 		= (mavlink_msg_callback_function_t)	&waypoint_handler_set_home;
	callback.module_struct 	= (handling_module_struct_t)		waypoint_handler;
	mavlink_message_handler_add_msg_callback( &mavlink_communication->message_handler, &callback );
	
	// Add callbacks for waypoint handler commands requests
	mavlink_message_handler_cmd_callback_t callbackcmd;
	
	callbackcmd.command_id = MAV_CMD_NAV_RETURN_TO_LAUNCH; // 20
	callbackcmd.sysid_filter = MAVLINK_BASE_STATION_ID;
	callbackcmd.compid_filter = MAV_COMP_ID_ALL;
	callbackcmd.compid_target = MAV_COMP_ID_MISSIONPLANNER; // 190
	callbackcmd.function = (mavlink_cmd_callback_function_t)	&waypoint_handler_set_current_waypoint_from_parameter;
	callbackcmd.module_struct =									waypoint_handler;
	mavlink_message_handler_add_cmd_callback(&mavlink_communication->message_handler, &callbackcmd);
	
	callbackcmd.command_id = MAV_CMD_MISSION_START; // 300
	callbackcmd.sysid_filter = MAVLINK_BASE_STATION_ID;
	callbackcmd.compid_filter = MAV_COMP_ID_ALL;
	callbackcmd.compid_target = MAV_COMP_ID_MISSIONPLANNER; // 190
	callbackcmd.function = (mavlink_cmd_callback_function_t)	&waypoint_handler_continue_to_next_waypoint;
	callbackcmd.module_struct =									waypoint_handler;
	mavlink_message_handler_add_cmd_callback(&mavlink_communication->message_handler, &callbackcmd);
	
	callbackcmd.command_id = MAV_CMD_CONDITION_LAST; // 159
	callbackcmd.sysid_filter = MAVLINK_BASE_STATION_ID;
	callbackcmd.compid_filter = MAV_COMP_ID_ALL;
	callbackcmd.compid_target = MAV_COMP_ID_MISSIONPLANNER; // 190
	callbackcmd.function = (mavlink_cmd_callback_function_t)	&waypoint_handler_set_circle_scenario;
	callbackcmd.module_struct =									waypoint_handler;
	mavlink_message_handler_add_cmd_callback(&mavlink_communication->message_handler, &callbackcmd);
	
	callbackcmd.command_id = MAV_CMD_NAV_TAKEOFF; // 22
	callbackcmd.sysid_filter = MAVLINK_BASE_STATION_ID;
	callbackcmd.compid_filter = MAV_COMP_ID_ALL;
	callbackcmd.compid_target = MAV_COMP_ID_ALL; // 0
	callbackcmd.function = (mavlink_cmd_callback_function_t)	&waypoint_handler_set_auto_takeoff;
	callbackcmd.module_struct =									waypoint_handler;
	mavlink_message_handler_add_cmd_callback(&mavlink_communication->message_handler, &callbackcmd);
	
	callbackcmd.command_id = MAV_CMD_NAV_LAND; // 21
	callbackcmd.sysid_filter = MAVLINK_BASE_STATION_ID;
	callbackcmd.compid_filter = MAV_COMP_ID_ALL;
	callbackcmd.compid_target = MAV_COMP_ID_MISSIONPLANNER; // 190
	callbackcmd.function = (mavlink_cmd_callback_function_t)	&waypoint_handler_auto_landing;
	callbackcmd.module_struct =									waypoint_handler;
	mavlink_message_handler_add_cmd_callback(&mavlink_communication->message_handler, &callbackcmd);
	
	print_util_dbg_print("Waypoint handler initialized.\n");
}

void waypoint_handler_init_homing_waypoint(mavlink_waypoint_handler_t* waypoint_handler)
{
	waypoint_struct waypoint;
	
	waypoint_handler->number_of_waypoints = 1;
	
	waypoint_handler->num_waypoint_onboard = waypoint_handler->number_of_waypoints;
	
	//Set home waypoint
	waypoint.autocontinue = 0;
	waypoint.current = 1;
	waypoint.frame = MAV_FRAME_LOCAL_NED;
	waypoint.waypoint_id = MAV_CMD_NAV_WAYPOINT;
	
	waypoint.x = 0.0f;
	waypoint.y = 0.0f;
	waypoint.z = -10.0f;
	
	waypoint.param1 = 10; // Hold time in decimal seconds
	waypoint.param2 = 2; // Acceptance radius in meters
	waypoint.param3 = 0; //  0 to pass through the WP, if > 0 radius in meters to pass by WP. Positive value for clockwise orbit, negative value for counter-clockwise orbit. Allows trajectory control.
	waypoint.param4 = 0; // Desired yaw angle at MISSION (rotary wing)
	
	waypoint_handler->waypoint_list[0] = waypoint;
}

void waypoint_handler_init_waypoint_list(mavlink_waypoint_handler_t* waypoint_handler)
{
	// Visit https://code.google.com/p/ardupilot-mega/wiki/MAVLink to have a description of all messages (or common.h)
	waypoint_struct waypoint;
	
	waypoint_handler->number_of_waypoints = 4;
	
	waypoint_handler->num_waypoint_onboard = waypoint_handler->number_of_waypoints;
	
	// Set nav waypoint
	waypoint.autocontinue = 0;
	waypoint.current = 1;
	waypoint.frame = MAV_FRAME_GLOBAL_RELATIVE_ALT;
	waypoint.waypoint_id = MAV_CMD_NAV_WAYPOINT;
	
	waypoint.x =  465185223.6174f / 1.0e7f; // convert to deg
	waypoint.y = 65670560 / 1.0e7f; // convert to deg
	waypoint.z = 20; //m
	
	waypoint.param1 = 10; // Hold time in decimal seconds
	waypoint.param2 = 2; // Acceptance radius in meters
	waypoint.param3 = 0; //  0 to pass through the WP, if > 0 radius in meters to pass by WP. Positive value for clockwise orbit, negative value for counter-clockwise orbit. Allows trajectory control.
	waypoint.param4 = 0; // Desired yaw angle at MISSION (rotary wing)
	
	waypoint_handler->waypoint_list[0] = waypoint;
	
	// Set nav waypoint
	waypoint.autocontinue = 0;
	waypoint.current = 0;
	waypoint.frame = MAV_FRAME_GLOBAL_RELATIVE_ALT;
	waypoint.waypoint_id = MAV_CMD_NAV_WAYPOINT;
	
	waypoint.x = 465186816 / 1.0e7f; // convert to deg
	waypoint.y = 65670560 / 1.0e7f; // convert to deg
	waypoint.z = 20; //m
	
	waypoint.param1 = 10; // Hold time in decimal seconds
	waypoint.param2 = 4; // Acceptance radius in meters
	waypoint.param3 = 0; //  0 to pass through the WP, if > 0 radius in meters to pass by WP. Positive value for clockwise orbit, negative value for counter-clockwise orbit. Allows trajectory control.
	waypoint.param4 = 270; // Desired yaw angle at MISSION (rotary wing)
	
	waypoint_handler->waypoint_list[1] = waypoint;
	
	// Set nav waypoint
	waypoint.autocontinue = 1;
	waypoint.current = 0;
	waypoint.frame = MAV_FRAME_GLOBAL_RELATIVE_ALT;
	waypoint.waypoint_id = MAV_CMD_NAV_WAYPOINT;
	
	waypoint.x = 465186816 / 1.0e7f; // convert to deg
	waypoint.y = 65659084 / 1.0e7f; // convert to deg
	waypoint.z = 180; //m
	
	waypoint.param1 = 10; // Hold time in decimal seconds
	waypoint.param2 = 15; // Acceptance radius in meters
	waypoint.param3 = 0; //  0 to pass through the WP, if > 0 radius in meters to pass by WP. Positive value for clockwise orbit, negative value for counter-clockwise orbit. Allows trajectory control.
	waypoint.param4 = 90; // Desired yaw angle at MISSION (rotary wing)
	
	waypoint_handler->waypoint_list[2] = waypoint;
	
	// Set nav waypoint
	waypoint.autocontinue = 0;
	waypoint.current = 0;
	waypoint.frame = MAV_FRAME_GLOBAL_RELATIVE_ALT;
	waypoint.waypoint_id = MAV_CMD_NAV_WAYPOINT;

	waypoint.x = 465182186 / 1.0e7f; // convert to deg
	waypoint.y = 65659084 / 1.0e7f; // convert to deg
	waypoint.z = 20; //m

	waypoint.param1 = 10; // Hold time in decimal seconds
	waypoint.param2 = 12; // Acceptance radius in meters
	waypoint.param3 = 0; //  0 to pass through the WP, if > 0 radius in meters to pass by WP. Positive value for clockwise orbit, negative value for counter-clockwise orbit. Allows trajectory control.
	waypoint.param4 = 90; // Desired yaw angle at MISSION (rotary wing)

	waypoint_handler->waypoint_list[3] = waypoint;
	
	print_util_dbg_print("Number of Waypoint onboard:");
	print_util_dbg_print_num(waypoint_handler->num_waypoint_onboard,10);
	print_util_dbg_print("\n");
}

void waypoint_handler_nav_plan_init(mavlink_waypoint_handler_t* waypoint_handler)
{
	uint8_t i,j;
	float rel_pos[3];
	
	if ((waypoint_handler->number_of_waypoints > 0)
	//&& (waypoint_handler->position_estimator->init_gps_position || (*waypoint_handler->simulation_mode==HIL_ON))
	&& (waypoint_handler->position_estimator->init_gps_position || state_test_if_in_flag_mode(waypoint_handler->state,MAV_MODE_FLAG_HIL_ENABLED))
	&& (waypoint_handler->waypoint_receiving == false))
	{
		for (i=0;i<waypoint_handler->number_of_waypoints;i++)
		{
			if ((waypoint_handler->waypoint_list[i].current == 1)&&(!waypoint_handler->state->nav_plan_active))
			{
				waypoint_handler->current_waypoint_count = i;
				waypoint_handler->current_waypoint = waypoint_handler->waypoint_list[waypoint_handler->current_waypoint_count];
				waypoint_handler->waypoint_coordinates = waypoint_handler_set_waypoint_from_frame(waypoint_handler, waypoint_handler->position_estimator->local_position.origin);
				
				print_util_dbg_print("Waypoint Nr");
				print_util_dbg_print_num(i,10);
				print_util_dbg_print(" set,\n");
			
				waypoint_handler->state->nav_plan_active = true;
				
				for (j=0;j<3;j++)
				{
					rel_pos[j] = waypoint_handler->waypoint_coordinates.pos[j]-waypoint_handler->position_estimator->local_position.pos[j];
				}
				waypoint_handler->dist2wp_sqr = vectors_norm_sqr(rel_pos);
			}
		}
	}
}

task_return_t waypoint_handler_control_time_out_waypoint_msg(mavlink_waypoint_handler_t* waypoint_handler)
{
	if (waypoint_handler->waypoint_sending || waypoint_handler->waypoint_receiving)
	{
		uint32_t tnow = time_keeper_get_millis();
		
		if ((tnow - waypoint_handler->start_timeout) > waypoint_handler->timeout_max_waypoint)
		{
			waypoint_handler->start_timeout = tnow;
			if (waypoint_handler->waypoint_sending)
			{
				waypoint_handler->waypoint_sending = false;
				print_util_dbg_print("Sending waypoint timeout");
			}
			if (waypoint_handler->waypoint_receiving)
			{
				waypoint_handler->waypoint_receiving = false;
				
				print_util_dbg_print("Receiving waypoint timeout");
				waypoint_handler->number_of_waypoints = 0;
				waypoint_handler->num_waypoint_onboard = 0;
			}
		}
	}
	return TASK_RUN_SUCCESS;
}

local_coordinates_t waypoint_handler_set_waypoint_from_frame(mavlink_waypoint_handler_t* waypoint_handler, global_position_t origin)
{
	uint8_t i;
	
	global_position_t waypoint_global;
	local_coordinates_t waypoint_coor;
	
	for (i=0;i<3;i++)
	{
		waypoint_coor.pos[i] = 0.0f;
	}
	waypoint_coor.origin = origin;
	waypoint_coor.heading = deg_to_rad(waypoint_handler->current_waypoint.param4);
	waypoint_coor.timestamp_ms = time_keeper_get_millis();

	switch(waypoint_handler->current_waypoint.frame)
	{
		case MAV_FRAME_GLOBAL:
		waypoint_global.latitude = waypoint_handler->current_waypoint.x;
		waypoint_global.longitude = waypoint_handler->current_waypoint.y;
		waypoint_global.altitude = waypoint_handler->current_waypoint.z;
		waypoint_coor = coord_conventions_global_to_local_position(waypoint_global,origin);
		
		waypoint_coor.heading = deg_to_rad(waypoint_handler->current_waypoint.param4);
		
		print_util_dbg_print("waypoint_global: lat (x1e7):");
		print_util_dbg_print_num(waypoint_global.latitude*10000000,10);
		print_util_dbg_print(" long (x1e7):");
		print_util_dbg_print_num(waypoint_global.longitude*10000000,10);
		print_util_dbg_print(" alt (x1000):");
		print_util_dbg_print_num(waypoint_global.altitude*1000,10);
		print_util_dbg_print(" waypoint_coor: x (x100):");
		print_util_dbg_print_num(waypoint_coor.pos[X]*100,10);
		print_util_dbg_print(", y (x100):");
		print_util_dbg_print_num(waypoint_coor.pos[Y]*100,10);
		print_util_dbg_print(", z (x100):");
		print_util_dbg_print_num(waypoint_coor.pos[Z]*100,10);
		print_util_dbg_print(" localOrigin lat (x1e7):");
		print_util_dbg_print_num(origin.latitude*10000000,10);
		print_util_dbg_print(" long (x1e7):");
		print_util_dbg_print_num(origin.longitude*10000000,10);
		print_util_dbg_print(" alt (x1000):");
		print_util_dbg_print_num(origin.altitude*1000,10);
		print_util_dbg_print("\n");
		break;
		
		case MAV_FRAME_LOCAL_NED:
		waypoint_coor.pos[X] = waypoint_handler->current_waypoint.x;
		waypoint_coor.pos[Y] = waypoint_handler->current_waypoint.y;
		waypoint_coor.pos[Z] = waypoint_handler->current_waypoint.z;
		waypoint_coor.heading= deg_to_rad(waypoint_handler->current_waypoint.param4);
		waypoint_coor.origin = coord_conventions_local_to_global_position(waypoint_coor);
		break;
		
		case MAV_FRAME_MISSION:
		// Problem here: rec is not defined here
		//mavlink_msg_mission_ack_send(MAVLINK_COMM_0,rec->msg.sysid,rec->msg.compid,MAV_CMD_ACK_ERR_NOT_SUPPORTED);
		break;
		case MAV_FRAME_GLOBAL_RELATIVE_ALT:
		waypoint_global.latitude = waypoint_handler->current_waypoint.x;
		waypoint_global.longitude = waypoint_handler->current_waypoint.y;
		waypoint_global.altitude = waypoint_handler->current_waypoint.z;
		
		global_position_t origin_relative_alt = origin;
		origin_relative_alt.altitude = 0.0f;
		waypoint_coor = coord_conventions_global_to_local_position(waypoint_global,origin_relative_alt);
		
		waypoint_coor.heading = deg_to_rad(waypoint_handler->current_waypoint.param4);
		
		print_util_dbg_print("LocalOrigin: lat (x1e7):");
		print_util_dbg_print_num(origin_relative_alt.latitude * 10000000,10);
		print_util_dbg_print(" long (x1e7):");
		print_util_dbg_print_num(origin_relative_alt.longitude * 10000000,10);
		print_util_dbg_print(" global alt (x1000):");
		print_util_dbg_print_num(origin.altitude*1000,10);
		print_util_dbg_print(" waypoint_coor: x (x100):");
		print_util_dbg_print_num(waypoint_coor.pos[X]*100,10);
		print_util_dbg_print(", y (x100):");
		print_util_dbg_print_num(waypoint_coor.pos[Y]*100,10);
		print_util_dbg_print(", z (x100):");
		print_util_dbg_print_num(waypoint_coor.pos[Z]*100,10);
		print_util_dbg_print("\n");
		
		break;
		case MAV_FRAME_LOCAL_ENU:
		// Problem here: rec is not defined here
		//mavlink_msg_mission_ack_send(MAVLINK_COMM_0,rec->msg.sysid,rec->msg.compid,MAV_CMD_ACK_ERR_NOT_SUPPORTED);
		break;
		
	}
	
	return waypoint_coor;
}