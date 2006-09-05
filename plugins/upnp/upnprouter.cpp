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
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.             *
 ***************************************************************************/
#include <stdlib.h>
#include <klocale.h>
#include <kglobal.h>
#include <kstandarddirs.h>
#include <qstringlist.h>
#include <kio/netaccess.h>
#include <kio/job.h>
#include <torrent/globals.h>
#include <util/log.h>
#include <util/array.h>
#include <util/error.h>
#include <util/functions.h>
#include <util/fileops.h>
#include <util/httprequest.h>
#include "upnprouter.h"
#include "upnpdescriptionparser.h"
#include "soap.h"

using namespace bt;

namespace kt 
{
	UPnPService::UPnPService()
	{
	}
	
	UPnPService::UPnPService(const UPnPService & s)
	{
		this->servicetype = s.servicetype;
		this->controlurl = s.controlurl;
		this->eventsuburl = s.eventsuburl;
		this->serviceid = s.serviceid;
		this->scpdurl = s.scpdurl;
	}

	void UPnPService::setProperty(const QString & name,const QString & value)
	{
		if (name == "serviceType")
			servicetype = value;
		else if (name == "controlURL")
			controlurl = value;
		else if (name == "eventSubURL")
			eventsuburl = value;
		else if (name == "SCPDURL")
			scpdurl = value;
		else if (name == "serviceId")
			serviceid = value;
	}
	
	void UPnPService::clear()
	{
		servicetype = controlurl = eventsuburl = scpdurl = serviceid = "";
	}
	
	void UPnPService::debugPrintData()
	{
		Out(SYS_PNP|LOG_DEBUG) << "    servicetype = " << servicetype << endl;
		Out(SYS_PNP|LOG_DEBUG) << "    controlurl = " << controlurl << endl;
		Out(SYS_PNP|LOG_DEBUG) << "    eventsuburl = " << eventsuburl << endl;
		Out(SYS_PNP|LOG_DEBUG) << "    scpdurl = " << scpdurl << endl;
		Out(SYS_PNP|LOG_DEBUG) << "    serviceid = " << serviceid << endl;
	}
	
	UPnPService & UPnPService::operator = (const UPnPService & s)
	{
		this->servicetype = s.servicetype;
		this->controlurl = s.controlurl;
		this->eventsuburl = s.eventsuburl;
		this->serviceid = s.serviceid;
		this->scpdurl = s.scpdurl;
		return *this;
	}
	
	///////////////////////////////////////
	
	void UPnPDeviceDescription::setProperty(const QString & name,const QString & value)
	{
		if (name == "friendlyName")
			friendlyName = value;
		else if (name == "manufacturer")
			manufacturer = value;
		else if (name == "modelDescription")
			modelDescription = value;
		else if (name == "modelName")
			modelName = value;
		else if (name == "modelNumber")
			modelNumber == value;
	}
	
	///////////////////////////////////////
	
	UPnPRouter::UPnPRouter(const QString & server,const KURL & location,bool verbose) : server(server),location(location),verbose(verbose)
	{
		// make the tmp_file unique, current time * a random number should be enough
		tmp_file = QString("/tmp/ktorrent_upnp_description-%1.xml").arg(bt::GetCurrentTime() * rand());
	}
	
	
	UPnPRouter::~UPnPRouter()
	{
		QValueList<HTTPRequest*>::iterator i = active_reqs.begin();
		while (i != active_reqs.end())
		{
			(*i)->deleteLater();
			i++;
		}
	}
	
	void UPnPRouter::addService(const UPnPService & s)
	{
		services.append(s);
	}
	
	void UPnPRouter::downloadFinished(KIO::Job* j)
	{
		if (j->error())
		{
			Out(SYS_PNP|LOG_IMPORTANT) << "Failed to download " << location << " : " << j->errorString() << endl;
			return;
		}
		
		QString target = tmp_file;
		// load in the file (target is always local)
		UPnPDescriptionParser desc_parse;
		bool ret = desc_parse.parse(target,this);
		if (!ret)
		{
			Out(SYS_PNP|LOG_IMPORTANT) << "Error parsing router description !" << endl;
			QString dest = KGlobal::dirs()->saveLocation("data","ktorrent") + "upnp_failure";
			KIO::file_copy(target,dest,-1,true,false,false);
		}
		else
		{
			if (verbose)
				debugPrintData();
		}
		xmlFileDownloaded(this,ret);
		bt::Delete(target);
	}
	
	void UPnPRouter::downloadXMLFile()
	{
		// downlaod XML description into a temporary file in /tmp
		KIO::Job* job = KIO::file_copy(location,tmp_file,-1,true,false,false);
		connect(job,SIGNAL(result(KIO::Job *)),this,SLOT(downloadFinished( KIO::Job* )));
	}
	
	void UPnPRouter::debugPrintData()
	{
		Out(SYS_PNP|LOG_DEBUG) << "UPnPRouter : " << endl;
		Out(SYS_PNP|LOG_DEBUG) << "Friendly name = " << desc.friendlyName << endl;
		Out(SYS_PNP|LOG_DEBUG) << "Manufacterer = " << desc.manufacturer << endl;
		Out(SYS_PNP|LOG_DEBUG) << "Model description = " << desc.modelDescription << endl;
		Out(SYS_PNP|LOG_DEBUG) << "Model name = " << desc.modelName << endl;
		Out(SYS_PNP|LOG_DEBUG) << "Model number = " << desc.modelNumber << endl;
		for (QValueList<UPnPService>::iterator i = services.begin();i != services.end();i++)
		{
			UPnPService & s = *i;
			Out() << "Service : " << endl;
			s.debugPrintData();
			Out(SYS_PNP|LOG_DEBUG) << "Done" << endl;
		}
		Out(SYS_PNP|LOG_DEBUG) << "Done" << endl;
	}
	
	QValueList<UPnPService>::iterator UPnPRouter::findPortForwardingService()
	{
		QValueList<UPnPService>::iterator i = services.begin();
		while (i != services.end())
		{
			UPnPService & s = *i;
			if (s.servicetype == "urn:schemas-upnp-org:service:WANIPConnection:1" ||
				s.servicetype == "urn:schemas-upnp-org:service:WANPPPConnection:1")
				return i;
			i++;
		}
		return services.end();
	}

	void UPnPRouter::getExternalIP()
	{
		// first find the right service
		QValueList<UPnPService>::iterator i = findPortForwardingService();
		if (i == services.end())
			throw Error(i18n("Cannot find port forwarding service in the device's description!"));
		
		UPnPService & s = *i;
		QString action = "GetExternalIPAddress";
		QString comm = SOAP::createCommand(action,s.servicetype);
		sendSoapQuery(comm,s.servicetype + "#" + action,s.controlurl);
	}
	
	void UPnPRouter::forward(Uint16 port,Protocol prot)
	{
		if (verbose)
			Out(SYS_PNP|LOG_NOTICE) << "Forwarding port " << port << " (" << (prot == UDP ? "UDP" : "TCP") << ")" << endl;
		// first find the right service
		QValueList<UPnPService>::iterator i = findPortForwardingService();
		if (i == services.end())
			throw Error(i18n("Cannot find port forwarding service in the device's description!"));
		
		// add all the arguments for the command
		QValueList<SOAP::Arg> args;
		SOAP::Arg a;
		a.element = "NewRemoteHost";
		args.append(a);
		
		// the external port
		a.element = "NewExternalPort";
		a.value = QString::number(port);
		args.append(a);
		
		// the protocol
		a.element = "NewProtocol";
		a.value = prot == TCP ? "TCP" : "UDP";
		args.append(a);
		
		// the local port
		a.element = "NewInternalPort";
		a.value = QString::number(port);
		args.append(a);
		
		// the local IP address
		a.element = "NewInternalClient";
		a.value = "$LOCAL_IP";// will be replaced by our local ip in bt::HTTPRequest
		args.append(a);
		
		a.element = "NewEnabled";
		a.value = "1";
		args.append(a);
		
		a.element = "NewPortMappingDescription";
		static Uint32 cnt = 0;
		a.value = QString("KTorrent UPNP %1").arg(cnt++);	// TODO: change this
		args.append(a);
		
		a.element = "NewLeaseDuration";
		a.value = "0";
		args.append(a);
		
		UPnPService & s = *i;
		QString action = "AddPortMapping";
		QString comm = SOAP::createCommand(action,s.servicetype,args);
		
		Forwarding fw = {port,prot,true};
		// erase old forwarding if one exists
		QValueList<Forwarding>::iterator itr = fwds.begin();
		while (itr != fwds.end())
		{
			Forwarding & fwo = *itr;
			if (fwo.port == port && fwo.prot == prot)
				itr = fwds.erase(itr);
			else
				itr++;
		}
		
		bt::HTTPRequest* r = sendSoapQuery(comm,s.servicetype + "#" + action,s.controlurl);
		reqs[r] = fwds.append(fw);
	}
	
	void UPnPRouter::undoForward(Uint16 port,Protocol prot)
	{
		Out(SYS_PNP|LOG_NOTICE) << "Undoing forward of port " << port << " (" << (prot == UDP ? "UDP" : "TCP") << ")" << endl;
		// first find the right service
		QValueList<UPnPService>::iterator i = findPortForwardingService();
		if (i == services.end())
			throw Error(i18n("Cannot find port forwarding service in the device's description!"));
		
		// add all the arguments for the command
		QValueList<SOAP::Arg> args;
		SOAP::Arg a;
		a.element = "NewRemoteHost";
		args.append(a);
		
		// the external port
		a.element = "NewExternalPort";
		a.value = QString::number(port);
		args.append(a);
		
		// the protocol
		a.element = "NewProtocol";
		a.value = prot == TCP ? "TCP" : "UDP";
		args.append(a);
		
		UPnPService & s = *i;
		QString action = "DeletePortMapping";
		QString comm = SOAP::createCommand(action,s.servicetype,args);
		sendSoapQuery(comm,s.servicetype + "#" + action,s.controlurl);
		QValueList<Forwarding>::iterator itr = fwds.begin();
		while (itr != fwds.end())
		{
			Forwarding & wd = *itr;
			if (wd.port == port && wd.prot == prot)
			{
				fwds.erase(itr);
				break;
			}
			itr++;
		}
		updateGUI();
	}
	
	void UPnPRouter::isPortForwarded(Uint16 port,Protocol prot)
	{
		// first find the right service
		QValueList<UPnPService>::iterator i = findPortForwardingService();
		if (i == services.end())
			throw Error(i18n("Cannot find port forwarding service in the device's description!"));
		
		// add all the arguments for the command
		QValueList<SOAP::Arg> args;
		SOAP::Arg a;
		a.element = "NewRemoteHost";
		args.append(a);
		
		// the external port
		a.element = "NewExternalPort";
		a.value = QString::number(port);
		args.append(a);
		
		// the protocol
		a.element = "NewProtocol";
		a.value = prot == TCP ? "TCP" : "UDP";
		args.append(a);
		
		UPnPService & s = *i;
		QString action = "GetSpecificPortMappingEntry";
		QString comm = SOAP::createCommand(action,s.servicetype,args);
		sendSoapQuery(comm,s.servicetype + "#" + action,s.controlurl);
	}
	
	bt::HTTPRequest* UPnPRouter::sendSoapQuery(const QString & query,const QString & soapact,const QString & controlurl)
	{
		QString http_hdr = QString(
				"POST %1 HTTP/1.1\r\n"
				"HOST: %2:%3\r\n"
				"Content-length: $CONTENT_LENGTH\r\n"
				"Content-Type: text/xml\r\n"
				"SOAPAction: \"%4\"\r\n"
				"\r\n").arg(controlurl).arg(location.host()).arg(location.port()).arg(soapact);

		
		HTTPRequest* r = new HTTPRequest(http_hdr,query,location.host(),location.port(),verbose);
		connect(r,SIGNAL(replyError(bt::HTTPRequest* ,const QString& )),
				this,SLOT(onReplyError(bt::HTTPRequest* ,const QString& )));
		connect(r,SIGNAL(replyOK(bt::HTTPRequest* ,const QString& )),
				this,SLOT(onReplyOK(bt::HTTPRequest* ,const QString& )));
		connect(r,SIGNAL(error(bt::HTTPRequest*, bool )),
				this,SLOT(onError(bt::HTTPRequest*, bool )));
		r->start();
		active_reqs.append(r);
		return r;
	}
	

	void UPnPRouter::onReplyOK(bt::HTTPRequest* r,const QString &)
	{
		if (verbose)
			Out(SYS_PNP|LOG_NOTICE) << "UPnPRouter : OK" << endl;
		if (reqs.contains(r))
		{
			(*reqs[r]).pending = false;
			reqs.erase(r);
		}
		updateGUI();
		active_reqs.remove(r);
		r->deleteLater();
	}
	
	void UPnPRouter::onReplyError(bt::HTTPRequest* r,const QString &)
	{
		if (verbose)
			Out(SYS_PNP|LOG_IMPORTANT) << "UPnPRouter : Error" << endl;
		if (reqs.contains(r))
		{
			fwds.erase(reqs[r]);
			reqs.erase(r);
		}
		updateGUI();
		active_reqs.remove(r);
		r->deleteLater();
	}
	
	void UPnPRouter::onError(bt::HTTPRequest* r,bool)
	{
		if (reqs.contains(r))
		{
			fwds.erase(reqs[r]);
			reqs.erase(r);
		}
		updateGUI();
		active_reqs.remove(r);
		r->deleteLater();
	}
	
}

#include "upnprouter.moc"
