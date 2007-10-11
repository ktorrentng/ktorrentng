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
#include <klocale.h>
#include <interfaces/torrentinterface.h>
#include <torrent/queuemanager.h>
#include "core.h"
#include "speedlimitsmodel.h"

namespace kt
{
	SpeedLimitsModel::SpeedLimitsModel(Core* core,QObject* parent) : QAbstractTableModel(parent),core(core)
	{
		bt::QueueManager* qman = core->getQueueManager();
		QList<kt::TorrentInterface*>::iterator itr = qman->begin();
		while (itr != qman->end())
		{
			Limits lim;
			TorrentInterface* tc = *itr;
			tc->getTrafficLimits(lim.up_original,lim.down_original);
			lim.down = lim.down_original;
			lim.up = lim.up_original;
			limits.insert(tc,lim);
			itr++;
		}
		
		connect(core,SIGNAL(torrentAdded(kt::TorrentInterface*)),this,SLOT(onTorrentAdded(kt::TorrentInterface*)));
		connect(core,SIGNAL(torrentRemoved(kt::TorrentInterface*)),this,SLOT(onTorrentRemoved(kt::TorrentInterface*)));
	}
		
	SpeedLimitsModel::~SpeedLimitsModel() 
	{}
	
	void SpeedLimitsModel::onTorrentAdded(kt::TorrentInterface* tc)
	{
		Limits lim;
		tc->getTrafficLimits(lim.up_original,lim.down_original);
		lim.down = lim.down_original;
		lim.up = lim.up_original;
		limits.insert(tc,lim);
		insertRow(limits.count() - 1);
	}
	
	void SpeedLimitsModel::onTorrentRemoved(kt::TorrentInterface* tc)
	{
		bt::QueueManager* qman = core->getQueueManager();
		int idx = 0;
		QList<kt::TorrentInterface*>::iterator itr = qman->begin();
		while (itr != qman->end())
		{
			if (*itr == tc)
				break;
			idx++;
			itr++;
		}
		
		limits.remove(tc);
		removeRow(idx);
	}
	
	int SpeedLimitsModel::rowCount(const QModelIndex & parent) const  
	{
		if (parent.isValid())
			return 0;
		else
			return core->getQueueManager()->count();
	}
		
	int SpeedLimitsModel::columnCount(const QModelIndex & parent) const  
	{
		if (parent.isValid())
			return 0;
		else
			return 3;
	}
		
	QVariant SpeedLimitsModel::headerData(int section, Qt::Orientation orientation,int role) const
	{
		if (role != Qt::DisplayRole || orientation != Qt::Horizontal)
			return QVariant();
			
		switch (section)
		{
			case 0: return i18n("Torrent");
			case 1: return i18n("Download Limit");
			case 2: return i18n("Upload Limit");
			default:
				return QVariant();
		}
	}
		
	QVariant SpeedLimitsModel::data(const QModelIndex & index, int role) const
	{
		if (role != Qt::DisplayRole && role != Qt::EditRole)
			return QVariant();
			
		kt::TorrentInterface* tc = torrentForIndex(index);
		if (!tc)
			return QVariant();
		
		const Limits & lim = limits[tc];
		
		switch (index.column())
		{
			case 0: return tc->getStats().torrent_name;
			case 1: 
				if (role == Qt::EditRole)
					return lim.down / 1024;
				else
					return lim.down == 0 ? i18n("No limit") : i18n("%1 KB/s",lim.down / 1024);
			case 2: 
				if (role == Qt::EditRole)
					return lim.up / 1024;
				else 
					return lim.up == 0 ? i18n("No limit") : i18n("%1 KB/s",lim.up / 1024);
			default: return QVariant();
		}
	}
		
	bool SpeedLimitsModel::setData(const QModelIndex & index,const QVariant & value,int role)
	{
		if (role != Qt::EditRole)
			return false;
			
		kt::TorrentInterface* tc = torrentForIndex(index);
		if (!tc || !limits.contains(tc))
			return false;
			
		bool ok = false;
		Uint32 up,down;
		tc->getTrafficLimits(up,down);
		if (index.column() == 1)
			down = value.toInt(&ok) * 1024;
		else
			up = value.toInt(&ok) * 1024;
			
		if (ok)
		{
			Limits & lim = limits[tc];
			lim.up = up;
			lim.down = down;
			
			emit dataChanged(index, index);
			if (lim.up != lim.up_original || lim.down != lim.down_original)
				enableApply(true);
		}
			
		return ok;
	}
		
	Qt::ItemFlags SpeedLimitsModel::flags(const QModelIndex & index) const
	{
		if (!index.isValid())
			return Qt::ItemIsEnabled;

		if (index.column() > 0)
			return QAbstractItemModel::flags(index) | Qt::ItemIsEditable;
		else
			return QAbstractItemModel::flags(index);
	}
		
	kt::TorrentInterface* SpeedLimitsModel::torrentForIndex(const QModelIndex & index) const
	{
		bt::QueueManager* qman = core->getQueueManager();
		int r = index.row();			
		QList<kt::TorrentInterface*>::iterator itr = qman->begin();
		itr += r;
		
		if (itr == qman->end())
			return 0;
		else
			return *itr;
	}
	
	void SpeedLimitsModel::apply()
	{
		QMap<kt::TorrentInterface*,Limits>::iterator itr = limits.begin();
		while (itr != limits.end())
		{
			TorrentInterface* tc = itr.key();
			Limits & lim = itr.value();
			if (lim.up != lim.up_original || lim.down != lim.down_original)
			{
				tc->setTrafficLimits(lim.up,lim.down);
				lim.up_original = lim.up;
				lim.down_original = lim.down;
			}
			itr++;
		}
	}
}

#include "speedlimitsmodel.moc"
