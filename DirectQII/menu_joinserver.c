/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "client.h"
#include "qmenu.h"

/*
=============================================================================

JOIN SERVER MENU

=============================================================================
*/
#define MAX_LOCAL_SERVERS 8

static menuframework_s	s_joinserver_menu;
static menuseparator_s	s_joinserver_server_title;
static menuaction_s		s_joinserver_search_action;
static menuaction_s		s_joinserver_address_book_action;
static menuaction_s		s_joinserver_server_actions[MAX_LOCAL_SERVERS];

int		m_num_servers;
#define	NO_SERVER_STRING	"<no server>"

// user readable information
static char local_server_names[MAX_LOCAL_SERVERS][80];

// network address
static netadr_t local_server_netadr[MAX_LOCAL_SERVERS];

void M_AddToServerList (netadr_t adr, char *info)
{
	int		i;

	if (m_num_servers == MAX_LOCAL_SERVERS)
		return;
	while (*info == ' ')
		info++;

	// ignore if duplicated
	for (i = 0; i < m_num_servers; i++)
		if (!strcmp (info, local_server_names[i]))
			return;

	local_server_netadr[m_num_servers] = adr;
	strncpy (local_server_names[m_num_servers], info, sizeof (local_server_names[0]) - 1);
	m_num_servers++;
}


void JoinServerFunc (void *self)
{
	char	buffer[128];
	int		index;

	index = (menuaction_s *) self - s_joinserver_server_actions;

	if (Q_stricmp (local_server_names[index], NO_SERVER_STRING) == 0)
		return;

	if (index >= m_num_servers)
		return;

	Com_sprintf (buffer, sizeof (buffer), "connect %s\n", NET_AdrToString (local_server_netadr[index]));
	Cbuf_AddText (buffer);
	M_ForceMenuOff ();
}

void AddressBookFunc (void *self)
{
	M_Menu_AddressBook_f ();
}

void NullCursorDraw (void *self)
{
}

void SearchLocalGames (void)
{
	int		i;

	m_num_servers = 0;

	for (i = 0; i < MAX_LOCAL_SERVERS; i++)
		strcpy (local_server_names[i], NO_SERVER_STRING);

	M_DrawTextBox (8, 120 - 48, 36, 3);
	M_Print (16 + 16, 120 - 48 + 8, "Searching for local servers, this");
	M_Print (16 + 16, 120 - 48 + 16, "could take up to a minute, so");
	M_Print (16 + 16, 120 - 48 + 24, "please be patient.");

	// the text box won't show up unless we do a buffer swap
	re.EndFrame (true);

	// send out info packets
	CL_PingServers_f ();
}

void SearchLocalGamesFunc (void *self)
{
	SearchLocalGames ();
}

void JoinServer_MenuInit (void)
{
	int i;

	s_joinserver_menu.x = viddef.conwidth * 0.50 - 120;
	s_joinserver_menu.nitems = 0;

	s_joinserver_address_book_action.generic.type = MTYPE_ACTION;
	s_joinserver_address_book_action.generic.name = "address book";
	s_joinserver_address_book_action.generic.flags = QMF_LEFT_JUSTIFY;
	s_joinserver_address_book_action.generic.x = 0;
	s_joinserver_address_book_action.generic.y = 0;
	s_joinserver_address_book_action.generic.callback = AddressBookFunc;

	s_joinserver_search_action.generic.type = MTYPE_ACTION;
	s_joinserver_search_action.generic.name = "refresh server list";
	s_joinserver_search_action.generic.flags = QMF_LEFT_JUSTIFY;
	s_joinserver_search_action.generic.x = 0;
	s_joinserver_search_action.generic.y = 10;
	s_joinserver_search_action.generic.callback = SearchLocalGamesFunc;
	s_joinserver_search_action.generic.statusbar = "search for servers";

	s_joinserver_server_title.generic.type = MTYPE_SEPARATOR;
	s_joinserver_server_title.generic.name = "connect to...";
	s_joinserver_server_title.generic.x = 80;
	s_joinserver_server_title.generic.y = 30;

	for (i = 0; i < MAX_LOCAL_SERVERS; i++)
	{
		s_joinserver_server_actions[i].generic.type = MTYPE_ACTION;
		strcpy (local_server_names[i], NO_SERVER_STRING);
		s_joinserver_server_actions[i].generic.name = local_server_names[i];
		s_joinserver_server_actions[i].generic.flags = QMF_LEFT_JUSTIFY;
		s_joinserver_server_actions[i].generic.x = 0;
		s_joinserver_server_actions[i].generic.y = 40 + i * 10;
		s_joinserver_server_actions[i].generic.callback = JoinServerFunc;
		s_joinserver_server_actions[i].generic.statusbar = "press ENTER to connect";
	}

	Menu_AddItem (&s_joinserver_menu, &s_joinserver_address_book_action);
	Menu_AddItem (&s_joinserver_menu, &s_joinserver_server_title);
	Menu_AddItem (&s_joinserver_menu, &s_joinserver_search_action);

	for (i = 0; i < 8; i++)
		Menu_AddItem (&s_joinserver_menu, &s_joinserver_server_actions[i]);

	Menu_Center (&s_joinserver_menu);

	SearchLocalGames ();
}

void JoinServer_MenuDraw (void)
{
	M_Banner ("m_banner_join_server");
	Menu_Draw (&s_joinserver_menu);
}


const char *JoinServer_MenuKey (int key)
{
	return Default_MenuKey (&s_joinserver_menu, key);
}

void M_Menu_JoinServer_f (void)
{
	JoinServer_MenuInit ();
	M_PushMenu (JoinServer_MenuDraw, JoinServer_MenuKey);
}



