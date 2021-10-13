/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2010  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "PrecompiledHeader.h"
#include "App.h"
#include "common/Threading.h"
#include "ghc/filesystem.h"
#define BACKWARD_HAS_BFD 1
#include "backward.hpp"

static wxString pxGetStackTrace( const FnChar_t* calledFrom )
{
	std::stringstream text;
	backward::StackTrace st;
	st.load_from((void*)calledFrom);

	backward::TraceResolver tr;
	tr.load_stacktrace(st);

	for (size_t i = 3; i < st.size(); ++i) {
		backward::ResolvedTrace trace = tr.resolve(st[i]);
		text << "#" << i
			<< " " << ghc::filesystem::path(trace.object_filename).filename().string()
			<< " " << ghc::filesystem::path(trace.source.filename).filename().string()
			<< ":" << trace.source.line
			<< " " << trace.object_function
			<< " [" << trace.addr << "]"
		<< std::endl;
	}

	return wxString(text.str());
}

#ifdef __WXDEBUG__

// This override of wx's implementation provides thread safe assertion message reporting.
// If we aren't on the main gui thread then the assertion message box needs to be passed
// off to the main gui thread via messages.
void Pcsx2App::OnAssertFailure( const wxChar *file, int line, const wxChar *func, const wxChar *cond, const wxChar *msg )
{
	// Re-entrant assertions are bad mojo -- trap immediately.
	static DeclareTls(int) _reentrant_lock( 0 );
	RecursionGuard guard( _reentrant_lock );
	if( guard.IsReentrant() ) pxTrap();

	wxCharBuffer bleh( wxString(func).ToUTF8() );
	if( AppDoAssert( DiagnosticOrigin( file, line, bleh, cond ), msg ) )
	{
		pxTrap();
	}
}

#endif

bool AppDoAssert( const DiagnosticOrigin& origin, const wxChar *msg )
{
	// Used to allow the user to suppress future assertions during this application's session.
	static bool disableAsserts = false;
	if( disableAsserts ) return false;

	wxString trace( pxGetStackTrace(origin.function) );
	wxString dbgmsg( origin.ToString( msg ) );

	wxMessageOutputDebug().Printf( L"%s", WX_STR(dbgmsg) );

	Console.Error( L"%s", WX_STR(dbgmsg) );
	Console.WriteLn( L"%s", WX_STR(trace) );

	wxString windowmsg( L"Assertion failed: " );
	if( msg != NULL )
		windowmsg += msg;
	else if( origin.condition != NULL )
		windowmsg += origin.condition;

	int retval = Msgbox::Assertion( windowmsg, dbgmsg + L"\nStacktrace:\n" + trace );

	if( retval == wxID_YES ) return true;
	if( retval == wxID_IGNORE ) disableAsserts = true;

	return false;
}
