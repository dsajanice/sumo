/****************************************************************************/
/// @file    MSCFModel_Kerner.h
/// @author  Daniel Krajzewicz
/// @date    03.04.2010
/// @version $Id$
///
// car-following model by B. Kerner
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
#ifndef MSCFModel_Kerner_h
#define	MSCFModel_Kerner_h

// ===========================================================================
// included modules
// ===========================================================================
#ifdef _MSC_VER
#include <windows_config.h>
#else
#include <config.h>
#endif

#include <microsim/MSCFModel.h>
#include <utils/xml/SUMOXMLDefinitions.h>


// ===========================================================================
// class definitions
// ===========================================================================
/** @class MSCFModel_Kerner
 * @brief car-following model by B. Kerner
 * @see MSCFModel
 */
class MSCFModel_Kerner : public MSCFModel {
public:
    /** @brief Constructor
     * @param[in] accel The maximum acceleration
     * @param[in] decel The maximum deceleration
     * @param[in] dawdle The driver imperfection
     * @param[in] tau The driver's reaction time
     */
    MSCFModel_Kerner(const MSVehicleType* vtype, SUMOReal accel, SUMOReal decel, SUMOReal tau, SUMOReal k, SUMOReal phi) throw();


    /// @brief Destructor
    ~MSCFModel_Kerner() throw();


    /// @name Implementations of the MSCFModel interface
    /// @{

    /** @brief Applies interaction with stops and lane changing model influences
     * @param[in] veh The ego vehicle
     * @param[in] lane The lane ego is at
     * @param[in] vPos The possible velocity
     * @return The velocity after applying interactions with stops and lane change model influences
     */
    SUMOReal moveHelper(MSVehicle * const veh, const MSLane * const lane, SUMOReal vPos) const throw();


    /** @brief Computes the vehicle's safe speed (no dawdling)
     * @param[in] veh The vehicle (EGO)
     * @param[in] speed The vehicle's speed
     * @param[in] gap2pred The (netto) distance to the LEADER
     * @param[in] predSpeed The speed of LEADER
     * @return EGO's safe speed
     * @see MSCFModel::ffeV
     */
    SUMOReal ffeV(const MSVehicle * const veh, SUMOReal speed, SUMOReal gap2pred, SUMOReal predSpeed) const throw();


    /** @brief Computes the vehicle's safe speed (no dawdling)
     * @param[in] veh The vehicle (EGO)
     * @param[in] gap2pred The (netto) distance to the LEADER
     * @param[in] predSpeed The speed of LEADER
     * @return EGO's safe speed
     * @see MSCFModel::ffeV
     * @todo used by MSLCM_DK2004, allows hypothetic values of gap2pred and predSpeed
     */
    SUMOReal ffeV(const MSVehicle * const veh, SUMOReal gap2pred, SUMOReal predSpeed) const throw();


    /** @brief Computes the vehicle's safe speed (no dawdling)
     * @param[in] veh The vehicle (EGO)
     * @param[in] pred The LEADER
     * @return EGO's safe speed
     * @see MSCFModel::ffeV
     * @todo generic Interface, models can call for the values they need
     */
    SUMOReal ffeV(const MSVehicle * const veh, const MSVehicle * const pred) const throw();


    /** @brief Computes the vehicle's safe speed for approaching a non-moving obstacle (no dawdling)
     * @param[in] veh The vehicle (EGO)
     * @param[in] gap2pred The (netto) distance to the the obstacle
     * @return EGO's safe speed for approaching a non-moving obstacle
     * @see MSCFModel::ffeS
     * @todo generic Interface, models can call for the values they need
     */
    SUMOReal ffeS(const MSVehicle * const veh, SUMOReal gap2pred) const throw();


    /** @brief Returns the maximum gap at which an interaction between both vehicles occurs
     *
     * "interaction" means that the LEADER influences EGO's speed.
     * @param[in] veh The EGO vehicle
     * @param[in] vL LEADER's speed
     * @return The interaction gap
     * @see MSCFModel::interactionGap
     * @todo evaluate signature
     */
    SUMOReal interactionGap(const MSVehicle * const veh, SUMOReal vL) const throw();


    /** @brief Get the vehicle's maximum acceleration [m/s^2]
     *
     * As some models describe that a vehicle is accelerating slower the higher its
     *  speed is, the velocity is given.
     *
     * @param[in] v The vehicle's velocity
     * @return The maximum acceleration
     */
    SUMOReal getMaxAccel(SUMOReal v) const throw() {
        UNUSED_PARAMETER(v);
        return myAccel;
    }


    /** @brief Returns the model's name
     * @return The model's name
     * @see MSCFModel::getModelName
     */
    int getModelID() const throw() {
        return SUMO_TAG_CF_BKERNER;
    }


    /** @brief Get the vehicle type's maximum acceleration [m/s^2]
     * @return The maximum acceleration (in m/s^2) of vehicles of this class
     */
    SUMOReal getMaxAccel() const throw() {
        return myAccel;
    }

    /** @brief Get the driver's reaction time [s]
     * @return The reaction time of this class' drivers in s
     */
    SUMOReal getTau() const throw() {
        return myTau;
    }
    /// @}



    /** @brief Duplicates the car-following model
     * @param[in] vtype The vehicle type this model belongs to (1:1)
     * @return A duplicate of this car-following model
     */
    MSCFModel *duplicate(const MSVehicleType *vtype) const throw();


private:
    /** @brief Returns the "safe" velocity
     * @param[in] gap2pred The (netto) distance to the LEADER
     * @param[in] predSpeed The LEADER's speed
     * @return the safe velocity
     */
    SUMOReal _v(SUMOReal speed, SUMOReal vfree, SUMOReal gap, SUMOReal predSpeed) const throw();



private:
    /// @name model parameter
    /// @{

    /// @brief The vehicle's maximum acceleration [m/s^2]
    SUMOReal myAccel;

    /// @brief The driver's reaction time [s]
    SUMOReal myTau;

    /// @brief The driver's reaction time [s]
    SUMOReal myK;

    /// @brief The driver's reaction time [s]
    SUMOReal myPhi;

    /// @brief The precomputed value for myDecel*myTau
    SUMOReal myTauDecel;
    /// @}

};

#endif	/* MSCFModel_Kerner_H */

