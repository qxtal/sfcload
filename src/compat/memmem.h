/*
    SFCLoad: A simple ROM loader for SD2SNES/FXPak Pro flash cartridges.
    Copyright (C) 2026 qxtal (www.xtal.net)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.

	SPDX-License-Identifier: GPL-3.0-or-later
*/

#ifndef SFCLOAD_COMPAT_MEMMEM_H
#define SFCLOAD_COMPAT_MEMMEM_H

#ifndef HAVE_MEMMEM

#include <stddef.h>

void *compat_memmem(const void *haystack, size_t hsize, const void *needle, size_t nsize);
#define memmem(h, hs, n, ns) compat_memmem(h, hs, n, ns)

#endif /* HAVE_MEMMEM */

#endif