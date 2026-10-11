/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

 /***********************************************************************************************
 ***              C O N F I D E N T I A L  ---  W E S T W O O D  S T U D I O S               ***
 ***********************************************************************************************
 *                                                                                             *
 *                 Project Name : WW3D                                                         *
 *                                                                                             *
 *                     $Archive:: /Commando/Code/ww3d2/render2dsentence.h                     $*
 *                                                                                             *
 *                       Author:: Greg Hjelstrom                                               *
 *                                                                                             *
 *                     $Modtime:: 8/29/01 10:58a                                              $*
 *                                                                                             *
 *                    $Revision:: 6                                                           $*
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#pragma once
#include "WWLib/always.h"
#include "WWLib/bittype.h"
#include "WWLib/refcount.h"
#include "WWLib/Vector.h"
#include "WWLib/wwstring.h"
#include "WWLib/win.h"

/*
** FontCharsClass
*/


//
//	Private data structures
//
class FontCharsClassCharDataStruct
{
	W3DMPO_CODE(FontCharsClassCharDataStruct)
public:
	WCHAR				Value;
	short				Width;
	uint16 *		Buffer;
};

enum { CHAR_BUFFER_LEN		= 32768 };

class FontCharsBuffer
{
public:
	FontCharsBuffer() : Length( 0 ), Buffer( nullptr ) {}
	FontCharsBuffer( int length, uint16 *buffer ) : Length( length ), Buffer( buffer ) {}

	bool operator== (const FontCharsBuffer &src) const { return Length == src.Length && Buffer == src.Buffer; }
	bool operator!= (const FontCharsBuffer &src) const { return !(*this == src); }

	int				Length;
	uint16 *		Buffer;
};


class FontCharsClass : public RefCountClass
{
	W3DMPO_CODE(FontCharsClass)

public:
	FontCharsClass();
	virtual ~FontCharsClass() override;

	// TR: Hack for unicode font support
	FontCharsClass					*AlternateUnicodeFont;


	bool	Initialize_GDI_Font( const char *font_name, int point_size, bool is_bold );
	bool Initialize_GDI_Font(const char *font_name, int point_size, bool is_bold, void *window);
	bool	Is_Font( const char *font_name, int point_size, bool is_bold );
	const char * Get_Name()			{ return Name; }

	int	Get_Char_Height()			{ return CharHeight; }
	int	Get_Char_Width( WCHAR ch );
	int	Get_Char_Spacing( WCHAR ch );

	int Get_Extra_Overlap() {return PixelOverlap;}

	void	Blit_Char( WCHAR ch, uint16 *dest_ptr, int dest_stride, int x, int y );

private:

	//
	//	Private methods
	//
	bool							Create_GDI_Font( const char *font_name, void *window );
	void							Free_GDI_Font();
	const FontCharsClassCharDataStruct *	Store_GDI_Char( WCHAR ch );
	void							Update_Current_Buffer( int char_width );
	const FontCharsClassCharDataStruct	*	Get_Char_Data( WCHAR ch );

	void							Grow_Unicode_Array( WCHAR ch );
	void							Free_Character_Arrays();

	//
	//	Private member data
	//
	StringClass							Name;
	DynamicVectorClass<FontCharsBuffer>	BufferList;
	int									CurrPixelOffset;
	int									CharHeight;
	int									CharAscent;
	int									CharOverhang;
	int									PixelOverlap;
	int									PointSize;
	StringClass							GDIFontName;
	HFONT									OldGDIFont;
	HBITMAP								OldGDIBitmap;
	HBITMAP								GDIBitmap;
	HFONT									GDIFont;
	uint8 *								GDIBitmapBits;
	HDC									MemDC;
	FontCharsClassCharDataStruct *					ASCIICharArray[256];
	FontCharsClassCharDataStruct **					UnicodeCharArray;
	uint16								FirstUnicodeChar;
	uint16								LastUnicodeChar;
	bool									IsBold;
};
