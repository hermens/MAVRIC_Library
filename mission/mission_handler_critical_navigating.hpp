/*******************************************************************************
 * Copyright (c) 2009-2016, MAV'RIC Development Team
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 ******************************************************************************/

/*******************************************************************************
 * \file mission_handler_critical_navigating.hpp
 *
 * \author MAV'RIC Team
 * \author Matthew Douglas
 *
 * \brief The MAVLink mission planner handler for the critical navigating state
 *
 ******************************************************************************/


#ifndef MISSION_HANDLER_CRITICAL_NAVIGATING__
#define MISSION_HANDLER_CRITICAL_NAVIGATING__

#include "mission/mission_handler_navigating.hpp"

/*
 * The handler class takes in a template parameter that allows control inputs.
 */
template <class T>
class Mission_handler_critical_navigating : public Mission_handler_navigating<T>
{
public:


    /**
     * \brief   Initialize the navigating mission planner handler
     *
     * \param   controller                          The reference to the controller
     * \param   ins                                 The reference to the ins
     * \param   navigation                          The reference to the navigation structure
     * \param   mission_planner                     The reference to the mission planner
     * \param   mavlink_stream                      The reference to the MAVLink stream structure
     * \param   waypoint_handler                    The handler for the manual control state
     */
     Mission_handler_critical_navigating(   T& controller,
                                            const INS& ins,
                                            Navigation& navigation,
                                            const Mavlink_stream& mavlink_stream,
                                            Mavlink_waypoint_handler& waypoint_handler);

    /**
     * \brief   Checks if the waypoint is a navigating waypoint
     *  
     * \details     Checks if the inputted waypoint is a:
     *                  MAV_CMD_NAV_CRITICAL_WAYPOINT
     *
     * \param   wpt                 The waypoint class
     *
     * \return  Can handle
     */
    virtual bool can_handle(const Waypoint& wpt) const;
};

#include "mission/mission_handler_critical_navigating.hxx"

#endif // MISSION_HANDLER_CRITICAL_NAVIGATING__
