/***************************************************************************
                          NBOptionsIO.cpp
			  A class for the initialisation, input and veryfying of the
        programs options
                             -------------------
    project              : SUMO
    subproject           : netbuilder / netconverter
    begin                : Tue, 20 Nov 2001
    copyright            : (C) 2001 by DLR http://ivf.dlr.de/
    author               : Daniel Krajzewicz
    email                : Daniel.Krajzewicz@dlr.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
namespace
{
    const char rcsid[] =
    "$Id$";
}
// $Log$
// Revision 1.1  2002/10/16 15:48:13  dkrajzew
// initial commit for net building classes
//
// Revision 1.10  2002/07/25 08:40:46  dkrajzew
// Visum7.5 and Cell import added
//
// Revision 1.9  2002/07/18 07:22:26  dkrajzew
// Default usage of types removed
//
// Revision 1.8  2002/07/11 05:47:35  dkrajzew
// Options describing files transfered to Option_FileName to enable relative path setting within the configuration files
//
// Revision 1.7  2002/06/17 15:19:29  dkrajzew
// unreferenced variable declarations removed
//
// Revision 1.6  2002/06/11 16:00:42  dkrajzew
// windows eol removed; template class definition inclusion depends now on the EXTERNAL_TEMPLATE_DEFINITION-definition
//
// Revision 1.5  2002/05/14 04:42:56  dkrajzew
// new computation flow
//
// Revision 1.4  2002/04/26 10:07:12  dkrajzew
// Windows eol removed; minor double to int conversions removed;
//
// Revision 1.3  2002/04/24 06:52:01  dkrajzew
// Deprecated initialisation of the options container that used enviroment variables removed
//
// Revision 1.2  2002/04/16 12:30:13  dkrajzew
// Usage of SUMO_DATA removed
//
// Revision 1.1.1.1  2002/04/09 14:18:27  dkrajzew
// new version-free project name (try2)
//
// Revision 1.1.1.1  2002/04/09 13:22:00  dkrajzew
// new version-free project name
//
// Revision 1.6  2002/04/09 12:21:25  dkrajzew
// Windows-Memoryleak detection changed
//
// Revision 1.5  2002/03/22 10:50:03  dkrajzew
// Memory leaks debugging added (MSVC++)
//
// Revision 1.4  2002/03/20 08:30:33  dkrajzew
// Help output added
//
// Revision 1.3  2002/03/06 10:13:25  traffic
// Enviroment variable changef from SUMO to SUMO_DATA
//
// Revision 1.2  2002/03/05 14:55:33  traffic
// Error report on unset path changed
//
// Revision 1.1.1.1  2002/02/19 15:33:04  traffic
// Initial import as a separate application.
//
//
/* =========================================================================
 * included modules
 * ======================================================================= */
#include <string>
#include <iostream>
#include <fstream>
#include <utils/options/Option.h>
#include <utils/options/OptionsCont.h>
#include <utils/options/OptionsIO.h>
#include "NBOptionsIO.h"
#include <utils/common/FileHelpers.h>
#include <utils/common/UtilExceptions.h>

/* =========================================================================
 * debugging definitions (MSVC++ only)
 * ======================================================================= */
#ifdef _DEBUG
   #define _CRTDBG_MAP_ALLOC // include Microsoft memory leak detection procedures
   #define _INC_MALLOC	     // exclude standard memory alloc procedures
#endif

/* =========================================================================
 * used namespaces
 * ======================================================================= */
using namespace std;

/* =========================================================================
 * method definitions
 * ======================================================================= */
/**
 * getOptions
 * Builds the container of options and parses the configuration file
 * and the command line arguments using it. After this, the settings are
 * validated using "checkSettings".
 * Returns 0 when something failed, otherwise the build OptionsCont.
 */
OptionsCont *
NBOptionsIO::getOptions(int argc, char **argv)
{
    // initialise options; return (exception) when the enviroment-variable is not set
    OptionsCont *oc = init();
    if(oc==0)
        throw ProcessError();
    // parse options from configuration and command line
    bool ok = OptionsIO::getOptions(oc, argc, argv);
    // do nothing more when only help shall be printed
    if(oc->getBool("help"))
        return oc;
    // print options when wished
    if(ok && oc->getBool("p"))
        cout << *oc;
    // check options
    if(ok)
        ok = check(oc);
    // throw an exception when something went wrong
    if(!ok) {
        delete oc;
        throw ProcessError();
    }
    // prepare for further processing
    precomputeValues(oc);
    return oc;
}


OptionsCont *
NBOptionsIO::init()
{
    OptionsCont *oc;
    oc = new OptionsCont();
    // register the file i/o options
    oc->doRegister("sumo-net", 's', new Option_FileName());
    oc->doRegister("sumo-logics", 'l', new Option_FileName());
    oc->doRegister("xml-node-files", 'n', new Option_FileName());
    oc->doRegister("xml-edge-files", 'e', new Option_FileName());
    oc->doRegister("cell-node-file", new Option_FileName());
    oc->doRegister("cell-edge-file", new Option_FileName());
    oc->doRegister("visum-file", new Option_FileName());
    oc->doRegister("type-file", 't', new Option_FileName(/*"types.xml"*/));
    oc->doRegister("output-file", 'o', new Option_FileName("default.net.xml"));
    oc->doRegister("configuration-file", 'c', new Option_FileName("sumo-netconvert.cfg"));
    oc->addSynonyme("xml-node-files", "xml-nodes");
    oc->addSynonyme("xml-edge-files", "xml-edges");
    oc->addSynonyme("cell-node-file", "cell-nodes");
    oc->addSynonyme("cell-edge-file", "cell-edges");
    oc->addSynonyme("visum-file", "visum");
    oc->addSynonyme("type-file", "types");
    oc->addSynonyme("output-file", "output");
    oc->addSynonyme("configuration-file", "configuration");
    // register building defaults
    oc->doRegister("type", 'T', new Option_String("Unknown"));
    oc->doRegister("lanenumber", 'L', new Option_Integer(1));
    oc->doRegister("speed", 'S', new Option_Float((float) 13.9));
    oc->doRegister("priority", 'P', new Option_Integer(1));
    oc->doRegister("capacity-norm", 'N', new Option_Float((float) 20000));
    // register the report options
    oc->doRegister("verbose", 'v', new Option_Bool(false));
    oc->doRegister("warn", 'w', new Option_Bool(false));
    oc->doRegister("print-options", 'p', new Option_Bool(false));
    oc->doRegister("help", new Option_Bool(false));
    // register the data processing options
    oc->doRegister("no-config", 'C', new Option_Bool(false));
    oc->addSynonyme("no-config", "no-configuration");
    oc->doRegister("recompute-junction-logics", new Option_Bool(false));
    return oc;
}

/**
 * checkSettings
 * Checks the build settings. The following constraints must be valid:
 * - a junction folder must be given
 *   (otherwise no junctions can be written)
 * - a node-list must be supplied
 *   (otherwise no information about the position of nodes is available and
 *   the logics can not be build)
 * - at least one list containing edges or sections must be supplied
 *   (otherwise the network is unknown)
 * Returns true when all constraints are valid
 */
bool
NBOptionsIO::check(OptionsCont *oc) {
   bool ok = true;
   try {
      if(!checkCompleteDescription(oc)) {
         if(!checkNodes(oc)) ok = false;
         if(!checkEdges(oc)) ok = false;
         if(!checkOutput(oc)) ok = false;
      }
    } catch (InvalidArgument &e) {
      cout << e.msg() << endl;
      return false;
    }
    return ok;
}

bool
NBOptionsIO::checkCompleteDescription(OptionsCont *oc)
{
   return false;
}

bool
NBOptionsIO::checkNodes(OptionsCont *oc)
{
    // check the existance of a name for the nodes file
    if( oc->isSet("n") ||
        oc->isSet("cell-nodes") ||
        oc->isSet("visum") ||
        oc->isSet("sumo-net") ) {
        return true;
    }
    cout << "Error: The nodes must be supplied." << endl;
    return false;
}

bool
NBOptionsIO::checkEdges(OptionsCont *oc)
{
    // check whether at least a sections or a edges file is supplied
    if( oc->isSet("e") ||
        oc->isSet("cell-edges") ||
        oc->isSet("visum") ||
        oc->isSet("sumo-net") ) {
        return true;
    }
    cout << "Error: Either sections or edges must be supplied." << endl;
    return false;
}

bool
NBOptionsIO::checkOutput(OptionsCont *oc)
{
    ofstream strm(oc->getString("o").c_str());
    if(!strm.good()) {
        cout << "Error: The output file \"" << oc->getString("o")
            << "\" can not be build." << endl;
        return false;
    }
    return true;
}

void
NBOptionsIO::precomputeValues(OptionsCont *oc) {
//   oc->doRegister("used-file-format", new Option_String("xml"));
}

/**************** DO NOT DEFINE ANYTHING AFTER THE INCLUDE *****************/
//#ifdef DISABLE_INLINE
//#include "NBOptionsIO.icc"
//#endif

// Local Variables:
// mode:C++
// End:

