/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
 
#ifndef __HWMSEN_CUSTOM_H__ 
#define __HWMSEN_CUSTOM_H__

#define ENABLE_LIGHT_SENSOR 1

#if defined(ENABLE_LIGHT_SENSOR)

#if defined(LIGHT_CM36283) || defined(LIGHT_AP3220)
	#if defined(MAGN_BMC050)
	#define MAX_NUM_SENSORS                 5
	#else
	#define MAX_NUM_SENSORS                 3
	#endif
#else
	#if defined(MAGN_BMC050)
	#define MAX_NUM_SENSORS                 4
	#else
	#define MAX_NUM_SENSORS                 2
	#endif
#endif

#else

#if defined(MAGN_BMC050)
#define MAX_NUM_SENSORS                 4
#else
#define MAX_NUM_SENSORS                 2
#endif

#endif

//#define MAX_NUM_SENSORS                 2

#endif

