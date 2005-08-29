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
#include <klocale.h>
#include <kstandarddirs.h>
#include <kglobal.h>
#include <qlayout.h>
#include <qlabel.h>
#include <qcheckbox.h>
#include <knuminput.h>
#include <kurlrequester.h>
#include <kurl.h> 
#include <kfiledialog.h>
#include <kmessagebox.h>
#include <klineedit.h> 
#include <qlistview.h> 
#include <libtorrent/globals.h>
#include <libutil/functions.h>
#include <kglobal.h>
#include <kiconloader.h>
#include <qdir.h>

#include "pref.h"
#include "downloadpref.h"
#include "searchpref.h" 
#include "settings.h"
#include "ktorrent.h"

using namespace bt;



KTorrentPreferences::KTorrentPreferences(KTorrent & ktor)
	: KDialogBase(IconList, i18n("Preferences"),Ok|Apply|Cancel, Ok),ktor(ktor)
{
	enableButtonSeparator(true);
		
	KIconLoader* iload = KGlobal::iconLoader();
	
	QFrame* frame = addPage(i18n("Downloads"), i18n("Download Options"),
							iload->loadIcon("down",KIcon::NoGroup));
		
	QVBoxLayout* vbox = new QVBoxLayout(frame);
	vbox->setAutoAdd(true);
	page_one = new PrefPageOne(frame);

	frame = addPage(i18n("General"), i18n("General Options"),
					iload->loadIcon("package_settings",KIcon::NoGroup));
	vbox = new QVBoxLayout(frame);
	vbox->setAutoAdd(true);
	page_two = new PrefPageTwo(frame);

	frame = addPage(i18n("Search"), i18n("Search engine options"),
					iload->loadIcon("viewmag",KIcon::NoGroup));
	vbox = new QVBoxLayout(frame);
	vbox->setAutoAdd(true);
	page_three = new PrefPageThree(frame);
}

void KTorrentPreferences::slotOk()
{
	slotApply();
	if (page_one->checkPorts())
		accept();
}

void KTorrentPreferences::slotApply()
{
	if (!page_one->checkPorts())
	{
		KMessageBox::error(this,i18n("The UDP Tracker Port and Port cannot have the same value !"),
						   i18n("Error"));
	}
	else
	{
		page_one->apply();
		page_two->apply();
		page_three->apply(); 
		Settings::writeConfig();
		ktor.applySettings();
	}
}

///////////////////////////////////////////////////////

PrefPageOne::PrefPageOne(QWidget *parent) : DownloadPref(parent)
{
	//setMinimumSize(400,400);
	max_downloads->setValue(Settings::maxDownloads());
	max_conns->setValue(Settings::maxConnections());
	max_upload_rate->setValue(Settings::maxUploadRate());
	max_download_rate->setValue(Settings::maxDownloadRate());
	keep_seeding->setChecked(Settings::keepSeeding());
	udp_tracker_port->setValue(Settings::udpTrackerPort());
	port->setValue(Settings::port());
}

bool PrefPageOne::checkPorts()
{
	if (udp_tracker_port->value() == port->value())
		return false;
	else
		return true;
}

void PrefPageOne::apply()
{
	Settings::setMaxDownloads(max_downloads->value());
	Settings::setMaxConnections(max_conns->value());
	Settings::setMaxUploadRate(max_upload_rate->value());
	Settings::setMaxDownloadRate(max_download_rate->value());
	Settings::setKeepSeeding(keep_seeding->isChecked());	
	Settings::setPort(port->value());
	Settings::setUdpTrackerPort(udp_tracker_port->value());
}

//////////////////////////////////////
PrefPageTwo::PrefPageTwo(QWidget *parent) : GeneralPref(parent)
{
	show_systray_icon->setChecked(Settings::showSystemTrayIcon());
	KURLRequester* u = temp_dir;
	u->fileDialog()->setMode(KFile::Directory);
	if (Settings::tempDir() == QString::null)
	{
		QString data_dir = KGlobal::dirs()->saveLocation("data","ktorrent");
		if (!data_dir.endsWith(bt::DirSeparator()))
			data_dir += bt::DirSeparator();
		u->setURL(data_dir);
	}
	else
	{
		u->setURL(Settings::tempDir());
	}

	u = autosave_location;
	u->fileDialog()->setMode(KFile::Directory);
	if (Settings::saveDir() == QString::null)
	{
		autosave_downloads_check->setChecked(false);
		u->setEnabled(false);
		u->clear();
	}
	else
	{
		autosave_downloads_check->setChecked(true);
		u->setURL(QDir::homeDirPath());
		u->setURL(Settings::saveDir());
		u->setEnabled(true);
	}
	connect(autosave_downloads_check,SIGNAL(toggled(bool)),
			this,SLOT(autosaveChecked(bool )));
}

void PrefPageTwo::apply()
{
	Settings::setShowSystemTrayIcon(show_systray_icon->isChecked());
	QString ourl = Settings::tempDir();
	
	KURLRequester* u = temp_dir;
	if (ourl != u->url())
	{
		Settings::setTempDir(u->url());
	}

	if (autosave_downloads_check->isChecked())
	{
		u = autosave_location;
		Settings::setSaveDir(u->url());
	}
	else
	{
		Settings::setSaveDir(QString::null);
	}
}

void PrefPageTwo::autosaveChecked(bool on)
{
	KURLRequester* u = autosave_location;
	if (on)
	{
		u->setEnabled(true);
		if (Settings::saveDir() == QString::null)
			u->setURL(QDir::homeDirPath());
		else
			u->setURL(Settings::saveDir());
	}
	else
	{
		u->setEnabled(false);
		u->clear();
	}
}
//////////////////////////////////////////////////////////////////////////////////////// 

PrefPageThree::PrefPageThree(QWidget *parent) : SEPreferences(parent) 
{ 
    loadSearchEngines(); 
    connect(btnAdd, SIGNAL(clicked()), this, SLOT(addClicked())); 
    connect(btnRemove, SIGNAL(clicked()), this, SLOT(removeClicked())); 
    connect(btn_add_default, SIGNAL(clicked()), this, SLOT(addDefaultClicked())); 
    connect(btnRemoveAll, SIGNAL(clicked()), this, SLOT(removeAllClicked())); 
} 
 
void PrefPageThree::apply() 
{ 
    saveSearchEngines(); 
} 
 
void PrefPageThree::loadSearchEngines() 
{ 
    QFile fptr(KGlobal::dirs()->saveLocation("data","ktorrent") + "search_engines"); 
     
    if (!fptr.open(IO_ReadOnly)) 
        return; 
 
    QTextStream in(&fptr); 
     
    int id = 0; 
     
    while (!in.atEnd()) 
    { 
        QString line = in.readLine(); 
         
        if(line.startsWith("#") || line.startsWith(" ") || line.isEmpty() ) continue; 
         
        QStringList tokens = QStringList::split(" ", line); 
         
         
        KURL url = KURL::fromPathOrURL(tokens[1]); 
		for(Uint32 i=2; i<tokens.count(); ++i)
			url.addQueryItem(tokens[i].section("=",0,0), tokens[i].section("=", 1, 1));
		
        QListViewItem* se = new QListViewItem(m_engines, tokens[0], url.url()); 
        m_items.append(se); 
        m_engines->insertItem(se); 
         
        ++id; 
    } 
} 
 
void PrefPageThree::saveSearchEngines() 
{ 
	QFile fptr(KGlobal::dirs()->saveLocation("data","ktorrent") + "search_engines");
	if (!fptr.open(IO_WriteOnly))
		return;
	QTextStream out(&fptr);
	out << "# PLEASE DO NOT MODIFY THIS FILE. Use KTorrent configuration dialog for adding new search engines." << ::endl;
	out << "# SEARCH ENGINES list" << ::endl;
     
	for(Uint32 i=0; i<m_items.count(); ++i)
	{
		QListViewItem* item = m_items.at(i);
		QString u = item->text(1);
		QMap<QString,QString> args = KURL(u).queryItems();
		
		out << item->text(0) << " " << u.section("?",0,0) << " ";
		for (QMap<QString,QString>::iterator j = args.begin();j != args.end();j++)
			out << j.key() << "=" << j.data() << " ";
		out << endl;
	} 
} 
 
void PrefPageThree::addClicked() 
{ 
    if ( m_engine_url->text().isEmpty() || m_engine_name->text().isEmpty() ) 
        KMessageBox::error(this, i18n("You must enter SE name and URL"), i18n("Error")); 
    else if ( m_engine_url->text().contains("$QUERY")  ) 
    { 
        KURL url = KURL::fromPathOrURL(m_engine_url->text()); 
        if ( !url.isValid() ) { KMessageBox::error(this, i18n("Malformed URL."), i18n("Error")); return; } 
        QListViewItem* se = new QListViewItem(m_engines, m_engine_name->text(), m_engine_url->text()); 
        m_engines->insertItem(se); 
        m_items.append(se); 
    } 
    else 
        KMessageBox::error(this, i18n("Bad URL. You should search for $QUERY with your internet browser and copy/paste exact URL here.")); 
} 
 
void PrefPageThree::removeClicked() 
{ 
    QListView trt; 
    if ( m_engines->selectedItem() == 0 ) return; 
     
    QListViewItem* item = m_engines->selectedItem(); 
    m_engines->takeItem(item); 
    m_items.remove(item); 
} 
 
void PrefPageThree::addDefaultClicked() 
{ 
    QListViewItem* se = new QListViewItem(m_engines, "bittorrent.com", "http://search.bittorrent.com/search.jsp?query=$QUERY"); 
    m_items.append(se); 
    m_engines->insertItem(se); 
     
    se = new QListViewItem(m_engines, "isohunt.com", "http://isohunt.com/torrents.php?ihq=$QUERY&op=and"); 
    m_items.append(se); 
    m_engines->insertItem(se); 
     
    se = new QListViewItem(m_engines, "mininova.org", "http://www.mininova.org/search.php?search=$QUERY"); 
    m_items.append(se); 
    m_engines->insertItem(se); 
     
    se = new QListViewItem(m_engines, "thepiratebay.org", "http://thepiratebay.org/search.php?q=$QUERY"); 
    m_items.append(se); 
    m_engines->insertItem(se); 
     
    se = new QListViewItem(m_engines, "bitoogle.com", "http://search.bitoogle.com/search.php?q=$QUERY&st=t"); 
    m_items.append(se); 
    m_engines->insertItem(se); 
     
    se = new QListViewItem(m_engines, "bytenova.org", "http://www.bytenova.org/search.php?search=$QUERY"); 
    m_items.append(se); 
    m_engines->insertItem(se); 
     
    se = new QListViewItem(m_engines, "torrentspy.com", "http://torrentspy.com/search.asp?query=$QUERY"); 
    m_items.append(se); 
    m_engines->insertItem(se); 
} 
 
void PrefPageThree::removeAllClicked() 
{ 
    m_engines->clear(); 
    m_items.clear(); 
} 
#include "pref.moc"
