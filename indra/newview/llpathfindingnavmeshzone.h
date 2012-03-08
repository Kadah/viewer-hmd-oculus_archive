/** 
 * @file llpathfindingnavmeshzone.h
 * @author William Todd Stinson
 * @brief A class for representing the zone of navmeshes containing and possible surrounding the current region.
 *
 * $LicenseInfo:firstyear=2002&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc. 
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 * 
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#ifndef LL_LLPATHFINDINGNAVMESHZONE_H
#define LL_LLPATHFINDINGNAVMESHZONE_H

#include "llsd.h"
#include "lluuid.h"
#include "llpathfindingnavmesh.h"

#include <vector>

#include <boost/shared_ptr.hpp>
#include <boost/function.hpp>
#include <boost/signals2.hpp>

//#define XXX_STINSON_DEBUG_NAVMESH_ZONE

class LLPathfindingNavMeshZone
{
public:
	typedef enum {
		kNavMeshZoneRequestUnknown,
		kNavMeshZoneRequestNeedsUpdate,
		kNavMeshZoneRequestStarted,
		kNavMeshZoneRequestCompleted,
		kNavMeshZoneRequestNotEnabled,
		kNavMeshZoneRequestError
	} ENavMeshZoneRequestStatus;

	typedef boost::function<void (ENavMeshZoneRequestStatus)>         navmesh_zone_callback_t;
	typedef boost::signals2::signal<void (ENavMeshZoneRequestStatus)> navmesh_zone_signal_t;
	typedef boost::signals2::connection                               navmesh_zone_slot_t;

	LLPathfindingNavMeshZone();
	virtual ~LLPathfindingNavMeshZone();

	navmesh_zone_slot_t registerNavMeshZoneListener(navmesh_zone_callback_t pNavMeshZoneCallback);
	void initialize();

	void enable();
	void disable();
	void refresh();

protected:

private:
	typedef boost::function<void (void)> navmesh_location_callback_t;
	class NavMeshLocation
	{
	public:
		NavMeshLocation(S32 pDirection, navmesh_location_callback_t pLocationCallback);
		virtual ~NavMeshLocation();

		void enable();
		void refresh();
		void disable();

		LLPathfindingNavMesh::ENavMeshRequestStatus getRequestStatus() const;
#ifdef XXX_STINSON_DEBUG_NAVMESH_ZONE
		const LLUUID &getRegionUUID() const {return mRegionUUID;};
		S32          getDirection() const {return mDirection;};
#endif // XXX_STINSON_DEBUG_NAVMESH_ZONE

	protected:

	private:
		void           handleNavMesh(LLPathfindingNavMesh::ENavMeshRequestStatus pNavMeshRequestStatus, const LLUUID &pRegionUUID, U32 pNavMeshVersion, const LLSD::Binary &pNavMeshData);

		void           clear();
		LLViewerRegion *getRegion() const;

		S32                                         mDirection;
		LLUUID                                      mRegionUUID;
		bool                                        mHasNavMesh;
		U32                                         mNavMeshVersion;
		navmesh_location_callback_t                 mLocationCallback;
		LLPathfindingNavMesh::ENavMeshRequestStatus mRequestStatus;
		LLPathfindingNavMesh::navmesh_slot_t        mNavMeshSlot;
	};

	typedef boost::shared_ptr<NavMeshLocation> NavMeshLocationPtr;
	typedef std::vector<NavMeshLocationPtr> NavMeshLocationPtrs;

	void handleNavMeshLocation();
	void updateStatus();

	NavMeshLocationPtrs   mNavMeshLocationPtrs;
	navmesh_zone_signal_t mNavMeshZoneSignal;
};

#endif // LL_LLPATHFINDINGNAVMESHZONE_H
