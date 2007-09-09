/****************************************************************************/
/// @file    RODFDetector.cpp
/// @author  Daniel Krajzewicz
/// @date    Thu, 16.03.2006
/// @version $Id$
///
// Class representing a detector within the DFROUTER
/****************************************************************************/
// SUMO, Simulation of Urban MObility; see http://sumo.sourceforge.net/
// copyright : (C) 2001-2007
//  by DLR (http://www.dlr.de/) and ZAIK (http://www.zaik.uni-koeln.de/AFS)
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

#include "RODFDetector.h"
#include <utils/common/MsgHandler.h>
#include <utils/common/UtilExceptions.h>
#include <router/ROEdge.h>
#include "RODFEdge.h"
#include "DFRORouteDesc.h"
#include "DFRORouteCont.h"
#include "RODFDetectorFlow.h"
#include <utils/helpers/RandomDistributor.h>
#include <utils/common/StdDefs.h>
#include <utils/geom/GeomHelper.h>
#include "RODFNet.h"
#include <utils/iodevices/OutputDevice.h>

#ifdef CHECK_MEMORY_LEAKS
#include <foreign/nvwa/debug_new.h>
#endif // CHECK_MEMORY_LEAKS


// ===========================================================================
// used namespaces
// ===========================================================================
using namespace std;


// ===========================================================================
// method definitions
// ===========================================================================
RODFDetector::RODFDetector(const std::string &Id, const std::string &laneId,
                       SUMOReal pos, const RORODFDetectorType type)
        : myID(Id), myLaneID(laneId), myPosition(pos), myType(type), myRoutes(0)
{}


RODFDetector::RODFDetector(const std::string &Id, const RODFDetector &f)
        : myID(Id), myLaneID(f.myLaneID), myPosition(f.myPosition),
        myType(f.myType), myRoutes(0)
{
    if (f.myRoutes!=0) {
        myRoutes = new DFRORouteCont(*(f.myRoutes));
    }
}


RODFDetector::~RODFDetector()
{
    delete myRoutes;
}


void
RODFDetector::setType(RORODFDetectorType type)
{
    myType = type;
}


SUMOReal
RODFDetector::computeDistanceFactor(const DFRORouteDesc &rd) const
{
    SUMOReal distance = GeomHelper::distance(
                            static_cast<RODFEdge*>(rd.edges2Pass[0])->getFromPosition(),
                            static_cast<RODFEdge*>(rd.edges2Pass[rd.edges2Pass.size()-1])->getToPosition());
    SUMOReal length = 0;
    for (std::vector<ROEdge*>::const_iterator i=rd.edges2Pass.begin(); i!=rd.edges2Pass.end(); ++i) {
        length += (*i)->getLength();
    }
    return (distance/length);
}


SUMOReal 
RODFDetector::getUsage(const RODFDetectorCon &detectors,DFRORouteDesc*route, DFRORouteCont::RoutesMap *curr,
                     SUMOTime time, const RODFDetectorFlows &flows) const
{
    if(curr->splitMap.size()==0) {
        // ok, reached end
        return 1;
    }
    // patch probability
    SUMOReal allProb = 0;
    SUMOReal cProb = 0;
    DFRORouteCont::RoutesMap *next = 0;
    for(std::map<ROEdge*, DFRORouteCont::RoutesMap*>::const_iterator i=curr->splitMap.begin(); i!=curr->splitMap.end(); ++i) {
        SUMOReal tprob = (SUMOReal) detectors.getAggFlowFor((*i).second->lastDetectorEdge, time, 30*60, flows);
        if(find(route->edges2Pass.begin(), route->edges2Pass.end(), (*i).first)!=route->edges2Pass.end()) {
            cProb += tprob;
            next = (*i).second;
        }
        allProb += tprob;
    }
    if(cProb==0) {
        return 0;
    }
    return cProb * getUsage(detectors, route, next, time, flows);
/*
    std::vector<ROEdge*>::iterator j1, j2;
    j1 = route->edges2Pass.begin();
    j2 = curr->common.begin();
    while(curr->splitMap.size()!=0) {
        while(j2!=curr->common.end()) {//*j1==*j2) {
            ++j1;
            ++j2;
            /
            if(j1==route->edges2Pass.end()||j2==curr->common.end()) {
                cout << "Ups!" << endl;
                return prob;
            }
            /
        }
        DFRORouteCont::RoutesMap *next = curr->splitMap.find(*j1)->second;
        // patch probability
        SUMOReal allProb = 0;
        SUMOReal cProb = 0;
        for(std::map<ROEdge*, DFRORouteCont::RoutesMap*>::const_iterator i=curr->splitMap.begin(); i!=curr->splitMap.end(); ++i) {
                SUMOReal tprob = (SUMOReal) detectors.getAggFlowFor((*i).first, time, 30*60, flows);
                if((*i).first==*next->common.begin()) {
                    cProb += tprob;
                }
                allProb += tprob;
        }
        if(cProb==0) {
            return 0;
        }
        if(next->splitMap.size()==0) {
            // ok, last entry found
            return prob;
        }
        curr = next;
        j2 = curr->common.begin();
        prob *= (cProb / allProb);
    }
    // ok, not found!?
    cout << "Not found!" << endl;
    return 0;
    */
}


void
RODFDetector::buildDestinationDistribution(const RODFDetectorCon &detectors,
        const RODFDetectorFlows &flows,
        SUMOTime startTime,
        SUMOTime endTime,
        SUMOTime stepOffset,
        const RODFNet &net,
        std::map<size_t, RandomDistributor<size_t>* > &into,
        int maxFollower) const
{
    if (myRoutes==0) {
        if (myType!=DISCARDED_DETECTOR&&myType!=BETWEEN_DETECTOR) {
            MsgHandler::getErrorInstance()->inform("Missing routes for detector '" + myID + "'.");
        }
        return;
    }
    DFRORouteCont::RoutesMap *routeMap = myRoutes->getRouteMap(net);

    std::vector<DFRORouteDesc*>::const_iterator ri;
    const std::vector<DFRORouteDesc*> &descs = myRoutes->get();
    const std::vector<FlowDef> &mflows = flows.getFlowDefs(myID);
    const std::map<ROEdge*, std::vector<ROEdge*> > &dets2Follow = myRoutes->getDets2Follow();
    // iterate through time (in output interval steps)
    int time;
    for (time=startTime; time<endTime; time+=stepOffset) {
        into[time] = new RandomDistributor<size_t>();
        // iterate through the routes
        size_t index = 0;
        for (ri=descs.begin(); ri!=descs.end(); ++ri, index++) {
            SUMOReal prob = getUsage(detectors, *ri, routeMap, time, flows);
            into[time]->add(prob, index);
            (*ri)->overallProb = prob;
        }
    }

    delete routeMap;
/*
    std::vector<DFRORouteDesc*>::const_iterator ri;
    const std::vector<DFRORouteDesc*> &descs = myRoutes->get();
    const std::vector<FlowDef> &mflows = flows.getFlowDefs(myID);
    const std::map<ROEdge*, std::vector<ROEdge*> > &dets2Follow = myRoutes->getDets2Follow();
    // iterate through time (in output interval steps)
    int time;
    for (time=startTime; time<endTime; time+=stepOffset) {
        into[time] = new RandomDistributor<size_t>();
        // iterate through the routes
        size_t index = 0;
        for (ri=descs.begin(); ri!=descs.end(); ++ri, index++) {
            DFRORouteDesc *rd = *ri;
            SUMOReal lastProb = 1;
            ROEdge *lastDetEdge = *rd->edges2Pass.begin();
            for (vector<ROEdge*>::const_iterator q=rd->edges2Pass.begin(); lastProb!=0&&q!=rd->edges2Pass.end(); ++q) {
                if ((*q)->getNoFollowing()>1&&q!=rd->edges2Pass.end()-1) {
                    // ok, we have a split; check probability for each of the successors
                    map<ROEdge*, SUMOReal> probs;
                    SUMOReal allProbs = 0;
                    for (size_t i=0; i<(*q)->getNoFollowing(); ++i) {
                        ROEdge *ne = (*q)->getFollower(i);
                        SUMOReal prob = getUsage(ne, rd->edges2Pass.end(), q, detectors, flows, time, net, maxFollower, 0);
                        probs[ne] = prob;
                        allProbs += prob;
                    }
                    if (allProbs!=0) {
                        lastProb *= (probs[*(q+1)] / allProbs);
                    } else {
                        lastProb = 0;
                    }
                }
            }
            into[time]->add(lastProb, index);
            (*ri)->overallProb = lastProb;
        }
    }
*/
    /*
    std::map<const ROEdge *, int> lastDetNo;
    for(ri=descs.begin(); ri!=descs.end(); ++ri) {
        DFRORouteDesc *rd = *ri;
        rd->lastDetectorEdge
    }
    /
    const std::vector<FlowDef> &mflows = flows.getFlowDefs(myID);
    const std::map<ROEdge*, std::vector<ROEdge*> > &dets2Follow = myRoutes->getDets2Follow();
    // iterate through time (in output interval steps)
    int time;
    for (time=startTime; time<endTime; time+=stepOffset) {
        into[time] = new RandomDistributor<size_t>();
        // get the number of vehicles to emit
        const FlowDef &srcFD = mflows[(int)(time/stepOffset) - startTime]; // !!! check stepOffset
        SUMOReal toEmit = (SUMOReal)(srcFD.qLKW + srcFD.qPKW);
        // iterate through the routes
        size_t index = 0;
        for (ri=descs.begin(); ri!=descs.end()&&toEmit>=0; ++ri, index++) {
            DFRORouteDesc *rd = *ri;

            SUMOReal distanceFactor = computeDistanceFactor(*rd);

            // the current probability is 1
            SUMOReal ovProb = 1.;
            bool hadMissing = false;
            // go through this route's edges
            std::vector<ROEdge*>::reverse_iterator i = rd->edges2Pass.rbegin() + 1;
            while(i!=rd->edges2Pass.rend()) {
                if((*i)->getNoFollowing()>1||hadMissing) {
                    if(dets2Follow.find(*i)!=dets2Follow.end()) {

                        const std::vector<ROEdge*> &possNext = dets2Follow.find(*i)->second;
                        ROEdge *my = 0;
                        SUMOReal sumFlows = 0;
                        SUMOReal myFlow = 0;
                        for(std::vector<ROEdge*>::const_iterator k=possNext.begin(); k!=possNext.end(); ++k) {
                            SUMOReal cflow = (SUMOReal) detectors.getAggFlowFor(*k, time, 30*60, flows);
                            if(cflow<0) {
                                cflow = 0; // !!!
                            }
                            if(my==0&&find(rd->edges2Pass.begin(), rd->edges2Pass.end(), *k)!=rd->edges2Pass.end()) {
                                my = *k;
                                myFlow = cflow;
                            }
                            sumFlows += cflow;
                        }

                        SUMOReal cProb = 1.;
                        if(sumFlows!=0) {
                            cProb = (myFlow / sumFlows);
                        } else {
                            // myFlow should be of course==0 so let's keep the previous probability
                            cProb = 1.;
                        }
                        ovProb *= cProb;
                        hadMissing = false;
                    } else {
                        hadMissing = true;
                    }
                }
                ++i;
            }
            /*
            //
            std::vector<ROEdge*>::iterator i = rd->edges2Pass.begin();
            SUMOReal noRamps = 0;
            SUMOReal noLanes = 1;
            SUMOReal length = 0;
            SUMOReal edgeFactor = 1;
            while (i!=rd->edges2Pass.end()) {
                if ((*i)->getID().find("Added")!=string::npos) {
                    noRamps += 1;
                }
                if ((*i)->getID()=="537844516") {
                    edgeFactor = MIN2((SUMOReal) .25, edgeFactor);
                } else if ((*i)->getID()=="537844517") {
                    edgeFactor = MIN2((SUMOReal) .10, edgeFactor);
                } else if ((*i)->getID()=="51054917") {
                    edgeFactor = MIN2((SUMOReal) .25, edgeFactor);
                } else if ((*i)->getID()=="-572279218") {
                    edgeFactor = MIN2((SUMOReal) .25, edgeFactor);
                } else if ((*i)->getID()=="-51049672") {
                    edgeFactor = MIN2((SUMOReal) .25, edgeFactor);
                } else if ((*i)->getID()=="-537849185-AddedOffRampEdge") {
                    edgeFactor = MIN2((SUMOReal) .25, edgeFactor);
                } else if ((*i)->getID()=="-51069807-AddedOffRampEdge") {
                    edgeFactor = MIN2((SUMOReal) .25, edgeFactor);
                } else if ((*i)->getID()=="-51140230") {
                    edgeFactor = MIN2((SUMOReal) .25, edgeFactor);
                } else if ((*i)->getID()=="-51555688") {
                    edgeFactor = MIN2((SUMOReal) .25, edgeFactor);
                } else if ((*i)->getID()=="50872837") {
                    edgeFactor = MIN2((SUMOReal) .25, edgeFactor);
                } else if ((*i)->getID()=="-51110597") {
                    edgeFactor = MIN2((SUMOReal) .25, edgeFactor);
                } else if ((*i)->getID()=="537835308") {
                    edgeFactor = MIN2((SUMOReal) .25, edgeFactor);
                }





                if ((*i)->getLaneNo()==1) {
                    noLanes *= (SUMOReal) .4;
                }
                length += (*i)->getLength();
                ++i;
            }
            //            SUMOReal noEdges = (SUMOReal) rd->edges2Pass.size();

            //
            distanceFactor = distanceFactor * distanceFactor;
            rd->factor = rd->factor * rd->factor;
            // SUMOReal laneFactor = (noLanes/noEdges);
            SUMOReal ovProb = (rd->factor * distanceFactor * length * edgeFactor * noLanes);
            /
            into[time]->add(ovProb, index);
            (*ri)->overallProb = ovProb;
        }
    }
    /*
    const std::map< int, FlowDef > &mflows = flows.getFlowDefs(myID);
    const std::map<ROEdge*, std::vector<ROEdge*> > &dets2Follow = myRoutes->getDets2Follow();
    for(int time=startTime; time<endTime; time+=stepOffset) {
        if(mflows.find(time)==mflows.end()) {
            continue;
        }
        into[time] = new RandomDistributor<size_t>();
        FlowDef srcFD = mflows.find(time)->second;
        SUMOReal toEmit = (SUMOReal) (srcFD.qLKW + srcFD.qPKW);
        const std::vector<DFRORouteDesc*> &descs = myRoutes->get();
        std::vector<DFRORouteDesc*>::const_iterator ri;
        size_t index = 0;
        for(ri=descs.begin(); ri!=descs.end()&&toEmit>=0; ++ri, index++) {
            SUMOReal destFlow = toEmit;
            for(std::vector<ROEdge*>::iterator j=(*ri)->edges2Pass.begin(); j!=(*ri)->edges2Pass.end()&&destFlow>0; ++j) {
                if(dets2Follow.find(*j)!=dets2Follow.end()) {
                    // here we have a detector which flows divides on consecutives
                    const std::vector<ROEdge*> &possNext = dets2Follow.find(*j)->second;
                    ROEdge *my = 0;
                    SUMOReal sumFlows = 0;
                    SUMOReal myFlow = 0;
                    for(std::vector<ROEdge*>::const_iterator k=possNext.begin(); k!=possNext.end(); ++k) {
                        SUMOReal cflow = detectors.getFlowFor(*k, (time/!!! + desc->duration2Last/), flows);
    if(myID=="12") {
        cout << myID << " " << cflow << " " << (*k)->getID() << endl;
    }
                        if(cflow<0) {
                            cflow = destFlow; // !!!
                        }
                        if(my==0&&find((*ri)->edges2Pass.begin(), (*ri)->edges2Pass.end(), *k)!=(*ri)->edges2Pass.end()) {
                            my = *k;
                            myFlow = cflow;
                        }
                        sumFlows += cflow;
    if(myID=="12") {
        if(my!=0) {
            cout << myID << " " << cflow << " " << my->getID() << endl;
        } else {
            cout << myID << " " << cflow << endl;
        }
        cout << myID << " " << destFlow << " " << sumFlows << " " << cflow << endl;
    }

                    }
                    if(my!=0&&sumFlows>0) {
                        destFlow = (myFlow / sumFlows) * destFlow;
                    } else {
                        destFlow = 0;
                    }
                }
            }
    //            cout << myID << " " << destFlow << endl;
            into[time]->add(destFlow, index);
    if(myID=="12") {
        cout << "dest " << myID << " " << destFlow << endl;
    }
            (*ri)->overallProb += destFlow;
        }
    }
    /
        std::vector<size_t> unset;
        size_t index = 0;
        for(ri=descs.begin(); ri!=descs.end()&&toEmit>=0; ++ri, index++) {
            DFRORouteDesc *desc = (*ri);
            const ROEdge *lastEdge = desc->lastDetectorEdge;
            if(lastEdge!=0) {
                int flow = detectors.getFlowFor(lastEdge, (SUMOTime) (time + desc->duration2Last), flows);
                if(flow>=0) {
                    into[time]->add(flow, index);
                    toEmit -= flow;
                    continue;
                }
            }
            unset.push_back(index);
        }
        if(toEmit>=0) {
            std::vector<size_t>::const_iterator di;
            for(di=unset.begin(); di!=unset.end()&&toEmit>=0; ++di) {
                into[time]->add((SUMOReal) toEmit / (SUMOReal) unset.size(), *di);
            }
        }
    }
    /
    myRoutes->sortByDistance();
    const std::map< int, FlowDef > &mflows = flows.getFlowDefs(myID);
    for(int time=startTime; time<endTime; time+=stepOffset) {
        if(mflows.find(time)==mflows.end()) {
            continue;
        }
        into[time] = new RandomDistributor<size_t>();
        FlowDef srcFD = mflows.find(time)->second;
        size_t toEmit = srcFD.qLKW + srcFD.qPKW;
        const std::vector<DFRORouteDesc*> &descs = myRoutes->get();
        std::vector<DFRORouteDesc*>::const_iterator ri;
        std::vector<size_t> unset;
        size_t index = 0;
        for(ri=descs.begin(); ri!=descs.end()&&toEmit>=0; ++ri, index++) {
            DFRORouteDesc *desc = (*ri);
            const ROEdge *lastEdge = desc->lastDetectorEdge;
            if(lastEdge!=0) {
                int flow = detectors.getFlowFor(lastEdge, (SUMOTime) (time + desc->duration2Last), flows);
                if(flow>=0) {
                    into[time]->add(flow, index);
                    toEmit -= flow;
                    continue;
                }
            }
            unset.push_back(index);
        }
        if(toEmit>=0) {
            std::vector<size_t>::const_iterator di;
            for(di=unset.begin(); di!=unset.end()&&toEmit>=0; ++di) {
                into[time]->add((SUMOReal) toEmit / (SUMOReal) unset.size(), *di);
            }
        }
    }

    /
    RandomDistributor<size_t> *ret = new RandomDistributor<size_t>;
    ret->add(1, 0); // !!!
    */
//	return ret;
}


const std::vector<DFRORouteDesc*> &
RODFDetector::getRouteVector() const
{
    return myRoutes->get();
}


void
RODFDetector::addPriorDetector(RODFDetector *det)
{
    myPriorDetectors.push_back(det);
}


void
RODFDetector::addFollowingDetector(RODFDetector *det)
{
    myFollowingDetectors.push_back(det);
}


const std::vector<RODFDetector*> &
RODFDetector::getPriorDetectors() const
{
    return myPriorDetectors;
}


const std::vector<RODFDetector*> &
RODFDetector::getFollowerDetectors() const
{
    return myFollowingDetectors;
}



bool
RODFDetector::writeEmitterDefinition(const std::string &file,
                                   const RODFDetectorCon &detectors,
                                   const RODFDetectorFlows &flows,
                                   SUMOTime startTime, SUMOTime endTime,
                                   SUMOTime stepOffset,
                                   const RODFNet &net,
                                   bool includeUnusedRoutes,
                                   SUMOReal scale,
                                   int maxFollower,
                                   bool emissionsOnly) const
{
    // write the definition
    OutputDevice& out = OutputDevice::getDevice(file);
//    cout << "a1" << endl;
    // begin
    if (getType()==SOURCE_DETECTOR) {
        out.writeXMLHeader("triggeredsource");
    } else {
        out.writeXMLHeader("calibrator");
    }

//    cout << "a2" << endl;
    std::map<size_t, RandomDistributor<size_t>* > dists;
    if (!emissionsOnly&&flows.knows(myID)) {
        buildDestinationDistribution(detectors, flows, startTime, endTime, stepOffset, net, dists, maxFollower); // !!!
    }
    //cout << "a3" << endl;

    // routes
    if(!emissionsOnly) {
        if (myRoutes!=0&&myRoutes->get().size()!=0) {
            const std::vector<DFRORouteDesc*> &routes = myRoutes->get();
            std::vector<DFRORouteDesc*>::const_iterator i;
            // compute the overall routes sum
            SUMOReal overallSum = 0;
            for (i=routes.begin(); i!=routes.end(); ++i) {
                overallSum += (*i)->overallProb;
            }

            int writtenRoutes = 0;
            if (overallSum!=0) {
                // !!! check things about intervals
                // !!! optional
                for (i=routes.begin(); i!=routes.end(); ++i) {
                    if ((*i)->overallProb>0||includeUnusedRoutes) {
                        out << "   <routedistelem id=\"" << (*i)->routename << "\" probability=\"" << ((*i)->overallProb/overallSum) << "\"/>\n"; // !!!
                        writtenRoutes++;
                    }
                }
            }
            // hmmmm - all routes seem to have a probability of zero...
            //  let's save them all (should maybe be done optional)
            if (writtenRoutes==0) {
                for (i=routes.begin(); i!=routes.end(); ++i) {
                    out << "   <routedistelem id=\"" << (*i)->routename << "\" probability=\"1\"/>\n"; // !!!
                }
            }
        }
        else {
            MsgHandler::getErrorInstance()->inform("Detector '" + getID() + "' has no routes!?");
            return false;
        }
    }
    // vehicle types
    {
    }
//    cout << "a4" << endl;
    // emissions
    if (emissionsOnly||flows.knows(myID)) {
        // get the flows for this detector

        const std::vector<FlowDef> &mflows = flows.getFlowDefs(myID);
        // go through the simulation seconds
        for (int time=startTime; time<endTime; time+=stepOffset) {
            // get own (departure flow)
            if ((int) mflows.size()<=(int)((time/stepOffset) - startTime)) {
                cout << mflows.size() << ":" << (int)(time/stepOffset) - startTime << endl;
                throw 1;
            }
            const FlowDef &srcFD = mflows[(int)(time/stepOffset) - startTime];  // !!! check stepOffset
            // get flows at end
            RandomDistributor<size_t> *destDist = dists.size()>time ? dists[time] : 0;
            // go through the cars
            size_t carNo = (size_t)((srcFD.qPKW + srcFD.qLKW) * scale);
//            cout << "b1 " << carNo <<  endl;
            for (size_t car=0; car<carNo; ++car) {
                // get the vehicle parameter
                string type = "test";
                SUMOReal v = -1;
                int destIndex = destDist!=0 && destDist->getOverallProb()>0 ? destDist->get() : -1;
//!!! micro srcIndex = srcDist.get();
//				std::vector<std::string> route = droutes[destIndex]->edges2Pass;
                if (srcFD.isLKW>1) {
                    srcFD.isLKW = srcFD.isLKW - (SUMOReal) 1.;
//!!!		        	type = lkwTypes[vehSpeedDist.get()];
                    v = srcFD.vLKW;
                } else {
//!!!	    			type = pkwTypes[vehSpeedDist.get()];
                    v = srcFD.vPKW;
                }
                if (v<=0) {
                    v = (SUMOReal)(100 / 3.6);
                } else if (v>=180) {
                    v = (SUMOReal)(100 / 3.6);
                }

                // compute the departure time
                int ctime = (int)(time + ((SUMOReal) stepOffset * (SUMOReal) car / (SUMOReal) carNo));

                // write
                out << "   <emit id=\"";
                if (getType()==SOURCE_DETECTOR) {
                    out << "emitter_" << myID;
                } else {
                    out << "calibrator_" << myID;
                }
                out << "_" << ctime  << "\"" // !!! running
                << " time=\"" << ctime << "\""
                << " speed=\"" << v << "\"";
                if (!emissionsOnly&&destIndex>=0) {
                    out << " route=\"" << myRoutes->get()[destIndex]->routename << "\""; // !!! optional
                }
                out  << "/>\n";
                srcFD.isLKW += srcFD.fLKW;
            }
            delete destDist;
        }
    }
//    cout << "a5" << endl;
    out.close();
    return true;
}


void
RODFDetector::addRoutes(DFRORouteCont *routes)
{
    delete myRoutes;
    myRoutes = routes;
}


void
RODFDetector::addRoute(const RODFNet &net, DFRORouteDesc *nrd)
{
    if (myRoutes==0) {
        myRoutes = new DFRORouteCont(net);
    }
    myRoutes->addRouteDesc(nrd);
}

bool
RODFDetector::hasRoutes() const
{
    return myRoutes!=0&&myRoutes->get().size()!=0;
}


bool
RODFDetector::writeRoutes(std::vector<std::string> &saved,
                          OutputDevice &out)
{
    if (myRoutes!=0) {
        return myRoutes->save(saved, "", out);
    }
    return false;
}


void
RODFDetector::writeSingleSpeedTrigger(const std::string &file,
                                    const RODFDetectorFlows &flows,
                                    SUMOTime startTime, SUMOTime endTime,
                                    SUMOTime stepOffset)
{
    OutputDevice& out = OutputDevice::getDevice(file);
    out.writeXMLHeader("vss");
    const std::vector<FlowDef> &mflows = flows.getFlowDefs(myID);
    for (SUMOTime t=startTime; t<endTime; t+=stepOffset) {
        const FlowDef &srcFD = mflows[(int)(t/stepOffset) - startTime]; // !!! check stepOffset
        SUMOReal speed = MAX2(srcFD.vLKW, srcFD.vPKW);
        if (speed==0) {
            speed = 200; // !!! no limit
        }
        speed = (SUMOReal)(speed / 3.6);
        out << "   <step time=\"" << t << "\" speed=\"" << speed << "\"/>\n";
    }
    out.close();
}










RODFDetectorCon::RODFDetectorCon()
{}


RODFDetectorCon::~RODFDetectorCon()
{}


bool
RODFDetectorCon::addDetector(RODFDetector *dfd)
{
    if (myDetectorMap.find(dfd->getID())!=myDetectorMap.end()) {
        return false;
    }
    myDetectorMap[dfd->getID()] = dfd;
    myDetectors.push_back(dfd);
    string edgeid = dfd->getLaneID().substr(0, dfd->getLaneID().rfind('_'));
    if (myDetectorEdgeMap.find(edgeid)==myDetectorEdgeMap.end()) {
        myDetectorEdgeMap[edgeid] = std::vector<RODFDetector*>();
    }
    myDetectorEdgeMap[edgeid].push_back(dfd);
    return true; // !!!
}


bool
RODFDetectorCon::detectorsHaveCompleteTypes() const
{
    for (std::vector<RODFDetector*>::const_iterator i=myDetectors.begin(); i!=myDetectors.end(); ++i) {
        if ((*i)->getType()==TYPE_NOT_DEFINED) {
            return false;
        }
    }
    return true;
}


bool
RODFDetectorCon::detectorsHaveRoutes() const
{
    for (std::vector<RODFDetector*>::const_iterator i=myDetectors.begin(); i!=myDetectors.end(); ++i) {
        if ((*i)->hasRoutes()) {
            return true;
        }
    }
    return false;
}


const std::vector< RODFDetector*> &
RODFDetectorCon::getDetectors() const
{
    return myDetectors;
}


void
RODFDetectorCon::save(const std::string &file) const
{
    OutputDevice& out = OutputDevice::getDevice(file);
    out.writeXMLHeader("detectors");
    for (std::vector<RODFDetector*>::const_iterator i=myDetectors.begin(); i!=myDetectors.end(); ++i) {
        out << "   <detector_definition id=\"" << (*i)->getID()
        << "\" lane=\"" << (*i)->getLaneID()
        << "\" pos=\"" << (*i)->getPos();
        switch ((*i)->getType()) {
        case BETWEEN_DETECTOR:
            out << "\" type=\"between\"";
            break;
        case SOURCE_DETECTOR:
            out << "\" type=\"source\"";
            break;
        case SINK_DETECTOR:
            out << "\" type=\"sink\"";
            break;
        case DISCARDED_DETECTOR:
            out << "\" type=\"discarded\"";
            break;
        default:
            throw 1;
        }
        out << "/>\n";
    }
    out.close();
}


void
RODFDetectorCon::saveAsPOIs(const std::string &file) const
{
    OutputDevice& out = OutputDevice::getDevice(file);
    out.writeXMLHeader("pois");
    for (std::vector<RODFDetector*>::const_iterator i=myDetectors.begin(); i!=myDetectors.end(); ++i) {
        out << "   <poi id=\"" << (*i)->getID();
        switch ((*i)->getType()) {
        case BETWEEN_DETECTOR:
            out << "\" type=\"between_detector_position\" color=\"0,0,1\"";
            break;
        case SOURCE_DETECTOR:
            out << "\" type=\"source_detector_position\" color=\"0,1,0\"";
            break;
        case SINK_DETECTOR:
            out << "\" type=\"sink_detector_position\" color=\"1,0,0\"";
            break;
        case DISCARDED_DETECTOR:
            out << "\" type=\"discarded_detector_position\" color=\".2,.2,.2\"";
            break;
        default:
            throw 1;
        }
        out << " lane=\"" << (*i)->getLaneID()<< "\" pos=\""
        << (*i)->getPos() << "\"/>\n";
    }
    out.close();
}


void
RODFDetectorCon::saveRoutes(const std::string &file) const
{
    OutputDevice& out = OutputDevice::getDevice(file);
    out.writeXMLHeader("routes");
    std::vector<std::string> saved;
    // write for source detectors
    bool lastWasSaved = true;
    for (std::vector<RODFDetector*>::const_iterator i=myDetectors.begin(); i!=myDetectors.end(); ++i) {
        if (lastWasSaved) {
            out << "\n";
        }
        lastWasSaved = (*i)->writeRoutes(saved, out);
    }
    out << "\n";
    out.close();
}


const RODFDetector &
RODFDetectorCon::getDetector(const std::string &id) const
{
    return *(myDetectorMap.find(id)->second);
}


/*
bool
RODFDetectorCon::isDetector( std::string id )
{
	bool ret = false;
	for(std::vector<RODFDetector*>::const_iterator i=myDetectors.begin(); i!=myDetectors.end(); ++i) {
		if ( (*i)->getID() == id )
			ret = true;
	}
	return ret;
}
*/


void
RODFDetectorCon::writeEmitters(const std::string &file,
                             const RODFDetectorFlows &flows,
                             SUMOTime startTime, SUMOTime endTime,
                             SUMOTime stepOffset, const RODFNet &net,
                             bool writeCalibrators,
                             bool includeUnusedRoutes,
                             SUMOReal scale,
                             int maxFollower,
                             bool emissionsOnly)
{
    OutputDevice& out = OutputDevice::getDevice(file);
    out.writeXMLHeader("additional");
    for (std::vector<RODFDetector*>::const_iterator i=myDetectors.begin(); i!=myDetectors.end(); ++i) {
        RODFDetector *det = *i;
        string defFileName;

        // write the declaration into the file
        if (det->getType()==SOURCE_DETECTOR) {
            defFileName = "emitter_" + det->getID() + ".def.xml";
        } else if (writeCalibrators&&det->getType()==BETWEEN_DETECTOR) {
            defFileName = "calibrator_" + det->getID() + ".def.xml";
        } else {
            defFileName = "other_" + det->getID() + ".def.xml";
            continue;
        }
//        cout << det->getID() << endl;
        // try to write the definition
        if (!det->writeEmitterDefinition(defFileName, *this, flows, startTime, endTime, stepOffset, net, includeUnusedRoutes, scale, maxFollower, emissionsOnly)) {
            // skip if something failed... (!!!)
            continue;
        }
        // write the declaration into the file
        if (det->getType()==SOURCE_DETECTOR) {
            out << "   <trigger id=\"source_" << det->getID()
            << "\" objecttype=\"emitter\" "
            << "pos=\"" << det->getPos() << "\" "
            << "objectid=\"" << det->getLaneID() << "\" "
            << "friendly_pos=\"x\" " // !!!
            << "file=\"" << defFileName << "\"/>\n";
        } else if (writeCalibrators&&det->getType()==BETWEEN_DETECTOR) {
            out << "   <trigger id=\"calibrator_" << det->getID()
            << "\" objecttype=\"calibrator\" "
            << "pos=\"" << det->getPos() << "\" "
            << "objectid=\"" << det->getLaneID() << "\" "
            << "friendly_pos=\"x\" " // !!!
            << "file=\"" << defFileName << "\"/>\n";
        }
    }
    out.close();
}

/*
void
RODFDetectorCon::writeVehicles(const std::string &file,
                             const RODFDetectorFlows &flows,
                             SUMOTime startTime, SUMOTime endTime,
                             SUMOTime stepOffset, bool writeCalibrators)
{
    ofstream strm(file.c_str());
    if (!strm.good()) {
        throw ProcessError("Could not open file '" + file + "'.");

    }
    strm << "<routes>" << endl;
    for(SUMOTime t=startTime; t<endTime; t++) {
        for (std::vector<RODFDetector*>::const_iterator i=myDetectors.begin(); i!=myDetectors.end(); ++i) {
            RODFDetector *det = *i;
            det->writeVehicles(defFileName, *this, flows, t);
            strm << endl;
        }
    }
    strm << "</routes>" << endl;
}
*/

void
RODFDetectorCon::writeEmitterPOIs(const std::string &file,
                                const RODFDetectorFlows &flows,
                                SUMOTime startTime, SUMOTime endTime,
                                SUMOTime stepOffset)
{
    OutputDevice& out = OutputDevice::getDevice(file);
    out.writeXMLHeader("additional");
    for (std::vector<RODFDetector*>::const_iterator i=myDetectors.begin(); i!=myDetectors.end(); ++i) {
        RODFDetector *det = *i;
        SUMOReal flow = flows.getFlowSumSecure(det->getID());
        SUMOReal col = flow / flows.getMaxDetectorFlow();
        col = (SUMOReal)(col / 2. + .5);
        SUMOReal r, g, b;
        r = g = b = 0;
        out << "   <poi id=\"" << (*i)->getID() << ":" << flow;
        switch ((*i)->getType()) {
        case BETWEEN_DETECTOR:
            out << "\" type=\"between_detector_position\" color=\"0,0," << col << "\"";
            break;
        case SOURCE_DETECTOR:
            out << "\" type=\"source_detector_position\" color=\"0," << col << ",0\"";
            break;
        case SINK_DETECTOR:
            out << "\" type=\"sink_detector_position\" color=\"" << col << ",0,0\"";
            break;
        case DISCARDED_DETECTOR:
            out << "\" type=\"discarded_detector_position\" color=\".2,.2,.2\"";
            break;
        default:
            throw 1;
        }
        out << " lane=\"" << (*i)->getLaneID()<< "\" pos=\"" << (*i)->getPos() << "\"/>\n";
    }
    out.close();
}


int
RODFDetectorCon::getFlowFor(const ROEdge *edge, SUMOTime time,
                          const RODFDetectorFlows &) const
{
    SUMOReal stepOffset = 60; // !!!
    SUMOReal startTime = 0; // !!!
    assert(myDetectorEdgeMap.find(edge->getID())!=myDetectorEdgeMap.end());
    const std::vector<FlowDef> &flows = static_cast<const RODFEdge*>(edge)->getFlows();
    if (flows.size()!=0) {
        const FlowDef &srcFD = flows[(int)((time/stepOffset) - startTime)];
        return (int)(MAX2(srcFD.qLKW, (SUMOReal) 0.) + MAX2(srcFD.qPKW, (SUMOReal) 0.));
    }
    /*
    const std::vector<RODFDetector*> &detsOnEdge = myDetectorEdgeMap.find(edge->getID())->second;
    std::vector<RODFDetector*>::const_iterator i;
    SUMOReal ret = 0;
    int counted = 0;
    for(i=detsOnEdge.begin(); i!=detsOnEdge.end(); ++i) {
        if(flows.knows((*i)->getID())) {
            const std::vector<FlowDef> &mflows = flows.getFlowDefs((*i)->getID());
            const FlowDef &srcFD = mflows[(int) (time/stepOffset) - startTime]; // !!! check stepOffset
            counted++; // !!! make a difference between pkws and lkws
            ret += (srcFD.qLKW + srcFD.qPKW);
        }
    }
    if(counted!=0) {
        return (int) (ret / (SUMOReal) counted);
    }
    */
    return -1;
}


int
RODFDetectorCon::getAggFlowFor(const ROEdge *edge, SUMOTime time, SUMOTime period,
                             const RODFDetectorFlows &) const
{
    if(edge==0) {
        return 0;
    }
    SUMOReal stepOffset = 60; // !!!
    SUMOReal startTime = 0; // !!!
//    cout << edge->getID() << endl;
    assert(myDetectorEdgeMap.find(edge->getID())!=myDetectorEdgeMap.end());
    const std::vector<FlowDef> &flows = static_cast<const RODFEdge*>(edge)->getFlows();
    SUMOReal agg = 0;
    for (std::vector<FlowDef>::const_iterator i=flows.begin(); i!=flows.end(); ++i) {
        const FlowDef &srcFD = *i;
        if (srcFD.qLKW>=0) {
            agg += srcFD.qLKW;
        }
        if (srcFD.qPKW>=0) {
            agg += srcFD.qPKW;
        }
    }
    return (int) agg;
    /* !!! make this time variable
    if (flows.size()!=0) {
        SUMOReal agg = 0;
        size_t beginIndex = (int)((time/stepOffset) - startTime);  // !!! falsch!!!
        for (SUMOTime t=0; t<period&&beginIndex<flows.size(); t+=(SUMOTime) stepOffset) {
            const FlowDef &srcFD = flows[beginIndex++];
            if (srcFD.qLKW>=0) {
                agg += srcFD.qLKW;
            }
            if (srcFD.qPKW>=0) {
                agg += srcFD.qPKW;
            }
        }
        return (int) agg;
    }
    */
    return -1;
}


void
RODFDetectorCon::writeSpeedTrigger(const std::string &file,
                                 const RODFDetectorFlows &flows,
                                 SUMOTime startTime, SUMOTime endTime,
                                 SUMOTime stepOffset)
{
    OutputDevice& out = OutputDevice::getDevice(file);
    out.writeXMLHeader("additional");
    for (std::vector<RODFDetector*>::const_iterator i=myDetectors.begin(); i!=myDetectors.end(); ++i) {
        RODFDetector *det = *i;
        // write the declaration into the file
        if (det->getType()==SINK_DETECTOR&&flows.knows(det->getID())) {
            string filename = "vss_" + det->getID() + ".def.xml";
            out << "   <trigger id=\"vss_" << det->getID() << '\"'
            << " objecttype=\"lane\""
            << " objectid=\"" << det->getLaneID() << '\"'
            << " attr=\"speed\" "
            << " file=\"" << filename << "\"/>\n";
            det->writeSingleSpeedTrigger(filename, flows, startTime, endTime, stepOffset);
        }
    }
    out.close();
}


void
RODFDetectorCon::writeEndRerouterDetectors(const std::string &file)
{
    OutputDevice& out = OutputDevice::getDevice(file);
    out.writeXMLHeader("additional");
    for (std::vector<RODFDetector*>::const_iterator i=myDetectors.begin(); i!=myDetectors.end(); ++i) {
        RODFDetector *det = *i;
        // write the declaration into the file
        if (det->getType()==SINK_DETECTOR) {
            out << "   <trigger id=\"endrerouter_" << det->getID()
            << "\" objecttype=\"rerouter\" objectid=\"" <<
            det->getLaneID() << "\" attr=\"reroute\" pos=\"0\" file=\"endrerouter_"
            << det->getID() << ".def.xml\"/>\n";
        }
    }
    out.close();
}


void
RODFDetectorCon::writeValidationDetectors(const std::string &file,
                                        bool includeSources,
                                        bool singleFile, bool friendly)
{
    OutputDevice& out = OutputDevice::getDevice(file);
    out.writeXMLHeader("additional");
    for (std::vector<RODFDetector*>::const_iterator i=myDetectors.begin(); i!=myDetectors.end(); ++i) {
        RODFDetector *det = *i;
        // write the declaration into the file
        if (det->getType()!=SOURCE_DETECTOR||includeSources) {
            SUMOReal pos = det->getPos();
            if (det->getType()==SOURCE_DETECTOR) {
                pos += 1;
            }
            out << "   <detector id=\"validation_" << det->getID() << "\" "
            << "lane=\"" << det->getLaneID() << "\" "
            << "pos=\"" << pos << "\" "
            << "freq=\"60\" style=\"CSV\" ";
            if (friendly) {
                out << "friendly_pos=\"x\" ";
            }
            if (!singleFile) {
                out << "file=\"validation_det_" << det->getID() << ".xml\"/>\n";
            } else {
                out << "file=\"validation_dets.xml\"/>\n";//!!!
            }
        }
    }
    out.close();
}


void
RODFDetectorCon::removeDetector(const std::string &id)
{
    //
    std::map<std::string, RODFDetector*>::iterator ri1 = myDetectorMap.find(id);
    RODFDetector *oldDet = (*ri1).second;
    myDetectorMap.erase(ri1);
    //
    std::vector<RODFDetector*>::iterator ri2 =
        find(myDetectors.begin(), myDetectors.end(), oldDet);
    myDetectors.erase(ri2);
    //
    bool found = false;
    for (std::map<std::string, std::vector<RODFDetector*> >::iterator rr3=myDetectorEdgeMap.begin(); !found&&rr3!=myDetectorEdgeMap.end(); ++rr3) {
        std::vector<RODFDetector*> &dets = (*rr3).second;
        for (std::vector<RODFDetector*>::iterator ri3=dets.begin(); !found&&ri3!=dets.end();) {
            if (*ri3==oldDet) {
                found = true;
                ri3 = dets.erase(ri3);
            } else {
                ++ri3;
            }
        }
    }
    delete oldDet;
}


void
RODFDetectorCon::guessEmptyFlows(RODFDetectorFlows &flows)
{
    // routes must be built (we have ensured this in main)
    // detector followers/prior must be build (we have ensured this in main)
    //
    bool changed = true;
    while (changed) {
        for (std::vector<RODFDetector*>::const_iterator i=myDetectors.begin(); i!=myDetectors.end(); ++i) {
            RODFDetector *det = *i;
            const vector<RODFDetector*> &prior = det->getPriorDetectors();
            const vector<RODFDetector*> &follower = det->getFollowerDetectors();
            size_t noFollowerWithRoutes = 0;
            size_t noPriorWithRoutes = 0;
            // count occurences of detectors with/without routes
            vector<RODFDetector*>::const_iterator j;
            for (j=prior.begin(); j!=prior.end(); ++j) {
                if (flows.knows((*j)->getID())) {
                    ++noPriorWithRoutes;
                }
            }
            assert(noPriorWithRoutes<=prior.size());
            for (j=follower.begin(); j!=follower.end(); ++j) {
                if (flows.knows((*j)->getID())) {
                    ++noFollowerWithRoutes;
                }
            }
            assert(noFollowerWithRoutes<=follower.size());

            // do not process detectors which have no routes
            if (!flows.knows(det->getID())) {
                continue;
            }

            // plain case: some of the following detectors have no routes
            if (noFollowerWithRoutes==follower.size()) {
                // the number of vehicles is the sum of all vehicles on prior
                continue;
            }

        }
    }
}


void
RODFDetectorCon::mesoJoin(const std::string &nid,
                        const std::vector<std::string> &oldids)
{
    // build the new detector
    const RODFDetector &first = getDetector(*(oldids.begin()));
    RODFDetector *newDet = new RODFDetector(nid, first);
    addDetector(newDet);
    // delete previous
    for (std::vector<std::string>::const_iterator i=oldids.begin(); i!=oldids.end(); ++i) {
        removeDetector(*i);
    }
}


/****************************************************************************/
