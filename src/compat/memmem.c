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

#ifdef HAVE_CONFIG_H
	#include <config.h>
#endif

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifndef HAVE_MEMMEM
/**
 * @brief Custom implementation of memmem() (for Windows compatibility)
 * 
 * Finds a byte subsequence in a byte sequence.
 * 
 * Uses the "Not So Naive" string search algorithm, which can be found here:
 * http://www-igm.univ-mlv.fr/~lecroq/string/node13.html#SECTION00130
 * 
 * Input parameters and return values are (at least roughly) based around the
 * implementation found in The Open Group Base Specifications Issue 8:
 * https://pubs.opengroup.org/onlinepubs/9799919799/functions/memmem.html
 * 
 * @param[in] haystack  The byte sequence (haystack) to search in.
 * @param[in] hsize     The size of the byte sequence.
 * @param[in] needle    The byte subsequence (needle) to search for.
 * @param[in] nsize     The size of the byte subsequence.
 * 
 * @return Pointer to the first byte of the located sequence, 
 *         or NULL if none found.
 */
void *compat_memmem(const void *haystack, size_t hsize,
					const void *needle, size_t nsize)
{
	const uint8_t *h = (const uint8_t*)haystack;
	const uint8_t *n = (const uint8_t*)needle;
	size_t j = 0, k = 1, l = 2;

	if (nsize == 0) {
		return (void*)haystack;
	} else if (hsize < nsize) {
		return NULL;
	} else if (nsize == 1) {
		return memchr(haystack, *n, hsize);
	}

	if (n[0] == n[1]) {
		k = 2;
		l = 1;
	}

	while (j <= hsize - nsize) {
		if (n[1] != h[j + 1]) {
			j += k;
		} else {
			if (memcmp(n + 2, h + j + 2, nsize - 2) == 0 && n[0] == h[j]) {
				return (void *)&h[j];
			}
			j += l;
		}
	}

	return NULL;
}

#endif