/***************************************************************************
 *   Copyright (C) 2005 by Joris Guisson                                   *
 *   joris.guisson@gmail.com                                               *
 *   ivasic@gmail.com                                                      *
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
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.             *
 ***************************************************************************/
#ifndef KTQUEUEMANAGER_H
#define KTQUEUEMANAGER_H

#include <set>
#include <qobject.h>
#include <qlinkedlist.h>
#include <interfaces/torrentinterface.h>
#include <interfaces/queuemanagerinterface.h>
#include <ktcore_export.h>

namespace bt
{
	class SHA1Hash;
	struct TrackerTier;
	class WaitJob;
}

namespace kt
{

	class KTCORE_EXPORT QueuePtrList : public QList<bt::TorrentInterface *>
	{
	public:
		QueuePtrList();
		virtual ~QueuePtrList();

		/**
		 * Sort based upon priority
		 */
		void sort();
			
	protected:
		static bool biggerThan(bt::TorrentInterface* tc1, bt::TorrentInterface* tc2);
	};

	/**
	 * @author Ivan Vasic
	 * @brief This class contains list of all TorrentControls and is responsible for starting/stopping them
	 */
	class KTCORE_EXPORT QueueManager : public QObject,public bt::QueueManagerInterface
	{
		Q_OBJECT
				
	public:
		QueueManager();
		virtual ~QueueManager();

		void append(bt::TorrentInterface* tc);
		void remove(bt::TorrentInterface* tc);
		void clear();
			
		/**
		 * Start a torrent
		 * @param tc The torrent
		 * @param user Wether or not the user does this
		 * @return What happened
		 */
		bt::TorrentStartResponse start(bt::TorrentInterface* tc, bool user = true);
		
		/**
		 * Stop a torrent
		 * @param tc The torrent
		 * @param user Wether or not the user does this
		 */
		void stop(bt::TorrentInterface* tc, bool user = false);
		
		/**
		 * Start a list of torrents. The torrents will become user controlled.
		 * @param todo The list of torrents 
		 */
		void start(QList<bt::TorrentInterface*> & todo);

		void stopall(int type);
		void startall(int type);
			
		/**
		 * Stop all running torrents
		 * @param wjob WaitJob which waits for stopped events to reach the tracker
		 */
		void onExit(bt::WaitJob* wjob);

		/**
		 * Enqueue/Dequeue function. Places a torrent in queue. 
		 * If the torrent is already in queue this will remove it from queue.
		 * @param tc TorrentControl pointer.
		 */
		void queue(bt::TorrentInterface* tc);

		/// Get the number of torrents	
		int count() { return downloads.count(); }
		
		/// Get the number of downloads
		int countDownloads();
		
		/// Get the number of seeds
		int countSeeds();

		enum Flags
		{
			SEEDS = 1,
			DOWNLOADS = 2,
			ALL = 3
		};
		
		/**
		 * Get the number of running torrents
		 * @param flags Which torrents to choose
		 */
		int getNumRunning(Flags flags = ALL);
		
		/**
		 * Start the next torrent.
		 */
		void startNext();
		
		typedef QList<bt::TorrentInterface *>::iterator iterator;
		
		iterator begin();
		iterator end();
		
		/**
		 * Get the torrent at index idx in the list.
		 * @param idx Index of the torrent
		 * @return The torrent or 0 if the index is out of bounds
		 */
		const bt::TorrentInterface* getTorrent(bt::Uint32 idx) const;

		/**
		 * See if we already loaded a torrent.
		 * @param ih The info hash of a torrent
		 * @return true if we do, false if we don't
		 */
		bool allreadyLoaded(const bt::SHA1Hash & ih) const;


		/**
		 * Merge announce lists to a torrent
		 * @param ih The info_hash of the torrent to merge to
		 * @param trk First tier of trackers
		 */
		void mergeAnnounceList(const bt::SHA1Hash & ih,const bt::TrackerTier* trk);

		/**
		 * Set the maximum number of downloads
		 * @param m Max downloads
		 */
		void setMaxDownloads(int m);
		
		/**
		 * Set the maximum number of seeds
		 * @param m Max seeds
		 */
		void setMaxSeeds(int m);

		/**
		 * Enable or disable keep seeding (after a torrent has finished)
		 * @param ks Keep seeding
		 */
		void setKeepSeeding(bool ks);

		/**
		 * Sets global paused state for QueueManager and stopps all running torrents.
		 * No torrents will be automatically started/stopped with QM.
		 */
		void setPausedState(bool pause);

		/// Get the paused state
		bool getPausedState() const {return paused_state;}
		
		/**
		 * Places all torrents from downloads in the right order in queue.
		 * Use this when torrent priorities get changed
		 */
		void orderQueue();

	signals:
		/**
		* User tried to enqueue a torrent that has reached max share ratio. It's not possible.
		* Signal should be connected to SysTray slot which shows appropriate KPassivePopup info.
		* @param tc The torrent in question.
		*/
		void queuingNotPossible(bt::TorrentInterface* tc);

		/**
		* Diskspace is running low.
		* Signal should be connected to SysTray slot which shows appropriate KPassivePopup info. 
		* @param tc The torrent in question.
		*/
		void lowDiskSpace(bt::TorrentInterface* tc, bool stopped);
		
		/**
		* Emitted when the QM reorders it's queue
		*/
		void queueOrdered();
		
		/**
		 * Emitted when the paused state changes.
		 * @param paused The paused state
		 */
		void pauseStateChanged(bool paused);

	public slots:
		void torrentFinished(bt::TorrentInterface* tc);
		void torrentAdded(bt::TorrentInterface* tc,bool user, bool start_torrent);
		void torrentRemoved(bt::TorrentInterface* tc);
		void torrentStopped(bt::TorrentInterface* tc);
		void onLowDiskSpace(bt::TorrentInterface* tc, bool toStop);

	private:
		void enqueue(bt::TorrentInterface* tc);
		void dequeue(bt::TorrentInterface* tc);
		void startSafely(bt::TorrentInterface* tc);
		void stopSafely(bt::TorrentInterface* tc,bool user,bt::WaitJob* wjob = 0);
		void checkDiskSpace(QList<bt::TorrentInterface*> & todo);
		void checkMaxSeedTime(QList<bt::TorrentInterface*> & todo);
		void checkMaxRatio(QList<bt::TorrentInterface*> & todo);

	private:
		QueuePtrList downloads;
		std::set<bt::TorrentInterface*> paused_torrents;
		int max_downloads;
		int max_seeds;
		bool paused_state;
		bool keep_seeding;
		bool exiting;
	};
}
#endif
