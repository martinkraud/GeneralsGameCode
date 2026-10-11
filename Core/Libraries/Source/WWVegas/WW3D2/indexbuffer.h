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

#pragma once

#include "WWLib/always.h"
#include "WWLib/refcount.h"

// W3D resource lifetime declarations. Concrete locks and legacy FVF
// construction remain implemented in dx8indexbuffer.cpp.

class IndexBufferClass : public RefCountClass
{
protected:
	virtual ~IndexBufferClass() override;
public:
	IndexBufferClass(unsigned type, unsigned short index_count);

	void Copy(unsigned int* indices,unsigned start_index,unsigned index_count);
	void Copy(unsigned short* indices,unsigned start_index,unsigned index_count);

	unsigned short Get_Index_Count() const { return index_count; }

	unsigned Type() const { return type; }

	void Add_Engine_Ref() const;
	void Release_Engine_Ref() const;
	unsigned Engine_Refs() const { return engine_refs; }

	class WriteLockClass
	{
		IndexBufferClass* index_buffer;
		unsigned short* indices;
	public:
		WriteLockClass(IndexBufferClass* index_buffer, int flags=0);
		~WriteLockClass();

		unsigned short* Get_Index_Array() { return indices; }
	};

	class AppendLockClass
	{
		IndexBufferClass* index_buffer;
		unsigned short* indices;
	public:
		AppendLockClass(IndexBufferClass* index_buffer,unsigned start_index, unsigned index_range);
		~AppendLockClass();

		unsigned short* Get_Index_Array() { return indices; }
	};

	static unsigned Get_Total_Buffer_Count();
	static unsigned Get_Total_Allocated_Indices();
	static unsigned Get_Total_Allocated_Memory();

protected:
	mutable int					engine_refs;
	unsigned short				index_count;		// number of indices
	unsigned						type;
};
