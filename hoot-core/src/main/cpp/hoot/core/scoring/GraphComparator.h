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

#ifndef GRAPHCOMPARATOR_H
#define GRAPHCOMPARATOR_H

#include "BaseComparator.h"

// hoot
#include <hoot/core/OsmMap.h>

namespace hoot
{
  using namespace geos::geom;
  using namespace std;

  class DirectedGraph;
  class ShortestPath;
  class Way;

class GraphComparator : public BaseComparator
{
public:
  GraphComparator(shared_ptr<OsmMap> map1, shared_ptr<OsmMap> map2);

  virtual ~GraphComparator() {}

  double compareMaps();

  /**
   * Returns the 90% confidence interval.
   */
  double getConfidenceInterval() { return _ci; }

  double getMeanScore() { return _mean; }

  double getMedianScore() { return _median; }

  double getStandardDeviation() { return _s; }

  void setDebugImages(bool di) { _debugImages = di; }

  void setIterations(int i) { _iterations = i; }

  void drawCostDistance(shared_ptr<OsmMap> map, vector<Coordinate>& c, QString output);

private:

  int _iterations;
  Coordinate _r;
  double _median;
  double _mean;
  // confidence interval
  double _ci;
  // sampled standard deviation
  double _s;
  double _maxGraphCost;
  bool _debugImages;

  cv::Mat _calculateCostDistance(shared_ptr<OsmMap> map, Coordinate c);

  void _calculateRasterCost(cv::Mat& mat);

  void _exportGraphImage(shared_ptr<OsmMap> map, DirectedGraph& graph, ShortestPath& sp,
                         QString path);

  void _init();

  cv::Mat _paintGraph(shared_ptr<OsmMap> map, DirectedGraph& graph, ShortestPath& sp);

  void _paintWay(cv::Mat& mat, ConstOsmMapPtr map, shared_ptr<Way> way, double friction,
    double startCost, double endCost);
};

}

#endif // GRAPHCOMPARATOR_H
