/*****************************************************************************
 * Copyright (c) 1998-2001, French Touch, SARL
 * http://www.french-touch.net
 * info@french-touch.net
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/


#ifndef __ERROR_TYPES_R__
#define __ERROR_TYPES_R__

type 'ErAc'
{
	integer = $$Countof(Actions);
	array Actions
	{
		integer;
		pstring;
		
		integer = $$Countof(Solutions);
		array Solutions
		{
			integer;
			pstring;
		};
	};
};

type 'ErCd'
{
	integer = $$Countof(Errors);
	array Errors
	{
		integer;
		pstring;
		pstring;
	};
};

#endif