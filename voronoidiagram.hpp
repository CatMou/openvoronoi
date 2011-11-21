/*  
 *  Copyright 2010-2011 Anders Wallin (anders.e.e.wallin "at" gmail.com)
 *  
 *  This file is part of OpenVoronoi.
 *
 *  OpenCAMlib is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  OpenCAMlib is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with OpenCAMlib.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef VORONOI_DIAGRAM_HPP
#define VORONOI_DIAGRAM_HPP

#include <queue>
#include <boost/tuple/tuple.hpp>

#include "point.hpp"
#include "graph.hpp"
#include "vertex_positioner.hpp"
#include "version_string.hpp" // autogenerated by version_string.cmake

namespace ovd
{

class VoronoiDiagramChecker;
class FaceGrid;


typedef std::pair<HEVertex, double> VertexDetPair;
class abs_comparison {
public:
  bool operator() (const VertexDetPair& lhs, const VertexDetPair&rhs) const {
    return ( fabs(lhs.second) < fabs(rhs.second) );
  }
};

// vertices for processing held in this queue
// sorted by decreasing fabs() of in_circle-predicate, so that the vertices whose IN/OUT status we are 'most certain' about are processed first
typedef std::priority_queue< VertexDetPair , std::vector<VertexDetPair>, abs_comparison > VertexQueue;

struct EdgeData {
    HEEdge v1_prv;
    HEVertex v1;
    HEEdge v1_nxt;
    HEEdge v2_prv;
    HEVertex v2;
    HEEdge v2_nxt;
    HEFace f;
};



/// \brief Voronoi diagram.
///
/// see http://en.wikipedia.org/wiki/Voronoi_diagram
/// 
/// the dual of a voronoi diagram is the delaunay diagram(triangulation).
///  voronoi-faces are dual to delaunay-vertices.
///  vornoi-vertices are dual to delaunay-faces 
///  voronoi-edges are dual to delaunay-edges
class VoronoiDiagram {
    public:
        /// ctor
        VoronoiDiagram() {}
        /// create diagram with given far-radius and number of bins
        /// \param far radius of circle centered at (0,0) within which all sites must lie
        /// \param n_bins number of bins used for nearest vd-vertex bucket-search
        VoronoiDiagram(double far, unsigned int n_bins);
        /// dtor
        virtual ~VoronoiDiagram();
        /// insert a point site into the diagram 
        /// returns an integer handle to the inserted point. use this integer when inserting lines/arcs
        int insert_point_site(const Point& p, int step=0);
        /// insert a line-segment site into the diagram
        bool insert_line_site(int idx1, int idx2, int step=10);
        /// return the far radius
        double get_far_radius() const {return far_radius;}
        /// return number of point sites in diagram
        int num_point_sites() const {return num_psites-3;} // the three initial vertices don't count
        /// return number of line-segments sites in diagram
        int num_line_sites() const {return num_lsites;}
        /// return number of voronoi-vertices
        int num_vertices() const { return g.num_vertices()-num_point_sites(); }
        int num_split_vertices();
        /// string repr
        std::string print() const;
        std::string version() const { return VERSION_STRING; }
        friend class VoronoiDiagramChecker;
        friend class VertexPositioner;
        friend class SplitPointError;
        static void reset_vertex_count() { VoronoiVertex::reset_count(); }
    protected:
        /// initialize the diagram with three generators
        void initialize();
        HEVertex   find_seed_vertex(HEFace f, Site* site) const;
        EdgeVector find_in_out_edges(); 
        EdgeData   find_edge_data(HEFace f, VertexVector startverts=VertexVector());
        EdgeVector find_split_edges(HEFace f, Point pt1, Point pt2);
        
        void augment_vertex_set( Site* site);        
        bool predicate_c4(HEVertex v);
        bool predicate_c5(HEVertex v);
        void mark_adjacent_faces(HEVertex v, Site* site);
        void mark_vertex(HEVertex& v,  Site* site); 
        void   add_vertices( Site* site );
        HEFace add_face(Site* site);
        void   add_edge(HEFace new_f1, HEFace f, HEFace new_f2 = 0);
        void   add_edge(EdgeData ed, HEFace new1, HEFace new2=0);
        void   add_vertex_in_edge(HEVertex v, HEEdge e);
        void   add_separator(HEFace f, HEVertex endp, Site* s1, Site* s2);
        void   add_split_vertex(HEFace f, Site* s);
        
        void repair_face( HEFace f );
        void remove_vertex_set();
        void remove_split_vertex(HEFace f);
        void reset_status();
        int new_vertex_count(HEFace f);
    // PRINT ETC
        void print_faces();
        void print_face(HEFace f);
        void print_vertices(VertexVector& q);
        void print_edges(EdgeVector& q);
    // HELPER-CLASSES
        /// sanity-checks on the diagram are done by this helper class
        VoronoiDiagramChecker* vd_checker;
        /// a grid-search algorithm which allows fast nearest-neighbor search
        FaceGrid* fgrid;
        /// an algorithm for positioning vertices
        VertexPositioner* vpos;
    // DATA
        /// the half-edge diagram of the vd
        HEGraph g;
        /// the voronoi diagram is constructed for sites within a circle with radius far_radius
        double far_radius;
        /// the number of point sites
        int num_psites;
        /// the number of line-segment sites
        int num_lsites;
        /// temporary variable for incident faces, will be reset to NONINCIDENT after a site has been inserted
        FaceVector incident_faces;
        /// temporary variable for in-vertices, out-vertices that need to be reset
        /// after a site has been inserted
        VertexVector modified_vertices;
        /// IN-vertices, i.e. to-be-deleted
        VertexVector v0;
        /// queue of vertices to be processed
        VertexQueue vertexQueue; 
        std::map<int,HEVertex> vertex_map;
};

// class for passing to numerical boost::toms748 root-finding algorithm
// to locate split-points
class SplitPointError {
public:
    SplitPointError(VoronoiDiagram* v, HEEdge split_edge,Point pt1, Point pt2) :
    vd(v), edge(split_edge), p1(pt1), p2(pt2)
    {}
    
    // find point on edge at t-value
    // compute a signed distance to the pt1-pt2 line
    double operator()(const double t) {
        Point p = vd->g[edge].point(t);
        // line: pt1 + u*(pt2-pt1) = p
        //   (p-pt1) dot (pt2-pt1) = u* (pt2-pt1) dot (pt2-pt1)
        
        double u = (p-p1).dot(p2-p1) / ( (p2-p1).dot(p2-p1) );
        Point proj = p1 + u*(p2-p1);
        double dist = (proj-p).norm();
        double sign;
        if ( p.is_right(p1,p2) )
            sign = +1;
        else
            sign = -1;
            
        return sign*dist;
    }
private:
    VoronoiDiagram* vd;
    HEEdge edge;
    Point p1;
    Point p2;
};

} // end namespace
#endif
// end voronoidiagram.h
