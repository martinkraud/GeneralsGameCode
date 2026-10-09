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

/////////////////////////////////////////////////////////////////////////EA-V1
// $File: //depot/GeneralsMD/Staging/code/Libraries/Source/debug/debug_stack.cpp $
// $Author: KMorness $
// $Revision: #2 $
// $DateTime: 2005/01/19 15:02:33 $
//
// (c) 2003 Electronic Arts
//
// Stack walker
//////////////////////////////////////////////////////////////////////////////

#include "debug.h"
#include "debug_stack.h"
#include <windows.h>
#include "Utility/stringex.h"
#include "Utility/stdio_adapter.h"
#include <imagehlp.h>

// Definitions to allow run-time linking to the dbghelp.dll functions.

#define DBGHELP(name,ret,par) typedef ret (WINAPI *name##Type) par;
#include "debug_stack.inl"
#undef DBGHELP

#define DBGHELP(name,ret,par) name##Type _##name;
static struct
{
#include "debug_stack.inl"
} gDbg;
#undef DBGHELP

// local dbghelp.dll module handle
static HMODULE g_dbghelp;

// local flag that is true if we're using an old dbghelp.dll version
static bool g_oldDbghelp;

static void InitDbghelp()
{
  // already called?
  if (g_dbghelp)
    return;

	// firstly check for dbghelp.dll in the EXE directory
	char dbgHelpPath[256];
	if (GetModuleFileName(nullptr,dbgHelpPath,sizeof(dbgHelpPath)))
	{
		char *slash=strrchr(dbgHelpPath,'\\');
		if (slash)
		{
			strcpy(slash+1,"DBGHELP.DLL");
			g_dbghelp=::LoadLibrary(dbgHelpPath);
		}
	}
	if (!g_dbghelp)
		// load any version we can
		g_dbghelp=::LoadLibrary("DBGHELP.DLL");

  if (!g_dbghelp)
    return;

  // Resolve each typed slot explicitly; never treat function pointers as DWORDs
  // or assume that separate fields form an array. Expand SDK export aliases.
  bool complete = true;
#define DBGHELP(name,ret,par) \
  gDbg._##name = reinterpret_cast<name##Type>(GetProcAddress(g_dbghelp, RTS_DIAGNOSTIC_EXPORT_NAME(name))); \
  if (!gDbg._##name) complete = false;
#include "debug_stack.inl"
#undef DBGHELP
  if (!complete)
  {
    memset(&gDbg, 0, sizeof(gDbg));
  }
  else
  {
    // Set options
    gDbg._SymSetOptions(gDbg._SymGetOptions()|SYMOPT_DEFERRED_LOADS|SYMOPT_LOAD_LINES);

    // Init module
    gDbg._SymInitialize(GetCurrentProcess(),nullptr,TRUE);

    // Check: are we using a newer version of dbghelp.dll?
    // (older versions have some serious issues.. err... bugs)
    if (!GetProcAddress(g_dbghelp,"SymEnumSymbolsForAddr"))
      g_oldDbghelp=true;
  }
}

//////////////////////////////////////////////////////////////////////////////

DebugStackwalk::Signature::Signature(const Signature &src)
{
  *this=src;
}

DebugStackwalk::Signature& DebugStackwalk::Signature::operator=(const Signature& src)
{
  if (&src!=this)
  {
    m_numAddr=src.m_numAddr;
    memcpy(m_addr,src.m_addr,m_numAddr*sizeof(*m_addr));
  }
  return *this;
}

WindowsDiagnostics::Address DebugStackwalk::Signature::GetAddress(int n) const
{
  DFAIL_IF_MSG(n<0||n>=MAX_ADDR,n << "/" << MAX_ADDR) return 0;
  return m_addr[n];
}

void DebugStackwalk::Signature::GetSymbol(WindowsDiagnostics::Address addr, char *buf, unsigned bufSize)
{
  DFAIL_IF(!buf) return;
  DFAIL_IF(bufSize<64||bufSize>=0x80000000) return;

  InitDbghelp();

  char *bufEnd=buf+bufSize;
  *buf=0;
  buf+=snprintf(buf,bufEnd-buf,RTS_DIAGNOSTIC_ADDRESS_FORMAT,addr);

  // determine module
  WindowsDiagnostics::ApiAddress modBase=gDbg._SymGetModuleBase ? gDbg._SymGetModuleBase(GetCurrentProcess(),addr) : 0;
  if (!modBase)
	{
		strcpy(buf," (unknown module)");
    return;
	}

  // illegal code ptr?
	if (IsBadReadPtr((void *)addr,4)||IsBadCodePtr((FARPROC)addr))
	{
		strcpy(buf," (invalid code addr)");
		return;
	}

  WindowsDiagnostics::SymbolBuffer symbolStorage;
  char* symbolBuffer = reinterpret_cast<char*>(symbolStorage.bytes);
  GetModuleFileName((HMODULE)modBase,symbolBuffer,sizeof(symbolStorage));

  char *p=strrchr(symbolBuffer,'\\'); // use filename only, strip off path
  p=p?p+1:symbolBuffer;
  *buf++=' ';
  strlcpy(buf,p,bufEnd-buf);
  buf+=strlen(buf);
  if (bufEnd-buf<32)
    return;
  buf+=snprintf(buf,bufEnd-buf,"+0x" RTS_DIAGNOSTIC_OFFSET_FORMAT,addr-modBase);

  // determine symbol
  PIMAGEHLP_SYMBOL symPtr=(PIMAGEHLP_SYMBOL)symbolBuffer;
  memset(symPtr,0,sizeof(symbolStorage));
  symPtr->SizeOfStruct=sizeof(IMAGEHLP_SYMBOL);
  symPtr->MaxNameLength=sizeof(symbolStorage)-sizeof(IMAGEHLP_SYMBOL);
  WindowsDiagnostics::SymbolDisplacement displacement;
  if (!gDbg._SymGetSymFromAddr(GetCurrentProcess(),addr,&displacement,symPtr))
    return;
  if ((unsigned int)(bufEnd-buf)<strlen(symPtr->Name)+32)
    return;
  buf+=snprintf(buf,bufEnd-buf,", %s+0x" RTS_DIAGNOSTIC_OFFSET_FORMAT,symPtr->Name,displacement);

  // and line number
  IMAGEHLP_LINE line;
  DWORD lineDisplacement;
  memset(&line,0,sizeof(line));
  line.SizeOfStruct=sizeof(line);
  if (!gDbg._SymGetLineFromAddr(GetCurrentProcess(),addr,&lineDisplacement,&line))
    return;

  p=strrchr(line.FileName,'\\'); // use filename only, strip off path
  p=p?p+1:line.FileName;

  if ((unsigned int)(bufEnd-buf)<strlen(p)+32)
    return;
  buf+=snprintf(buf,bufEnd-buf,", %s:%lu+0x%lx",p,line.LineNumber,lineDisplacement);
}

void DebugStackwalk::Signature::GetSymbol(WindowsDiagnostics::Address addr,
                                          char *bufMod, unsigned sizeMod, WindowsDiagnostics::Address *relMod,
                                          char *bufSym, unsigned sizeSym, WindowsDiagnostics::Address *relSym,
                                          char *bufFile, unsigned sizeFile, unsigned *linePtr, unsigned *relLine)
{
  InitDbghelp();

  if (bufMod) *bufMod=0;
  if (relMod) *relMod=0;
  if (bufSym) *bufSym=0;
  if (relSym) *relSym=0;

  if (bufFile) *bufFile=0;
  if (linePtr) *linePtr=0;
  if (relLine) *relLine=0;

  DFAIL_IF(bufMod&&sizeMod<16) return;
  DFAIL_IF(bufSym&&sizeSym<16) return;
  DFAIL_IF(bufFile&&sizeFile<16) return;

  // determine module
  WindowsDiagnostics::ApiAddress modBase=gDbg._SymGetModuleBase ? gDbg._SymGetModuleBase(GetCurrentProcess(),addr) : 0;
  if (!modBase)
	{
    if (bufMod)
		  strcpy(bufMod,"(unknown mod)");
    if (bufSym)
      strcpy(bufSym,"(unknown)");
    return;
	}

  // illegal code ptr?
	if (IsBadReadPtr((void *)addr,4)||IsBadCodePtr((FARPROC)addr))
	{
    if (bufMod)
		  strcpy(bufMod,"(inv code addr)");
    if (bufSym)
      strcpy(bufSym,"(unknown)");
		return;
	}

  WindowsDiagnostics::SymbolBuffer symbolStorage;
  char* symbolBuffer = reinterpret_cast<char*>(symbolStorage.bytes);
  if (bufMod)
  {
    GetModuleFileName((HMODULE)modBase,symbolBuffer,sizeof(symbolStorage));

    char *p=strrchr(symbolBuffer,'\\'); // use filename only, strip off path
    p=p?p+1:symbolBuffer;
    strlcpy(bufMod,p,sizeMod);
  }
  if (relMod)
    *relMod=addr-modBase;

  // determine symbol
  if (bufSym)
  {
    PIMAGEHLP_SYMBOL symPtr=(PIMAGEHLP_SYMBOL)symbolBuffer;
    memset(symPtr,0,sizeof(symbolStorage));
    symPtr->SizeOfStruct=sizeof(IMAGEHLP_SYMBOL);
    symPtr->MaxNameLength=sizeof(symbolStorage)-sizeof(IMAGEHLP_SYMBOL);
    WindowsDiagnostics::SymbolDisplacement displacement;
    if (gDbg._SymGetSymFromAddr(GetCurrentProcess(),addr,&displacement,symPtr))
    {
      strlcpy(bufSym,symPtr->Name,sizeSym);
      if (relSym)
        *relSym=displacement;
    }
    else
      strcpy(bufSym,"(unknown)");
  }

  // and line number
  if (bufFile)
  {
    IMAGEHLP_LINE line;
    DWORD lineDisplacement;
    memset(&line,0,sizeof(line));
    line.SizeOfStruct=sizeof(line);
    if (!gDbg._SymGetLineFromAddr(GetCurrentProcess(),addr,&lineDisplacement,&line))
      strcpy(bufFile,"(unknown)");
    else
    {
      char *p=strrchr(line.FileName,'\\'); // use filename only, strip off path
      p=p?p+1:line.FileName;
      strlcpy(bufFile,p,sizeFile);
      if (linePtr)
        *linePtr=line.LineNumber;
      if (relLine)
        *relLine=lineDisplacement;
    }
  }
}

Debug& operator<<(Debug &dbg, const DebugStackwalk::Signature &sig)
{
  dbg << sig.Size() << " addresses:\n";

  for (unsigned k=0;k<sig.Size();k++)
  {
    char buf[512];
    sig.GetSymbol(sig.GetAddress(k),buf,sizeof(buf));
    dbg << buf << "\n";
  }

  return dbg;
}

//////////////////////////////////////////////////////////////////////////////

DebugStackwalk::DebugStackwalk()
{
  // it doesn't harm to do this here
  InitDbghelp();
}

DebugStackwalk::~DebugStackwalk()
{
}

void *DebugStackwalk::GetDbghelpHandle()
{
  return g_dbghelp;
}

bool DebugStackwalk::IsOldDbghelp()
{
  return g_oldDbghelp;
}

int DebugStackwalk::StackWalk(Signature &sig, struct _CONTEXT *ctx)
{
  InitDbghelp();

  sig.m_numAddr=0;

  // bail out if no stack walk available
  if (!gDbg._StackWalk)
    return 0;

	// Set up the stack frame structure for the start point of the stack walk (i.e. here).
	STACKFRAME stackFrame;
	memset(&stackFrame,0,sizeof(stackFrame));

	stackFrame.AddrPC.Mode = AddrModeFlat;
	stackFrame.AddrStack.Mode = AddrModeFlat;
	stackFrame.AddrFrame.Mode = AddrModeFlat;

	CONTEXT walkContext = {};
	// Work on a copy because native unwinding modifies register state.
	// Use the context struct if it was provided.
	if (ctx)
  {
    walkContext = *ctx;
    WindowsDiagnostics::InitializeFrame(stackFrame, walkContext);
	}
  else
  {
#if defined(_WIN64)
    RtlCaptureContext(&walkContext);
    WindowsDiagnostics::InitializeFrame(stackFrame, walkContext);
#else
    // walk stack back using current call chain
	  unsigned long reg_eip, reg_ebp, reg_esp;
#if defined(_MSC_VER)
	  __asm
    {
    here:
		  lea	eax,here
		  mov	reg_eip,eax
		  mov	reg_ebp,ebp
		  mov	reg_esp,esp
	  };
#elif (defined(__GNUC__) || defined(__clang__)) && (defined(__i386__) || defined(_M_IX86))
	  __asm__ __volatile__ (
		  "call 1f\n\t"
		  "1: pop %0\n\t"
		  "mov %%ebp, %1\n\t"
		  "mov %%esp, %2"
		  : "=r" (reg_eip), "=r" (reg_ebp), "=r" (reg_esp)
	  );
#else
#error "Unsupported compiler or architecture for register capture"
#endif
	  stackFrame.AddrPC.Offset = reg_eip;
	  stackFrame.AddrStack.Offset = reg_esp;
	  stackFrame.AddrFrame.Offset = reg_ebp;
#endif
  }

	// Walk the stack by the requested number of return address iterations.
  bool skipFirst=!ctx;
  while (sig.m_numAddr<Signature::MAX_ADDR&&
		     gDbg._StackWalk(WindowsDiagnostics::MachineType,GetCurrentProcess(),GetCurrentThread(),
                         &stackFrame,WindowsDiagnostics::WalkContext(walkContext),nullptr,gDbg._SymFunctionTableAccess,gDbg._SymGetModuleBase,nullptr))
  {
    if (skipFirst)
      skipFirst=false;
    else
      sig.m_addr[sig.m_numAddr++]=stackFrame.AddrPC.Offset;
  }

	return sig.m_numAddr;
}
