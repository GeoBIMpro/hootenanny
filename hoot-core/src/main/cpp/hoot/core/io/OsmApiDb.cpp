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
#include "OsmApiDb.h"

// hoot
#include <hoot/core/elements/Node.h>
#include <hoot/core/elements/Way.h>
#include <hoot/core/elements/Relation.h>
#include <hoot/core/io/db/SqlBulkInsert.h>
#include <hoot/core/util/ConfigOptions.h>
#include <hoot/core/util/HootException.h>
#include <hoot/core/util/Log.h>
#include <hoot/core/io/ElementCacheLRU.h>
#include <hoot/core/util/OsmUtils.h>
#include <hoot/core/util/GeometryUtils.h>

// qt
#include <QStringList>
#include <QVariant>
#include <QtSql/QSqlError>
#include <QtSql/QSqlRecord>
#include <QFile>
#include <QTextStream>

// Standard
#include <math.h>
#include <cmath>
#include <fstream>

// tgs
#include <tgs/System/Time.h>

#include "db/InternalIdReserver.h"

namespace hoot
{

const QString OsmApiDb::TIME_FORMAT = "yyyy-MM-dd HH:mm:ss.zzz";
const QString OsmApiDb::TIMESTAMP_FUNCTION = "(now() at time zone 'utc')";

OsmApiDb::OsmApiDb()
{
  _init();
}

OsmApiDb::~OsmApiDb()
{
  close();
}

void OsmApiDb::_init()
{
  _floatingPointCoords = false;
  _capitalizeRelationMemberType = true;
  _inTransaction = false;
  _resetQueries();
}

void OsmApiDb::close()
{
  _resetQueries();

  if (_inTransaction)
  {
    LOG_WARN("Closing database before transaction is committed. Rolling back transaction.");
    rollback();
  }

  // Seeing this? "Unable to free statement: connection pointer is NULL"
  // Make sure all queries are listed in _resetQueries.
  _db.close();
}

void OsmApiDb::deleteData()
{
  // delete ways data first
  _execNoPrepare("DELETE FROM current_relation_members CASCADE");
  _execNoPrepare("DELETE FROM current_relation_tags CASCADE");
  _execNoPrepare("DELETE FROM current_relations CASCADE");
  _execNoPrepare("ALTER SEQUENCE current_relations_id_seq RESTART WITH 1");
  _execNoPrepare("DELETE FROM relation_members CASCADE");
  _execNoPrepare("DELETE FROM relation_tags CASCADE");
  _execNoPrepare("DELETE FROM relations CASCADE");

  // delete relations data 2nd
  _execNoPrepare("DELETE FROM current_way_nodes CASCADE");
  _execNoPrepare("DELETE FROM current_way_tags CASCADE");
  _execNoPrepare("DELETE FROM current_ways CASCADE");
  _execNoPrepare("ALTER SEQUENCE current_ways_id_seq RESTART WITH 1");
  _execNoPrepare("DELETE FROM way_nodes CASCADE");
  _execNoPrepare("DELETE FROM way_tags CASCADE");
  _execNoPrepare("DELETE FROM ways CASCADE");

  // delete nodes data 3rd
  _execNoPrepare("DELETE FROM current_node_tags CASCADE");
  _execNoPrepare("DELETE FROM current_nodes CASCADE");
  _execNoPrepare("ALTER SEQUENCE current_nodes_id_seq RESTART WITH 1");
  _execNoPrepare("DELETE FROM node_tags CASCADE");
  _execNoPrepare("DELETE FROM nodes CASCADE");

  // delete changesets
  _execNoPrepare("DELETE FROM changesets_subscribers CASCADE");
  _execNoPrepare("DELETE FROM changeset_tags CASCADE");
  _execNoPrepare("DELETE FROM changesets CASCADE");
  _execNoPrepare("ALTER SEQUENCE changesets_id_seq RESTART WITH 1");

  // delete users
  _execNoPrepare("DELETE FROM users CASCADE");
  _execNoPrepare("ALTER SEQUENCE users_id_seq RESTART WITH 1");
}

bool OsmApiDb::isSupported(QUrl url)
{
  bool valid = ApiDb::isSupported(url);

  //postgresql is deprecated but still supported
  if (url.scheme() != "osmapidb" && url.scheme() != "postgresql")
  {
    valid = false;
  }

  if (valid)
  {
    QString path = url.path();
    QStringList plist = path.split("/");

    // OSM API will have plist.size of 2.
    if ( plist.size() != 2 )
    {
      valid = false;
    }
  }

  return valid;
}

void OsmApiDb::open(const QUrl& url)
{
  if (!isSupported(url))
  {
    throw HootException("An unsupported URL was passed into OsmApiDb: " + url.toString());
  }
  ApiDb::open(url);
}

void OsmApiDb::deleteUser(long userId)
{
  _exec("DELETE FROM users WHERE id=:id", (qlonglong)userId);
}

void OsmApiDb::_resetQueries()
{
  ApiDb::_resetQueries();

  _selectElementsForMap.reset();
  _selectTagsForNode.reset();
  _selectTagsForWay.reset();
  _selectTagsForRelation.reset();
  _selectNodeIdsForWay.reset();
  _selectMembersForRelation.reset();
  _selectNodeById.reset();
  _selectUserByEmail.reset();
  _insertUser.reset();
  for (QHash<QString, shared_ptr<QSqlQuery> >::iterator itr = _seqQueries.begin();
       itr != _seqQueries.end(); ++itr)
  {
    itr.value().reset();
  }
}

void OsmApiDb::rollback()
{
  _resetQueries();

  if (!_db.rollback())
  {
    LOG_WARN("Error rolling back transaction.");
    throw HootException("Error rolling back transaction: " + _db.lastError().text());
  }
  _inTransaction = false;
}

void OsmApiDb::transaction()
{
  // Queries must be created from within the current transaction.
  _resetQueries();
  if (!_db.transaction())
  {
    throw HootException(_db.lastError().text());
  }
  _inTransaction = true;
}

void OsmApiDb::commit()
{
  if ( _db.isOpen() == false )
  {
    throw HootException("Tried to commit a transaction on a closed database");
  }

  if ( _inTransaction == false )
  {
    throw HootException("Tried to commit but weren't in a transaction");
  }

  _resetQueries();
  if (!_db.commit())
  {
    LOG_WARN("Error committing transaction.");
    throw HootException("Error committing transaction: " + _db.lastError().text());
  }
  _inTransaction = false;
}

QString OsmApiDb::tableTypeToTableName(const TableType& tableType) const
{
  if (tableType == TableType::Node)
  {
    return "current_nodes";
  }
  else if (tableType == TableType::Way)
  {
    return "current_ways";
  }
  else if (tableType == TableType::Relation)
  {
    return "current_relations";
  }
  else if (tableType == TableType::WayNode)
  {
    return "current_way_nodes";
  }
  else if (tableType == TableType::RelationMember)
  {
    return "current_relation_members";
  }
  else
  {
    throw HootException("Unsupported table type.");
  }
}

//TODO: this needs to be better named
QString OsmApiDb::_elementTypeToElementTableName(const ElementType& elementType) const
{
  if (elementType == ElementType::Node)
  {
    return QString("id, latitude, longitude, changeset_id, visible, timestamp, tile, version, k, v ") +
      QString("from current_nodes left outer join current_node_tags on current_nodes.id=current_node_tags.node_id");
  }
  else if (elementType == ElementType::Way)
  {
    return QString("id, changeset_id, timestamp, visible, version, k, v ")+
      QString("from current_ways left outer join current_way_tags on current_ways.id=current_way_tags.way_id");
  }
  else if (elementType == ElementType::Relation)
  {
    return QString("id, changeset_id, timestamp, visible, version, k, v ")+
      QString("from current_relations left outer join current_relation_tags on current_relations.id=current_relation_tags.relation_id");
  }
  else
  {
    throw HootException("Unsupported element type.");
  }
}

vector<long> OsmApiDb::selectNodeIdsForWay(long wayId)
{
  QString sql =  "SELECT ";
  sql += "node_id FROM " + _getWayNodesTableName() + " WHERE way_id = :wayId ORDER BY sequence_id";

  return ApiDb::selectNodeIdsForWay(wayId, sql);
}

shared_ptr<QSqlQuery> OsmApiDb::selectNodesForWay(long wayId)
{
  QString sql =  "SELECT ";
  sql += "node_id, latitude, longitude FROM current_way_nodes inner join current_nodes on current_way_nodes.node_id=current_nodes.id and way_id = :wayId ORDER BY sequence_id";
  return ApiDb::selectNodesForWay(wayId, sql);
}

vector<RelationData::Entry> OsmApiDb::selectMembersForRelation(long relationId)
{
  vector<RelationData::Entry> result;

  if (!_selectMembersForRelation)
  {
    _selectMembersForRelation.reset(new QSqlQuery(_db));
    _selectMembersForRelation->setForwardOnly(true);
    _selectMembersForRelation->prepare(
      "SELECT member_type, member_id, member_role FROM " + _getRelationMembersTableName() +
      " WHERE relation_id = :relationId ORDER BY sequence_id");
  }

  _selectMembersForRelation->bindValue(":relationId", (qlonglong)relationId);
  if (_selectMembersForRelation->exec() == false)
  {
    throw HootException("Error selecting members for relation with ID: " +
      QString::number(relationId) + " Error: " + _selectMembersForRelation->lastError().text());
  }

  while (_selectMembersForRelation->next())
  {
    const QString memberType = _selectMembersForRelation->value(0).toString();
    if (ElementType::isValidTypeString(memberType))
    {
      result.push_back(
        RelationData::Entry(
          _selectMembersForRelation->value(2).toString(),
          ElementId(ElementType::fromString(memberType),
          _selectMembersForRelation->value(1).toLongLong())));
    }
    else
    {
        LOG_WARN("Invalid relation member type: " + memberType + ".  Skipping relation member.");
    }
  }

  return result;
}

shared_ptr<QSqlQuery> OsmApiDb::selectNodeById(const long elementId)
{
  _selectNodeById.reset(new QSqlQuery(_db));
  _selectNodeById->setForwardOnly(true);
  QString sql =
    "SELECT " + _elementTypeToElementTableName(ElementType::Node) +
    " WHERE (id=:elementId) ORDER BY id DESC";
  _selectNodeById->prepare(sql);
  _selectNodeById->bindValue(":elementId", (qlonglong)elementId);

  LOG_TRACE(QString("The sql query= "+ sql));

  // execute the query on the DB and get the results back
  if (_selectNodeById->exec() == false)
  {
    QString err = _selectNodeById->lastError().text();
    LOG_WARN(sql);
    throw HootException("Error selecting node by id: " + QString::number(elementId) +
      " Error: " + err);
  }

  return _selectNodeById;
}

shared_ptr<QSqlQuery> OsmApiDb::selectElements(const ElementType& elementType)
{
  _selectElementsForMap.reset(new QSqlQuery(_db));
  _selectElementsForMap->setForwardOnly(true);

  // setup base sql query string
  QString sql =  "SELECT " + _elementTypeToElementTableName(elementType);

  // sort them in descending order, set limit and offset
  sql += " WHERE visible = true ORDER BY id DESC";

  // let's see what that sql query string looks like
  LOG_DEBUG(QString("The sql query= "+sql));

  _selectElementsForMap->prepare(sql);

  // execute the query on the DB and get the results back
  if (_selectElementsForMap->exec() == false)
  {
    QString err = _selectElementsForMap->lastError().text();
    LOG_WARN(sql);
    throw HootException("Error selecting elements of type: " + elementType.toString() +
      " Error: " + err);
  }

  return _selectElementsForMap;
}

shared_ptr<QSqlQuery> OsmApiDb::selectTagsForRelation(long relId)
{
  if (!_selectTagsForRelation)
  {
    _selectTagsForRelation.reset(new QSqlQuery(_db));
    _selectTagsForRelation->setForwardOnly(true);
    QString sql =  "SELECT ";
    sql += "relation_id, k, v FROM current_relation_tags where relation_id = :relId";
    _selectTagsForRelation->prepare( sql );
  }

  _selectTagsForRelation->bindValue(":relId", (qlonglong)relId);
  if (_selectTagsForRelation->exec() == false)
  {
    throw HootException("Error selecting tags for relation with ID: " + QString::number(relId) +
      " Error: " + _selectTagsForRelation->lastError().text());
  }

  return _selectTagsForRelation;
}

shared_ptr<QSqlQuery> OsmApiDb::selectTagsForWay(long wayId)
{
  if (!_selectTagsForWay)
  {
    _selectTagsForWay.reset(new QSqlQuery(_db));
    _selectTagsForWay->setForwardOnly(true);
    QString sql =  "SELECT ";
    sql += "way_id, k, v FROM current_way_tags where way_id = :wayId";
    _selectTagsForWay->prepare( sql );
  }

  _selectTagsForWay->bindValue(":wayId", (qlonglong)wayId);
  if (_selectTagsForWay->exec() == false)
  {
    throw HootException("Error selecting tags for way with ID: " + QString::number(wayId) +
      " Error: " + _selectTagsForWay->lastError().text());
  }

  return _selectTagsForWay;
}

shared_ptr<QSqlQuery> OsmApiDb::selectTagsForNode(long nodeId)
{
  if (!_selectTagsForNode)
  {
    _selectTagsForNode.reset(new QSqlQuery(_db));
    _selectTagsForNode->setForwardOnly(true);
    QString sql =  "SELECT ";
    sql += "node_id, k, v FROM current_node_tags where node_id = :nodeId";
    _selectTagsForNode->prepare( sql );
  }

  _selectTagsForNode->bindValue(":nodeId", (qlonglong)nodeId);
  if (_selectTagsForNode->exec() == false)
  {
    throw HootException("Error selecting tags for node with ID: " + QString::number(nodeId) +
      " Error: " + _selectTagsForNode->lastError().text());
  }

  return _selectTagsForNode;
}

QString OsmApiDb::extractTagFromRow(shared_ptr<QSqlQuery> row, const ElementType::Type type)
{
  QString tag = "";
  int pos = -1;
  if(type==ElementType::Node) pos=OsmApiDb::NODES_TAGS;
  else if(type==ElementType::Way) pos=OsmApiDb::WAYS_TAGS;
  else if(type==ElementType::Relation) pos=OsmApiDb::RELATIONS_TAGS;
  else throw HootException("extractTagFromRow_OsmApi called with unknown Type");

  // test for blank tag
  QString val1 = row->value(pos).toString();
  QString val2 = row->value(pos+1).toString();
  if(val1!="" || val2!="") tag = "\""+val1+"\"=>\""+val2+"\"";

  return tag;
}

long OsmApiDb::getNextId(const ElementType& type)
{
  switch (type.getEnum())
  {
    case ElementType::Node:
      return getNextId("current_" + type.toString().toLower() + "s");
    case ElementType::Way:
      return getNextId("current_" + type.toString().toLower() + "s");
    case ElementType::Relation:
      return getNextId("current_" + type.toString().toLower() + "s");
    default:
      throw HootException("Unknown element type");
  }
}

long OsmApiDb::getNextId(const QString tableName)
{
  long result;
  if (_seqQueries[tableName].get() == 0)
  {
    _seqQueries[tableName].reset(new QSqlQuery(_db));
    _seqQueries[tableName]->setForwardOnly(true);
    _seqQueries[tableName]->prepare(QString("SELECT NEXTVAL('%1_id_seq')").arg(tableName.toLower()));
  }

  shared_ptr<QSqlQuery> query = _seqQueries[tableName];
  if (query->exec() == false)
  {
    throw HootException("Error reserving IDs. type: " +
      tableName + " Error: " + query->lastError().text());
  }

  if (query->next())
  {
    bool ok;
    result = query->value(0).toLongLong(&ok);
    if (!ok)
    {
      throw HootException("Did not retrieve starting reserved ID.");
    }
  }
  else
  {
    throw HootException("Error retrieving sequence value. type: " +
      tableName + " Error: " + query->lastError().text());
  }

  query->finish();

  return result;
}

long OsmApiDb::toOsmApiDbCoord(const double x)
{
  return round(x * COORDINATE_SCALE);
}

double OsmApiDb::fromOsmApiDbCoord(const long x)
{
  return (double)x / COORDINATE_SCALE;
}

}