/***************************************************************************
 *   Copyright (C) 2005 by Joris Guisson                                   *
 *   joris.guisson@gmail.com                                               *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Steet, Fifth Floor, Boston, MA 02110-1301, USA.             *
 ***************************************************************************/
#ifndef BTSERVER_H
#define BTSERVER_H

#include <qptrlist.h>
#include "globals.h"

namespace bt
{
	class PeerManager;

	/**
	 * @author Joris Guisson
	 *
	 * Class which listens for incoming connections.
	 * Handles authentication and then hands of the new
	 * connections to a PeerManager.
	 *
	 * All PeerManager's should register with this class when they
	 * are created and should unregister when they are destroyed.
	 */
/*	class Server : public QServerSocket
	{
		QPtrList<PeerManager> peer_managers;
	public:
		Server(Uint16 port);
		virtual ~Server();

		void addPeerManager(PeerManager* pman);
		void removePeerManager(PeerManager* pman);

	protected:
		virtual void newConnection(int socket);
	};
*/
}

#endif
