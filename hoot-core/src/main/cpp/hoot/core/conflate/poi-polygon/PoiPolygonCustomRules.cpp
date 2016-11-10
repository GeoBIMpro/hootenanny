/*
 * This file is part of Hootenanny.
 *
 * Hootenanny is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * --------------------------------------------------------------------
 *
 * The following copyright notices are generated automatically. If you
 * have a new notice to add, please use the format:
 * " * @copyright Copyright ..."
 * This will properly maintain the copyright information. DigitalGlobe
 * copyrights will be updated automatically.
 *
 * @copyright Copyright (C) 2015 DigitalGlobe (http://www.digitalglobe.com/)
 */
#include "PoiPolygonCustomRules.h"

// geos
//#include <geos/geom/LineString.h>
#include <geos/util/TopologyException.h>
#include <geos/geom/Geometry.h>

// hoot
#include <hoot/core/util/ElementConverter.h>
#include <hoot/core/schema/OsmSchema.h>
#include <hoot/core/util/ElementConverter.h>

#include "extractors/PoiPolygonTypeScoreExtractor.h"
#include "extractors/PoiPolygonNameScoreExtractor.h"
#include "extractors/PoiPolygonAddressScoreExtractor.h"


namespace hoot
{

PoiPolygonCustomRules::PoiPolygonCustomRules(const ConstOsmMapPtr& map,
                                             const set<ElementId>& polyNeighborIds,
                                             const set<ElementId>& poiNeighborIds,
                                             double distance) :
_map(map),
_polyNeighborIds(polyNeighborIds),
_poiNeighborIds(poiNeighborIds),
_distance(distance),
_badGeomCount(0),
_isRecCenterMatch(false),
_poiNeighborWithAddressContainedInPoly(false)
{
}

void PoiPolygonCustomRules::collectInfo(ConstElementPtr poi, ConstElementPtr poly)
{
  ElementConverter elementConverter(_map);
  shared_ptr<Geometry> polyGeom = elementConverter.convertToGeometry(poly);
  if (QString::fromStdString(polyGeom->toString()).toUpper().contains("EMPTY"))
  {
    throw geos::util::TopologyException();
  }
  shared_ptr<Geometry> poiGeom = elementConverter.convertToGeometry(poi);

  const bool poiHasType = PoiPolygonTypeScoreExtractor::hasType(poi);
  const bool poiIsRecCenter = PoiPolygonTypeScoreExtractor::isRecCenter(poi);
  const QString poiAddress =
    poi->getTags().get(PoiPolygonAddressScoreExtractor::FULL_ADDRESS_TAG_NAME).toLower().trimmed();

  const bool polyHasName = PoiPolygonNameScoreExtractor::elementHasName(poly);
  const bool polyIsPark = PoiPolygonTypeScoreExtractor::isPark(poly);
  const bool polyHasType = PoiPolygonTypeScoreExtractor::hasType(poly);
  const bool polyIsBuildingIsh = PoiPolygonTypeScoreExtractor::isBuildingIsh(poly);
  const bool polyHasMoreThanOneType = PoiPolygonTypeScoreExtractor::hasMoreThanOneType(poly);
  bool polyHasSpecificType = polyHasType;
  if ((poly->getTags().get("building") == "yes" || poly->getTags().get("poi") == "yes") &&
      !polyHasMoreThanOneType)
  {
    polyHasSpecificType = false;
  }

  LOG_VART(poiHasType);
  LOG_VART(poiIsRecCenter);
  LOG_VART(poiAddress);

  LOG_VART(polyHasName);
  LOG_VART(polyIsPark);
  LOG_VART(polyHasType);
  LOG_VART(polyIsBuildingIsh);
  LOG_VART(polyHasMoreThanOneType);
  LOG_VART(polyHasSpecificType);

  bool poiContainedInAnotherParkPoly = false;
  bool polyContainedInAnotherParkPoly = false;

  //specific condition set here to minimize run time.  obviously, as more rules are added, this
  //condition could get unwieldy
  if (!poiHasType && poiIsRecCenter && polyIsBuildingIsh && (!polyHasSpecificType || !polyHasName))
  {
    set<ElementId>::const_iterator polyNeighborItr = _polyNeighborIds.begin();
    while (polyNeighborItr != _polyNeighborIds.end())
    {
      ConstElementPtr polyNeighbor = _map->getElement(*polyNeighborItr);
      if (polyNeighbor->getElementId() != poly->getElementId())
      {
        shared_ptr<Geometry> polyNeighborGeom;
        try
        {
          polyNeighborGeom = ElementConverter(_map).convertToGeometry(polyNeighbor);

          if (polyNeighborGeom.get() &&
              QString::fromStdString(polyNeighborGeom->toString()).toUpper().contains("EMPTY"))
          {
            if (_badGeomCount <= ConfigOptions().getOgrLogLimit())
            {
              LOG_WARN(
                "Invalid area neighbor polygon: " << polyNeighborGeom->toString());
              _badGeomCount++;
            }
          }
          else if (polyNeighborGeom.get())
          {
            if (PoiPolygonTypeScoreExtractor::isPark(polyNeighbor))
            {
              if (polyNeighborGeom->contains(poiGeom.get()))
              {
                poiContainedInAnotherParkPoly = true;

                LOG_TRACE(
                      "poi examined and found to be contained within a park poly outside of this "
                      "match comparison: " << poi->toString());
                LOG_TRACE("park poly it is very close to: " << polyNeighbor->toString());

              }

              if (polyNeighborGeom->contains(polyGeom.get()))
              {
                //may need to be specific that the poi and the poly are in the same
                //park...
                polyContainedInAnotherParkPoly = true;
              }
            }
          }
        }
        catch (const geos::util::TopologyException& e)
        {
          if (_badGeomCount <= ConfigOptions().getOgrLogLimit())
          {
            LOG_WARN(
              "Feature passed to PoiPolygonMatchCreator caused topology exception on conversion to a " <<
              "geometry: " << polyNeighbor->toString() << "\n" << e.what());
            _badGeomCount++;
          }
        }
      }
      polyNeighborItr++;
    }
  }

  const bool poiContainedInParkPoly =
    poiContainedInAnotherParkPoly || (polyIsPark && _distance == 0);

  LOG_VART(poiContainedInAnotherParkPoly);
  LOG_VART(poiContainedInParkPoly);

  //If a POI is in a park, and is rec center, force a review against any other building-like
  //poly in the park.
  if (poiContainedInParkPoly && !poiHasType && poiIsRecCenter && polyIsBuildingIsh &&
      (!polyHasSpecificType || !polyHasName) && polyContainedInAnotherParkPoly)
  {
    LOG_TRACE("Found evidence per rule #1...");
    _isRecCenterMatch = true;
  }

  if (poiAddress.isEmpty())
  {
    return;
  }

  set<ElementId>::const_iterator poiNeighborItr = _poiNeighborIds.begin();
  while (poiNeighborItr != _poiNeighborIds.end())
  {
    ConstElementPtr poiNeighbor = _map->getElement(*poiNeighborItr);
    if (poiNeighbor->getElementId() != poi->getElementId())
    {
      try
      {
        const QString poiNeighborName = PoiPolygonNameScoreExtractor::getElementName(poiNeighbor);
        LOG_VART(poiNeighborName);

        if (!poiNeighborName.isEmpty() && poiNeighborName == poiAddress)
        {
          shared_ptr<Geometry> poiNeighborGeom =
            ElementConverter(_map).convertToGeometry(poiNeighbor);
          if (polyGeom->contains(poiNeighborGeom.get()))
          {
            _poiNeighborWithAddressContainedInPoly = true;
            LOG_VART(_poiNeighborWithAddressContainedInPoly);
          }
        }
      }
      catch (const geos::util::TopologyException& e)
      {
        if (_badGeomCount <= ConfigOptions().getOgrLogLimit())
        {
          LOG_WARN(
            "Feature passed to PoiPolygonMatchCreator caused topology exception on "
            "conversion to a geometry: " << poiNeighbor->toString() << "\n" << e.what());
          _badGeomCount++;
        }
      }
    }
    poiNeighborItr++;
  }
}

}
