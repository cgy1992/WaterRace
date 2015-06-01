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


// --------------------------------------------------------------------
//	�	ColorGamma.h
// --------------------------------------------------------------------
//
//	ColorGammaLib
//
//		by Drew Thaler (athaler@umich.edu)
//		with help and technical advice from Matt Slot (fprefect@AmbrosiaSW.com)
//		based in part on Matt Slot's Gamma Fader lib v1.2.
//
//	This library is public domain.
//
//	Send comments and suggestions to me (drew) at <athaler@umich.edu>.  I don't know
//	if I'll really be able to handle every bug report, but I'd be interested in
//	hearing from anyone who finds it useful or has suggestions for improvements.
//	Because of variations in video hardware, there's no guarantee that this stuff
//	will necessarily work for you or for your users... but it works for me, and
//	everyone i've given it to so far.  The author shall not be held liable for
//	any possible damage, etc, that might result from use of this library... use
//	it at your own risk.  It's public domain, after all.  =)
//	
//	If you release software that takes advantage of this lib, it'd be awfully
//	kind of you to send me a copy of the finished product by way of thanks, but it's
//	certainly not necessary.  Although I *am* a poor college student who would
//	greatly appreciate such a gift.... ;)
//
//
//
//	Contents:
//
//		Everything you need to do a color gamma fade.  Includes a high-level
//		API with calls such as FadeToColor and FadeToBlack, and a low-level API
//		for when you absolutely need that extra bit of control.  Data storage
//		is maintained in handles to be easy on your heap and prevent fragmentation.
//
//	Technical background:
//
//		Fading the screen to a color is a cool special effect.  There are several
//		ways to do it... 'clut'-fading is one well-known technique for doing this
//		type of fade by altering a monitor's color lookup table.  There's one caveat,
//		though: it doesn't work in thousands or millions of colors, since those are
//		direct-rgb modes that don't use cluts.  Gamma fading is the other option,
//		but although there was a public domain lib to do percentage gamma fades
//		to black, there hasn't been a free library to do gamma fading to an
//		arbitrary color... until now, that is.  :>
//		
//		Color gamma fading is accomplished by fading the three channels of a
//		screen's gamma table in the ratio of the r-g-b values of the color.
//		The speed of a color gamma fade is generally much better than a 'clut'
//		fade, providing silky-smooth transitions on both 68K and PPC.
//
//	High-level API
//
//		The gamma state of the attached monitor(s) is maintained in an opaque
//		data structure called a GammaRef by the library.  Before fading, the
//		user must call StartFading to retrieve the initial gamma state, and
//		then StopFading afterward to dispose of the allocated state.
//		StopFading can optionally restore the unmodified initial state before
//		disposing of the GammaRef.  This is highly recommended, in case the gamma
//		fader lib has screwed up somehow -- only in extraordinary circumstances
//		would you ever *not* want to do this.
//		
//		The high-level API consists of three functions: FadeToColor, FadeToGamma,
//		and FadeToBlack, with corresponding single-monitor versions.  FadeToColor
//		takes a color to fade to as an argument, and FadeToGamma accepts a GammaRef
//		similarly.  You can specify the number of steps in the fade (more steps take
//		longer) and the type of fade to perform -- either linear, quadratic, or
//		inverse quadratic. A linear fade does a straight interpolated fade between
//		the two states, while the quadratic fade starts slowly and "accelerates" as
//		it nears the destination.  (This looks good for a fade to black.)  Inverse
//		quadratic is the opposite of quadratic, starting quickly and slowing
//		down as it nears the destination.
//
//		Before calling StopFading, you generally would call FadeToGamma with the
//		initial state returned by StartFading to fade back into normal, rather than
//		abruptly switching into it.
//		
//	Low-level API
//		
//		The low-level API provides more control, and allows you to do other things
//		during a fade rather than just sit there.  I'm not going to go into great
//		detail here... the functions are mostly self-explanatory, and the source is
//		provided.  Figure it out.
//		
//		Note that as far as I can figure, the low-level API (with the exception of
//		the ones that return GammaRefs, of course) does not move memory and therefore
//		might be safe to call from interrupt time!?  Don't quote me on that one,
//		since I've never tested it, but the only Toolbox calls made by the two low-level
//		fade calls are BlockMoveData, PBControl, Get/SetGDevice, and SetEntries.  So if
//		you really NEED to do fades at interrupt time, and enjoy living on the edge,
//		it might just work.  Personally, I wouldn't recommend it, but you know, some
//		people are crazy like that.
//	
//	Compilation notes
//
//		By default, ColorGamma will try to link to the ANSI memset function to avoid
//		unnecessary code duplication.  If you're not linking with the ANSI libs in
//		your project, change the flags at the beginning of the file and #define
//		EXTERN_MEMSET false.  There are a few other compilation flags... check out
//		the .c file to see all of them.  (they're right at the top.)
//
//	Other notes
//
//		Thanks to Matt Slot for his input on the high-level API, and the tipoff on
//		how to do the color gamma fade in the first place.  Thanks also to DaveK,
//		whyte and everyone else who beta-tested this lib on #macdev.
//		


#ifndef __COLORGAMMA__
#define __COLORGAMMA__



#ifdef __cplusplus
extern "C" {
#endif



// --------------------------------------------------------------------
//	�	Typedefs and enums
// --------------------------------------------------------------------

typedef struct OpaqueGammaInfo	**GammaRef;

// ----------
// Fade types

typedef UInt16	FadeType;
enum {
	inverseFade				= (1<<0),		// invert fade flag (for nonlinear fades)

	linearFade				= (1<<1),
	quadraticFade			= (1<<2),
	inverseQuadraticFade	= quadraticFade | inverseFade
};


// ----------
// Error codes

enum {
	noMonitorsCanFade		= -19400,
	invalidFadeType			= -19401,
	monitorCantDoColorGamma	= -19402
};



// --------------------------------------------------------------------
//	�	Prototypes
// --------------------------------------------------------------------


	// availability -- checks to make sure the ColorGamma traps are available. Does
	// not actually check to see whether any monitors are capable of color fades.
	// This function is mostly unnecessary, since it's safe to call ColorGamma
	// even if this function returns false; the fades just won't do anything.

pascal Boolean	IsColorGammaAvailable( void );

	// begin/end routines -- these must be called before/after fading.  StopFading
	// can optionally restore the original monitor state when it's done -- this
	// is a really good idea, you should do it.

pascal OSErr	StartFading( GammaRef *returnedInitialState );
pascal OSErr	Start1Fading( GDHandle theDevice, GammaRef *returnedInitialState );
pascal void		StopFading( GammaRef initialState, Boolean restore );

	// high-level routines -- mostly self-explanatory.  FadeType is an enum
	// defined above, consisting of either linearFade, quadraticFade, or
	// inverseQuadraticFade.

pascal OSErr	FadeToColor( const RGBColor *color, UInt16 numSteps, FadeType typeOfFade );

pascal OSErr	FadeToGamma( GammaRef to, UInt16 numSteps, FadeType typeOfFade );

pascal OSErr	FadeToBlack( UInt16 numSteps, FadeType typeOfFade );

	// single-monitor versions of high-level routines

pascal OSErr	Fade1ToColor( GDHandle theDevice, const RGBColor *color,
						UInt16 numSteps, FadeType typeOfFade, SndChannelPtr chan, short volume );

pascal OSErr	Fade1ToGamma( GDHandle theDevice, GammaRef to,
						UInt16 numSteps, FadeType typeOfFade, SndChannelPtr chan, short volume );

pascal OSErr	Fade1ToBlack( GDHandle theDevice, UInt16 numSteps, FadeType typeOfFade, SndChannelPtr chan, short volume );


	// state routines -- you can get/set the current monitor state at any time
	// using these routines.  Note that any calls that return GammaRefs allocate
	// memory, which the caller is responsible for disposing of with DisposeGammaRef.

pascal GammaRef	GetMonitorState( void );
pascal GammaRef	Get1MonitorState( GDHandle theDevice );
pascal OSErr	SetMonitorState( GammaRef state );
pascal OSErr	Set1MonitorState( GDHandle theDevice, GammaRef state );

pascal OSErr	DisposeGammaRef( GammaRef state );

	// calculations for partial fading -- these return a GammaRef representing
	//	the specified fade, without actually doing it.  The new ref can then
	//	be used in FadeToGamma or another routine.  Caller is responsible for
	//	disposing of the allocated GammaRef.
	
pascal GammaRef CalcFadeToColor( GammaRef from, const RGBColor *color,
							UInt16 whichStep, UInt16 numSteps );

pascal GammaRef CalcFadeToGamma( GammaRef from, GammaRef to,
							UInt16 whichStep, UInt16 numSteps );



	// low-level routines, providing the ability to fade incrementally
	//	or only partway, thereby allowing you to do something useful
	//	while you're fading.  calls to these functions should be
	//	bracketed with HideCursor/ShowCursor to prevent weird problems
	//	on PCI macs with hardware cursors.
	
pascal OSErr	FadeFromGammaToColor( GammaRef from, const RGBColor *color,
								UInt16 whichStep, UInt16 numSteps );

pascal OSErr	FadeFromGammaToGamma( GammaRef from, GammaRef to,
								UInt16 whichStep, UInt16 numSteps );

pascal OSErr Set1MonitorGammaColor(GammaRef state, GDHandle theDevice, const RGBColor* color, long intensity, long max);



#ifdef __cplusplus
}
#endif

#endif // __COLORGAMMA__
