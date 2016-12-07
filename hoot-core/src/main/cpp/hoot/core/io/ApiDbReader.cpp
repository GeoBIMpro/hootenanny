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
 * @copyright Copyright (C) 2016 DigitalGlobe (http://www.digitalglobe.com/)
 */
#include "ApiDbReader.h"

// Qt
#include <QSet>

namespace hoot
{

ApiDbReader::ApiDbReader() :
_useDataSourceIds(true),
_status(Status::Invalid),
_open(false)
{

}

void ApiDbReader::_addTagsToElement(shared_ptr<Element> element)
{
  bool ok;
  Tags& tags = element->getTags();

  if (tags.contains("hoot:status"))
  {
    QString statusStr = tags.get("hoot:status");
    bool ok;
    const int statusInt = statusStr.toInt(&ok);
    Status status = static_cast<Status::Type>(statusInt);
    if (ok && status.getEnum() >= Status::Invalid && status.getEnum() <= Status::Conflated)
    {
      element->setStatus(status);
    }
    else
    {
      LOG_WARN("Invalid status: " + statusStr + " for element with ID: " +
               QString::number(element->getId()));
    }
    tags.remove("hoot:status");
  }

  if (tags.contains("type"))
  {
    Relation* r = dynamic_cast<Relation*>(element.get());
    if (r)
    {
      r->setType(tags["type"]);
      tags.remove("type");
    }
  }

  if (tags.contains("error:circular"))
  {
    element->setCircularError(tags.get("error:circular").toDouble(&ok));
    if (!ok)
    {
      try
      {
        double tv = tags.getLength("error:circular").value();
        element->setCircularError(tv);
        ok = true;
        LOG_TRACE(
          "Set circular error from error:circular tag to " << tv << " for element with ID: " <<
          element->getId());
      }
      catch (const HootException& e)
      {
        ok = false;
      }

      if (!ok)
      {
        LOG_WARN("Error parsing error:circular.");
      }
    }
    tags.remove("error:circular");
  }
  else if (tags.contains("accuracy"))
  {
    element->setCircularError(tags.get("accuracy").toDouble(&ok));

    if (!ok)
    {
      try
      {
        double tv = tags.getLength("accuracy").value();
        element->setCircularError(tv);
        ok = true;
        LOG_TRACE(
          "Set circular error from accuracy tag to " << tv << " for element with ID: " <<
          element->getId());
      }
      catch (const HootException& e)
      {
        ok = false;
      }

      if (!ok)
      {
        LOG_WARN("Error parsing accuracy.");
      }
    }
    tags.remove("accuracy");
  }
}

ElementId ApiDbReader::_mapElementId(const OsmMap& map, ElementId oldId)
{
  ElementId result;
  LOG_VART(oldId);
  if (_useDataSourceIds)
  {
    result = oldId;
  }
  else
  {
    long id = oldId.getId();
    switch (oldId.getType().getEnum())
    {
    case ElementType::Node:
      if (_nodeIdMap.count(id) > 0)
      {
        result = ElementId::node(_nodeIdMap.at(id));
      }
      else
      {
        long newId = map.createNextNodeId();
        _nodeIdMap[id] = newId;
        result = ElementId::node(newId);
      }
      break;
    case ElementType::Way:
      if (_wayIdMap.count(id) > 0)
      {
        result = ElementId::way(_wayIdMap.at(id));
      }
      else
      {
        long newId = map.createNextWayId();
        _wayIdMap[id] = newId;
        result = ElementId::way(newId);
      }
      break;
    case ElementType::Relation:
      if (_relationIdMap.count(id) > 0)
      {
        result = ElementId::relation(_relationIdMap.at(id));
      }
      else
      {
        long newId = map.createNextRelationId();
        _relationIdMap[id] = newId;
        result = ElementId::relation(newId);
      }
      break;
    default:
      throw IllegalArgumentException("Expected a valid element type, but got: " +
        QString::number(oldId.getType().getEnum()));
    }
  }
  LOG_VART(result);

  return result;
}

void ApiDbReader::_readByBounds(shared_ptr<OsmMap> map, const Envelope& bounds)
{
  LOG_DEBUG("Retrieving node records within the query bounds...");
  shared_ptr<QSqlQuery> nodeItr = _getDatabase()->selectNodesByBounds(bounds);
  QSet<QString> nodeIds;
  while (nodeItr->next())
  {
    NodePtr element = _resultToNode(*nodeItr, *map);
    LOG_VART(element->toString());
    map->addElement(element);
    nodeIds.insert(QString::number(element->getId()));
  }
  LOG_VARD(nodeIds.size());

  if (nodeIds.size() > 0)
  {
    LOG_DEBUG("Retrieving way IDs referenced by the selected nodes...");
    QSet<QString> wayIds;
    shared_ptr<QSqlQuery> wayIdItr = _getDatabase()->selectWayIdsByWayNodeIds(nodeIds);
    while (wayIdItr->next())
    {
      const QString wayId = QString::number((*wayIdItr).value(0).toLongLong());
      LOG_VART(wayId);
      wayIds.insert(wayId);
    }
    LOG_VARD(wayIds.size());

    if (wayIds.size() > 0)
    {
      LOG_DEBUG("Retrieving ways by way ID...");
      shared_ptr<QSqlQuery> wayItr =
        _getDatabase()->selectElementsByElementIdList(wayIds, TableType::Way);
      while (wayItr->next())
      {
        WayPtr element = _resultToWay(*wayItr, *map);
        LOG_VART(element->toString());
        //I'm a little confused why this wouldn't cause a problem in that you could be writing ways
        //to the map here whose nodes haven't yet been written to the map yet.  Haven't encountered
        //the problem yet with test data, but will continue to keep an eye on it.
        map->addElement(element);
      }

      LOG_DEBUG("Retrieving way node IDs referenced by the selected ways...");
      QSet<QString> additionalWayNodeIds;
      shared_ptr<QSqlQuery> additionalWayNodeIdItr =
        _getDatabase()->selectWayNodeIdsByWayIds(wayIds);
      while (additionalWayNodeIdItr->next())
      {
        const QString nodeId = QString::number((*additionalWayNodeIdItr).value(0).toLongLong());
        LOG_VART(nodeId);
        additionalWayNodeIds.insert(nodeId);
      }

      //subtract nodeIds from additionalWayNodeIds so no dupes get added
      LOG_VARD(nodeIds.size());
      LOG_VARD(additionalWayNodeIds.size());
      additionalWayNodeIds = additionalWayNodeIds.subtract(nodeIds);
      LOG_VARD(additionalWayNodeIds.size());

      if (additionalWayNodeIds.size() > 0)
      {
        nodeIds.unite(additionalWayNodeIds);
        LOG_DEBUG(
          "Retrieving nodes falling outside of the query bounds but belonging to a selected way...");
        shared_ptr<QSqlQuery> additionalWayNodeItr =
          _getDatabase()->selectElementsByElementIdList(additionalWayNodeIds, TableType::Node);
        while (additionalWayNodeItr->next())
        {
          NodePtr element = _resultToNode(*additionalWayNodeItr, *map);
          LOG_VART(element->toString());
          map->addElement(element);
        }
      }
    }

    LOG_DEBUG("Retrieving relation IDs referenced by the selected ways and nodes...");
    QSet<QString> relationIds;
    assert(nodeIds.size() > 0);
    shared_ptr<QSqlQuery> relationIdItr =
      _getDatabase()->selectRelationIdsByMemberIds(nodeIds, ElementType::Node);
    while (relationIdItr->next())
    {
      const QString relationId = QString::number((*relationIdItr).value(0).toLongLong());
      LOG_VART(relationId);
      relationIds.insert(relationId);
    }
    if (wayIds.size() > 0)
    {
      relationIdItr = _getDatabase()->selectRelationIdsByMemberIds(wayIds, ElementType::Way);
      while (relationIdItr->next())
      {
        const QString relationId = QString::number((*relationIdItr).value(0).toLongLong());
        LOG_VART(relationId);
        relationIds.insert(relationId);
      }
    }
    LOG_VARD(relationIds.size());

    if (relationIds.size() > 0)
    {
      LOG_DEBUG("Retrieving relations by relation ID...");
      shared_ptr<QSqlQuery> relationItr =
        _getDatabase()->selectElementsByElementIdList(relationIds, TableType::Relation);
      while (relationItr->next())
      {
        RelationPtr element = _resultToRelation(*relationItr, *map);
        LOG_VART(element->toString());
        map->addElement(element);
      }
    }
  }
}

}
