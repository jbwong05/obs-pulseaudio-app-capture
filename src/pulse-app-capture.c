/*
Copyright (C) 2021 by Joshua Wong <jbwong05@gmail.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include <obs-module.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-pulseaudio-app-capture", "en-US")
MODULE_EXPORT const char *obs_module_description(void)
{
	return "PulseAudio app capture";
}

extern void register_source();

bool obs_module_load(void)
{
	register_source();
	return true;
}
