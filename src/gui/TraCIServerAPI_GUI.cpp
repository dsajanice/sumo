/****************************************************************************/
/// @file    TraCIServerAPI_GUI.cpp
/// @author  Daniel Krajzewicz
/// @date    07.05.2009
/// @version $Id$
///
// APIs for getting/setting GUI values via TraCI
/****************************************************************************/
// SUMO, Simulation of Urban MObility; see http://sumo.sourceforge.net/
// Copyright (C) 2001-2011 DLR (http://www.dlr.de/) and contributors
/****************************************************************************/
//
//   This program is free software; you can redistribute it and/or modify
//   it under the terms of the GNU General Public License as published by
//   the Free Software Foundation; either version 2 of the License, or
//   (at your option) any later version.
//
/****************************************************************************/


// ===========================================================================
// included modules
// ===========================================================================
#ifdef _MSC_VER
#include <windows_config.h>
#else
#include <config.h>
#endif

#ifndef NO_TRACI

#include <fx.h>
#include <utils/gui/windows/GUIMainWindow.h>
#include <utils/gui/windows/GUIGlChildWindow.h>
#include <utils/gui/windows/GUISUMOAbstractView.h>
#include <utils/gui/windows/GUIPerspectiveChanger.h>
#include <utils/foxtools/MFXImageHelper.h>
#include <traci-server/TraCIConstants.h>
#include "TraCIServerAPI_GUI.h"
#include <guisim/GUINet.h>

#ifdef CHECK_MEMORY_LEAKS
#include <foreign/nvwa/debug_new.h>
#endif // CHECK_MEMORY_LEAKS


// ===========================================================================
// used namespaces
// ===========================================================================
using namespace std;
using namespace traci;
using namespace tcpip;


// ===========================================================================
// method definitions
// ===========================================================================
bool
TraCIServerAPI_GUI::processGet(TraCIServer &server, tcpip::Storage &inputStorage,
                               tcpip::Storage &outputStorage) throw(TraCIException, std::invalid_argument) {
    std::string warning = ""; // additional description for response
    // variable & id
    int variable = inputStorage.readUnsignedByte();
    std::string id = inputStorage.readString();
    // check variable
    if (variable!=ID_LIST
            &&variable!=VAR_VIEW_ZOOM&&variable!=VAR_VIEW_OFFSET&&variable!=VAR_VIEW_SCHEMA&&variable!=VAR_VIEW_BOUNDARY
            &&variable!=VAR_VIEW_BACKGROUNDCOLOR&&variable!=VAR_NET_SIZE
       ) {
        server.writeStatusCmd(CMD_GET_GUI_VARIABLE, RTYPE_ERR, "Get GUI Variable: unsupported variable specified", outputStorage);
        return false;
    }
    // begin response building
    Storage tempMsg;
    //  response-code, variableID, objectID
    tempMsg.writeUnsignedByte(RESPONSE_GET_GUI_VARIABLE);
    tempMsg.writeUnsignedByte(variable);
    tempMsg.writeString(id);
    // process request
    if (variable==ID_LIST) {
        std::vector<std::string> ids = getMainWindow()->getViewIDs();
        tempMsg.writeUnsignedByte(TYPE_STRINGLIST);
        tempMsg.writeStringList(ids);
    } else {
        GUISUMOAbstractView *v = getNamedView(id);
        if (v==0) {
            server.writeStatusCmd(CMD_GET_GUI_VARIABLE, RTYPE_ERR, "View '" + id + "' is not known", outputStorage);
            return false;
        }
        switch (variable) {
        case VAR_VIEW_ZOOM:
            tempMsg.writeUnsignedByte(TYPE_FLOAT);
            tempMsg.writeFloat(v->getChanger().getZoom());
            break;
        case VAR_VIEW_OFFSET:
            tempMsg.writeUnsignedByte(POSITION_2D);
            tempMsg.writeFloat(v->getChanger().getXPos());
            tempMsg.writeFloat(v->getChanger().getYPos());
            break;
        case VAR_VIEW_SCHEMA: {
            FXComboBox &c = v->getColoringSchemesCombo();
            tempMsg.writeUnsignedByte(TYPE_STRING);
            tempMsg.writeString((string)c.getItem(c.getCurrentItem()).text());
            break;
        }
        case VAR_VIEW_BOUNDARY: {
            tempMsg.writeUnsignedByte(TYPE_POLYGON);
            tempMsg.writeUnsignedByte(2);
            Boundary b = v->getVisibleBoundary();
            tempMsg.writeFloat(b.xmin());
            tempMsg.writeFloat(b.ymin());
            tempMsg.writeFloat(b.xmax());
            tempMsg.writeFloat(b.ymax());
            break;
        }
        case VAR_VIEW_BACKGROUNDCOLOR:
            tempMsg.writeUnsignedByte(TYPE_COLOR);
            tempMsg.writeUnsignedByte(static_cast<int>(v->getBackgroundColor().red()*255.+.5));
            tempMsg.writeUnsignedByte(static_cast<int>(v->getBackgroundColor().green()*255.+.5));
            tempMsg.writeUnsignedByte(static_cast<int>(v->getBackgroundColor().blue()*255.+.5));
            tempMsg.writeUnsignedByte(255);
            break;
        case VAR_NET_SIZE: {
            GUINet *net = static_cast<GUINet*>(MSNet::getInstance());
            tempMsg.writeUnsignedByte(POSITION_2D);
            tempMsg.writeFloat(net->getBoundary().getWidth());
            tempMsg.writeFloat(net->getBoundary().getHeight());
        }
        break;
        default:
            break;
        }
    }
    server.writeStatusCmd(CMD_GET_GUI_VARIABLE, RTYPE_OK, warning, outputStorage);
    // send response
    outputStorage.writeUnsignedByte(0); // command length -> extended
    outputStorage.writeInt(static_cast<int>(1 + 4 + tempMsg.size()));
    outputStorage.writeStorage(tempMsg);
    return true;
}


bool
TraCIServerAPI_GUI::processSet(TraCIServer &server, tcpip::Storage &inputStorage,
                               tcpip::Storage &outputStorage) throw(TraCIException, std::invalid_argument) {
    std::string warning = ""; // additional description for response
    // variable
    int variable = inputStorage.readUnsignedByte();
    if (variable!=VAR_VIEW_ZOOM&&variable!=VAR_VIEW_OFFSET&&variable!=VAR_VIEW_SCHEMA&&variable!=VAR_VIEW_BOUNDARY
            &&variable!=VAR_VIEW_BACKGROUNDCOLOR&&variable!=VAR_SCREENSHOT&&variable!=VAR_TRACK_VEHICLE
       ) {
        server.writeStatusCmd(CMD_SET_GUI_VARIABLE, RTYPE_ERR, "Change GUI State: unsupported variable specified", outputStorage);
        return false;
    }
    // id
    std::string id = inputStorage.readString();
    GUISUMOAbstractView *v = getNamedView(id);
    if (v==0) {
        server.writeStatusCmd(CMD_SET_GUI_VARIABLE, RTYPE_ERR, "View '" + id + "' is not known", outputStorage);
        return false;
    }
    // process
    int valueDataType = inputStorage.readUnsignedByte();
    switch (variable) {
    case VAR_VIEW_ZOOM:
        if (valueDataType!=TYPE_FLOAT) {
            server.writeStatusCmd(CMD_SET_GUI_VARIABLE, RTYPE_ERR, "The zoom must be given as a float.", outputStorage);
            return false;
        }
        v->setViewport(inputStorage.readFloat(), v->getChanger().getXPos(), v->getChanger().getYPos());
        break;
    case VAR_VIEW_OFFSET: {
        if (valueDataType!=POSITION_2D) {
            server.writeStatusCmd(CMD_SET_GUI_VARIABLE, RTYPE_ERR, "The view port must be given as a position.", outputStorage);
            return false;
        }
        v->setViewport(v->getChanger().getZoom(), inputStorage.readFloat(), v->getChanger().getYPos());
        v->setViewport(v->getChanger().getZoom(), v->getChanger().getXPos(), inputStorage.readFloat());
    }
    break;
    case VAR_VIEW_SCHEMA:
        if (valueDataType!=TYPE_STRING) {
            server.writeStatusCmd(CMD_SET_GUI_VARIABLE, RTYPE_ERR, "The scheme must be specified by a string.", outputStorage);
            return false;
        }
        if (!v->setColorScheme(inputStorage.readString())) {
            server.writeStatusCmd(CMD_SET_GUI_VARIABLE, RTYPE_ERR, "The scheme is not known.", outputStorage);
            return false;
        }
        break;
    case VAR_VIEW_BOUNDARY: {
        if (valueDataType!=TYPE_POLYGON) {
            server.writeStatusCmd(CMD_SET_GUI_VARIABLE, RTYPE_ERR, "The boundary must be specified by a polygon.", outputStorage);
            return false;
        }
        if (inputStorage.readInt()!=2) {
            server.writeStatusCmd(CMD_SET_GUI_VARIABLE, RTYPE_ERR, "The boundary must be specified by a polygon of size 2.", outputStorage);
            return false;
        }
        const SUMOReal xmin = inputStorage.readFloat();
        const SUMOReal ymin = inputStorage.readFloat();
        const SUMOReal xmax = inputStorage.readFloat();
        const SUMOReal ymax = inputStorage.readFloat();
        Boundary b(xmin, ymin, xmax, ymax);
        v->centerTo(b);
        break;
    }
    case VAR_VIEW_BACKGROUNDCOLOR:
        break;
    case VAR_SCREENSHOT: {
        if (valueDataType!=TYPE_STRING) {
            server.writeStatusCmd(CMD_SET_GUI_VARIABLE, RTYPE_ERR, "Making a snapshot requires a file name.", outputStorage);
            return false;
        }
        std::string filename = inputStorage.readString();
        std::string error = v->makeSnapshot(filename);
        if (error!="") {
            server.writeStatusCmd(CMD_SET_GUI_VARIABLE, RTYPE_ERR, error, outputStorage);
            return false;
        }
    }
    break;
    case VAR_TRACK_VEHICLE: {
        if (valueDataType!=TYPE_STRING) {
            server.writeStatusCmd(CMD_SET_GUI_VARIABLE, RTYPE_ERR, "Tracking requires a string vehicle ID.", outputStorage);
            return false;
        }
        std::string id = inputStorage.readString();
        if (id=="") {
            v->stopTrack();
        } else {
            SUMOVehicle *veh = MSNet::getInstance()->getVehicleControl().getVehicle(id);
            if (veh==0) {
                server.writeStatusCmd(CMD_SET_GUI_VARIABLE, RTYPE_ERR, "Could not find vehicle '" + id + "'.", outputStorage);
                return false;
            }
            if (!static_cast<GUIVehicle*>(veh)->hasActiveAddVisualisation(v, GUIVehicle::VO_TRACKED)) {
                v->startTrack(static_cast<GUIVehicle*>(veh)->getGlID());
                static_cast<GUIVehicle*>(veh)->addActiveAddVisualisation(v, GUIVehicle::VO_TRACKED);
            }
        }
    }
    default:
        break;
    }
    server.writeStatusCmd(CMD_SET_GUI_VARIABLE, RTYPE_OK, warning, outputStorage);
    return true;
}


GUIMainWindow *
TraCIServerAPI_GUI::getMainWindow() throw() {
    FXWindow *w = FXApp::instance()->getRootWindow()->getFirst();
    while (w!=0&&dynamic_cast<GUIMainWindow*>(w)==0) {
        w = w->getNext();
    }
    if (w==0) {
        // main window not found
        return 0;
    }
    return dynamic_cast<GUIMainWindow*>(w);
}


GUISUMOAbstractView * const
TraCIServerAPI_GUI::getNamedView(const std::string &id) throw() {
    GUIMainWindow *mw = static_cast<GUIMainWindow*>(getMainWindow());
    if (mw==0) {
        return 0;
    }
    GUIGlChildWindow *c = static_cast<GUIGlChildWindow*>(mw->getViewByID(id));
    if (c==0) {
        return 0;
    }
    return c->getView();
}


#endif


/****************************************************************************/

