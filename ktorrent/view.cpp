/***************************************************************************
 *   Copyright (C) 2007 by Joris Guisson and Ivan Vasic                    *
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
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.          *
 ***************************************************************************/
#include <QHeaderView>
#include <QFileInfo>
#include <QSortFilterProxyModel>
#include <krun.h>
#include <klocale.h>
#include <ksharedconfig.h>
#include <kmessagebox.h>
#include <kfiledialog.h>
#include <interfaces/torrentinterface.h>
#include <torrent/queuemanager.h>
#include <util/functions.h>
#include <util/log.h>
#include <interfaces/functions.h>
#include <groups/group.h>
#include "view.h"
#include "core.h"
#include "viewmodel.h"
#include "viewmenu.h"
#include "scandlg.h"
#include "speedlimitsdlg.h"
#include "addpeersdlg.h"

using namespace bt;

namespace kt
{
	
	View::View(ViewModel* model,Core* core,QWidget* parent) 
		: QTreeView(parent),core(core),group(0),num_torrents(0),num_running(0),model(model),flags(0)
	{
		menu = new ViewMenu(core->getGroupManager(),this);
		setContextMenuPolicy(Qt::CustomContextMenu);
		setRootIsDecorated(false);
		setSortingEnabled(true);
		setAlternatingRowColors(true);
		setDragEnabled(true);
		setSelectionMode(QAbstractItemView::ExtendedSelection);
		setSelectionBehavior(QAbstractItemView::SelectRows);
		
		connect(this,SIGNAL(wantToRemove(bt::TorrentInterface*,bool )),core,SLOT(remove(bt::TorrentInterface*,bool )));
		connect(this,SIGNAL(wantToStart( QList<bt::TorrentInterface*> & )),core,SLOT(start( QList<bt::TorrentInterface*> & )));
		connect(this,SIGNAL(wantToStop( bt::TorrentInterface*, bool )),core,SLOT(stop( bt::TorrentInterface*, bool )));
		connect(this,SIGNAL(customContextMenuRequested(const QPoint & ) ),this,SLOT(showMenu( const QPoint& )));
	
		header()->setContextMenuPolicy(Qt::CustomContextMenu);
		connect(header(),SIGNAL(customContextMenuRequested(const QPoint & ) ),this,SLOT(showHeaderMenu( const QPoint& )));
		header_menu = new KMenu(this);
		header_menu->addTitle(i18n("Columns"));
	
		for (int i = 0;i < model->columnCount(QModelIndex());i++)
		{
			QString col = model->headerData(i,Qt::Horizontal,Qt::DisplayRole).toString();
			QAction* act = header_menu->addAction(col);
			act->setCheckable(true);
			act->setChecked(true);
			column_idx_map[act] = i;
		}
		
		connect(header_menu,SIGNAL(triggered(QAction* )),this,SLOT(onHeaderMenuItemTriggered(QAction*)));
		
		proxy_model = new QSortFilterProxyModel(this);
		proxy_model->setSourceModel(model);
		proxy_model->setSortRole(Qt::UserRole);
		setModel(proxy_model);
		//setModel(model);
		connect(selectionModel(),SIGNAL(currentChanged(const QModelIndex &,const QModelIndex &)),
				this,SLOT(onCurrentItemChanged(const QModelIndex&, const QModelIndex&)));
		connect(selectionModel(),SIGNAL(selectionChanged(const QItemSelection &,const QItemSelection)),
				this,SLOT(onSelectionChanged(const QItemSelection &,const QItemSelection)));
	}

	View::~View()
	{
	}

	void View::setGroup(Group* g)
	{
		group = g;
		update();
	}

	bool View::update()
	{
		Uint32 torrents = 0;
		Uint32 running = 0;
		Uint32 idx = 0;
		QList<bt::TorrentInterface*> all;
		model->allTorrents(all);
		
		// update items which are part of the current group
		// if they are not part of the current group, just hide them
		foreach (bt::TorrentInterface* ti,all)
		{
			QModelIndex midx = model->index(idx,0,QModelIndex());
			midx = proxy_model->mapFromSource(midx);
			if (!group || (group && group->isMember(ti)))
			{
				if (isRowHidden(midx.row(),QModelIndex()))
					setRowHidden(midx.row(),QModelIndex(),false);

				torrents++;
				if (ti->getStats().running)
					running++;
			}
			else if (!isRowHidden(midx.row(),QModelIndex()))
				setRowHidden(midx.row(),QModelIndex(),true);
			
			idx++;
		}

		int nflags = flags & (kt::START | kt::STOP | kt::REMOVE);
		if (running == torrents && torrents > 0)
			nflags |= kt::STOP_ALL;
		else if (running == 0 && torrents > 0)
			nflags |= kt::START_ALL;
		else if (torrents > 0)
			nflags |= kt::START_ALL | kt::STOP_ALL;
			
		if (flags != nflags)
		{
			flags = nflags;
			enableActions(this,(ActionEnableFlags)flags);
		}
		
		
		if (model->updated())
			proxy_model->invalidate();
		
		// update the caption
		if (num_running != running || num_torrents != torrents)
		{
			num_running = running;
			num_torrents = torrents;
			return true;
		}
		
		return false;
	}

	bool View::needToUpdateCaption()
	{
		Uint32 torrents = 0;
		Uint32 running = 0;
		QList<bt::TorrentInterface*> all;
		model->allTorrents(all);
		foreach (bt::TorrentInterface* ti,all)
		{
			if (!group || (group && group->isMember(ti)))
			{
				torrents++;
				if (ti->getStats().running)
					running++;
			}
		}

		if (num_running != running || num_torrents != torrents)
		{
			num_running = running;
			num_torrents = torrents;
			return true;
		}
		
		return false;
	}
	
	QString View::caption() const
	{
		return QString("%1 %2/%3").arg(group->groupName()).arg(num_running).arg(num_torrents);
	}

	void View::startTorrents()
	{
		QList<bt::TorrentInterface*> sel;
		getSelection(sel);
		wantToStart(sel);
	}

	void View::stopTorrents()
	{
		QList<bt::TorrentInterface*> sel;
		getSelection(sel);
		foreach(bt::TorrentInterface* tc,sel)
		{
			wantToStop(tc,true);
		}
	}

	void View::removeTorrents()
	{
		QList<bt::TorrentInterface*> sel;
		getSelection(sel);
		foreach(bt::TorrentInterface* tc,sel)
		{
			bool dummy;
			if (tc && !tc->isCheckingData(dummy))
			{	
				const TorrentStats & s = tc->getStats();
				bool data_to = false;
				if (!s.completed)
				{
					QString msg = i18n("The torrent <b>%1</b> has not finished downloading, "
							"do you want to delete the incomplete data, too?",s.torrent_name);
					int ret = KMessageBox::questionYesNoCancel(
							this,
							msg,
							i18n("Remove Download"),
							KGuiItem(i18n("Delete Data")),
							KGuiItem(i18n("Keep Data")),
							KStandardGuiItem::cancel());
					if (ret == KMessageBox::Cancel)
						return;
					else if (ret == KMessageBox::Yes)
						data_to = true;
				}
				wantToRemove(tc,data_to);
			}
		}
	}

	void View::removeTorrentsAndData()
	{
		QList<bt::TorrentInterface*> sel;
		getSelection(sel);
		if (sel.count() == 0)
			return;

		QString msg = i18n("You will lose all the downloaded data. Are you sure you want to do this?");
		if (KMessageBox::warningYesNo(this,msg, i18n("Remove Torrent"), KStandardGuiItem::remove(),KStandardGuiItem::cancel()) == KMessageBox::No)
			return;

		foreach(bt::TorrentInterface* tc,sel)
		{
			bool dummy = false;
			if (tc && !tc->isCheckingData(dummy))
				wantToRemove(tc,true);
		}
	}

	void View::startAllTorrents()
	{
		QList<bt::TorrentInterface*> all;
		model->allTorrents(all);
		wantToStart(all);
	}

	void View::stopAllTorrents()
	{
		QList<bt::TorrentInterface*> all;
		model->allTorrents(all);
		foreach (bt::TorrentInterface* tc,all)
		{
			wantToStop(tc,true);
		}
	}

	void View::queueTorrents()
	{
		QList<bt::TorrentInterface*> sel;
		getSelection(sel);
		foreach(bt::TorrentInterface* tc,sel)
		{
			bool dummy;
			if (tc && !tc->isCheckingData(dummy))
				core->queue(tc);
		}
	}

	void View::addPeers()
	{
		QList<bt::TorrentInterface*> sel;
		getSelection(sel);
		if (sel.count() > 0)
		{
			AddPeersDlg dlg(sel[0],this);
			dlg.exec();
		}
	}

	void View::manualAnnounce()
	{
		QList<bt::TorrentInterface*> sel;
		getSelection(sel);
		foreach(bt::TorrentInterface* tc,sel)
		{
			if (tc->getStats().running) 
				tc->updateTracker();
		}
	}

	void View::scrape()
	{
		QList<bt::TorrentInterface*> sel;
		getSelection(sel);
		foreach(bt::TorrentInterface* tc,sel)
		{
			tc->scrapeTracker();
		}
	}

	void View::previewTorrents()
	{
		QList<bt::TorrentInterface*> sel;
		getSelection(sel);
		foreach(bt::TorrentInterface* tc,sel)
		{
			if (tc->readyForPreview() && !tc->getStats().multi_file_torrent)
			{
				QFileInfo fi(tc->getTorDir()+"cache");                          
				new KRun(KUrl(fi.symLinkTarget()), 0, 0, true, true);
			}
		}
	}

	void View::openDataDir()
	{
		QList<bt::TorrentInterface*> sel;
		getSelection(sel);
		foreach(bt::TorrentInterface* tc,sel)
		{
			if (tc->getStats().multi_file_torrent)
				new KRun(KUrl(tc->getStats().output_path), 0, 0, true, true);
			else
				new KRun(KUrl(tc->getDataDir()), 0, 0, true, true);
		}
	}
	
	void View::openTorDir()
	{
		QList<bt::TorrentInterface*> sel;
		getSelection(sel);
		foreach(bt::TorrentInterface* tc,sel)
		{
			new KRun(KUrl(tc->getTorDir()), 0, 0, true, true);
		}
	}
	
	void View::moveData()
	{
		QList<bt::TorrentInterface*> sel;
		getSelection(sel);
		if (sel.count() == 0)
			return;
		
		QString dir = KFileDialog::getExistingDirectory(KUrl("kfiledialog:///openTorrent"),this,i18n("Select a directory to move the data to."));
		if (dir.isNull())
			return;
		
		foreach(bt::TorrentInterface* tc,sel)
		{
			tc->changeOutputDir(dir,bt::TorrentInterface::MOVE_FILES);
		}
	}
	
	void View::removeFromGroup()
	{
		if (!group || group->isStandardGroup())
			return;

		QList<bt::TorrentInterface*> sel;
		getSelection(sel);
		foreach(bt::TorrentInterface* tc,sel)
			group->removeTorrent(tc);

		update();
	}
	
	void View::speedLimitsDlg()
	{
		QList<bt::TorrentInterface*> sel;
		getSelection(sel);
		SpeedLimitsDlg dlg(sel.count() > 0 ? sel.front() : 0,core,this);
		dlg.exec();
	}

	void View::toggleDHT()
	{
		QList<bt::TorrentInterface*> sel;
		getSelection(sel);
		foreach(bt::TorrentInterface* tc,sel)
		{
			bool dummy;
			if (tc && !tc->isCheckingData(dummy))
			{
				bool on = tc->isFeatureEnabled(bt::DHT_FEATURE);
				tc->setFeatureEnabled(bt::DHT_FEATURE,!on);
			}
		}							
	}

	void View::togglePEX()
	{
		QList<bt::TorrentInterface*> sel;
		getSelection(sel);
		foreach(bt::TorrentInterface* tc,sel)
		{
			bool dummy;
			if (tc && !tc->isCheckingData(dummy))
			{
				bool on = tc->isFeatureEnabled(bt::UT_PEX_FEATURE);
				tc->setFeatureEnabled(bt::UT_PEX_FEATURE,!on);
			}
		}
	}

	void View::checkData()
	{
		QList<bt::TorrentInterface*> sel;
		getSelection(sel);
		if (sel.count() == 0)
			return;
		
		ScanDlg* dlg = new ScanDlg(core,false,this);
		dlg->show();
		dlg->execute(sel.front(),false);
		core->startUpdateTimer(); // make sure update timer of core is running
	}

	void View::showMenu(const QPoint & pos)
	{
		menu->show(mapToGlobal(pos));
	}
	
	void View::showHeaderMenu(const QPoint& pos)
	{
		header_menu->popup(header()->mapToGlobal(pos));
	}

	void View::getSelection(QList<bt::TorrentInterface*> & sel)
	{
		QModelIndexList indices = selectionModel()->selectedRows();
		for (QModelIndexList::iterator i = indices.begin();i != indices.end();i++)
			*i = proxy_model->mapToSource(*i);
			
		model->torrentsFromIndexList(indices,sel);
	}

	void View::saveState(KSharedConfigPtr cfg,int idx)
	{
		KConfigGroup g = cfg->group(QString("View%1").arg(idx));
		QByteArray s = header()->saveState();
		g.writeEntry("state",s.toBase64());
	}


	void View::loadState(KSharedConfigPtr cfg,int idx)
	{
		KConfigGroup g = cfg->group(QString("View%1").arg(idx));
		QByteArray s = QByteArray::fromBase64(g.readEntry("state",QByteArray()));
		if (!s.isNull())
			header()->restoreState(s);
		
		QMap<QAction*,int>::iterator i = column_idx_map.begin();
		while (i != column_idx_map.end())
		{
			QAction* act = i.key();
			act->setChecked(!header()->isSectionHidden(i.value()));
			i++;
		}
	}

	bt::TorrentInterface* View::getCurrentTorrent()
	{
		return model->torrentFromIndex(proxy_model->mapToSource(selectionModel()->currentIndex()));
	}

	void View::onCurrentItemChanged(const QModelIndex & current,const QModelIndex & /*previous*/)
	{
		//Out(SYS_GEN|LOG_DEBUG) << "onCurrentItemChanged " << current.row() << endl;
		bt::TorrentInterface* tc = model->torrentFromIndex(proxy_model->mapToSource(current));
		currentTorrentChanged(this,tc);
	}

	void View::onHeaderMenuItemTriggered(QAction* act)
	{
		int idx = column_idx_map[act];
		if (act->isChecked())
			header()->showSection(idx);
		else
			header()->hideSection(idx);
	}
	
	void View::onSelectionChanged(const QItemSelection & /*selected*/,const QItemSelection & /*deselected*/)
	{
		int nflags = flags & (kt::START_ALL | kt::STOP_ALL);
		QList<bt::TorrentInterface*> sel;
		getSelection(sel);
		
		foreach (bt::TorrentInterface* tc,sel)
		{
			if (!tc->getStats().running)
				nflags |= kt::START | kt::START_ALL;
			else
				nflags |= kt::STOP | kt::STOP_ALL;
		}
		
		if (sel.count() > 0)
			nflags |= kt::REMOVE;
		
		if (flags != nflags)
		{
			flags = nflags;
			enableActions(this,(kt::ActionEnableFlags)flags);
		}
	}
	
	void View::updateFlags()
	{
		int nflags = flags & (kt::START_ALL | kt::STOP_ALL);
		QList<bt::TorrentInterface*> sel;
		getSelection(sel);
		
		foreach (bt::TorrentInterface* tc,sel)
		{
			if (!tc->getStats().running)
				nflags |= kt::START | kt::START_ALL;
			else
				nflags |= kt::STOP | kt::STOP_ALL;
		}
		
		if (sel.count() > 0)
			nflags |= kt::REMOVE;
		
		
		flags = nflags;
		enableActions(this,(kt::ActionEnableFlags)flags);
	}
}

#include "view.moc"

