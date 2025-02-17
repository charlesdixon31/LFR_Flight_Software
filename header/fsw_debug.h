/*------------------------------------------------------------------------------
--  Solar Orbiter's Low Frequency Receiver Flight Software (LFR FSW),
--  This file is a part of the LFR FSW
--  Copyright (C) 2021, Plasma Physics Laboratory - CNRS
--
--  This program is free software; you can redistribute it and/or modify
--  it under the terms of the GNU General Public License as published by
--  the Free Software Foundation; either version 2 of the License, or
--  (at your option) any later version.
--
--  This program is distributed in the hope that it will be useful,
--  but WITHOUT ANY WARRANTY; without even the implied warranty of
--  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
--  GNU General Public License for more details.
--
--  You should have received a copy of the GNU General Public License
--  along with this program; if not, write to the Free Software
--  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
-------------------------------------------------------------------------------*/
/*--                  Author : Alexis Jeandet
--                   Contact : Alexis Jeandet
--                      Mail : alexis.jeandet@lpp.polytechnique.fr
----------------------------------------------------------------------------*/
#pragma once

#include <stdio.h>

#ifdef PRINT_MESSAGES_ON_CONSOLE
    #define LFR_PRINTF(...)        printf(__VA_ARGS__)
#else
    #define LFR_PRINTF(...)
#endif

#ifdef BOOT_MESSAGES
    #define BOOT_PRINTF(...)        printf(__VA_ARGS__)
#else
    #define BOOT_PRINTF(...)
#endif

#ifdef DEBUG_MESSAGES
    #define DEBUG_PRINTF(...)        printf(__VA_ARGS__)
    #define DEBUG_CHECK_PTR(ptr)     {if((ptr)==NULL){printf("NULL ptr @ %s:%d\n", __FILE__, __LINE__);}}
    #define DEBUG_CHECK_STATUS(status) {if((status)!=0){printf("Status error @ %s:%d\nGot %d", __FILE__, __LINE__,status);}}
#else
    #define DEBUG_PRINTF(...)
    #define DEBUG_CHECK_PTR(ptr)
    #define DEBUG_CHECK_STATUS(status)
#endif
