/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "../djdpmi.h"

#define DD(r, n, a, ...) \
r ___##n a;
#define DDv(r, n) \
r ___##n(void);
#define vDD(n, a, ...) \
void ___##n a;
#define vDDv(n) \
void ___##n(void);

#include "dpmi_inc.h"

#undef DD
#undef DDv
#undef vDD
#undef vDDv
