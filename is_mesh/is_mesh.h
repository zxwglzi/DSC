#ifndef PROGRESSIVE_MESH_INCIDENCE_SIMPLICIAL_H
#define PROGRESSIVE_MESH_INCIDENCE_SIMPLICIAL_H

// Disclaimer / copyrights / stuff

#include <algorithm>
#include <functional>
#include <queue>
#include <map>
#include <list>

#include <is_mesh/kernel.h>
#include <is_mesh/is_mesh_default_traits.h>
#include <is_mesh/is_mesh_key_type.h>
#include <is_mesh/is_mesh_utils.h>
#include <is_mesh/is_mesh_simplex.h>
#include <is_mesh/simplex_set.h>

namespace OpenTissue
{
    namespace is_mesh
    {
        /**
         * A data structure for managing a Simplicial Complex. Based on the work
         * by de Floriani and Hui, the Incidence Simplicial.
         * The complex is specialixed for 3-dimensional simplices, and can only 
         * store 0-, 1-, 2-, nad 3-simplices. 
         * Simplices are explicitly stored using a different memory kernel for
         * each type.h
         */
        template<
        typename NodeTraits         = default_node_traits
        , typename TetrahedronTraits  = default_tetrahedron_traits
        , typename EdgeTraits         = default_edge_traits
        , typename FaceTraits         = default_face_traits
        >
        class t4mesh
        {
        public:
            typedef t4mesh<NodeTraits, TetrahedronTraits, EdgeTraits, FaceTraits>        mesh_type;
            
            typedef NodeKey                                                              node_key_type;
            typedef EdgeKey                                                              edge_key_type;
            typedef FaceKey                                                              face_key_type;
            typedef TetrahedronKey                                                       tetrahedron_key_type;
            
            typedef NodeTraits                                                           node_traits;
            typedef TetrahedronTraits                                                    tetrahedron_traits;
            typedef EdgeTraits                                                           edge_traits;
            typedef FaceTraits                                                           face_traits;
            
            typedef Node<NodeTraits, TetrahedronTraits, EdgeTraits, FaceTraits>          node_type;
            typedef Edge<NodeTraits, TetrahedronTraits, EdgeTraits, FaceTraits>          edge_type;
            typedef Face<NodeTraits, TetrahedronTraits, EdgeTraits, FaceTraits>          face_type;
            typedef Tetrahedron<NodeTraits, TetrahedronTraits, EdgeTraits, FaceTraits>   tetrahedron_type;
            
            typedef          simplex_set<node_key_type, edge_key_type
            , face_key_type, tetrahedron_key_type>            simplex_set_type;
            
        public:
            typedef          kernel<node_type, node_key_type>                            node_kernel_type;
            typedef          kernel<edge_type, edge_key_type>                            edge_kernel_type;
            typedef          kernel<face_type, face_key_type>                            face_kernel_type;
            typedef          kernel<tetrahedron_type, tetrahedron_key_type>              tetrahedron_kernel_type;
            
        public:
            
            typedef typename node_kernel_type::iterator                                  node_iterator;
            typedef typename edge_kernel_type::iterator                                  edge_iterator;
            typedef typename face_kernel_type::iterator                                  face_iterator;
            typedef typename tetrahedron_kernel_type::iterator                           tetrahedron_iterator;
            
            //expose some types from the kernel
            typedef typename node_kernel_type::size_type                                 size_type;
            
        public:
            struct node_undo_info
            {
                node_key_type key;
                typename node_type::co_boundary_set old_co_boundary;
                typename node_type::co_boundary_set new_co_boundary;
            };
            
            struct edge_undo_info
            {
                edge_key_type key;
                typename edge_type::boundary_list old_boundary;
                typename edge_type::boundary_list new_boundary;
                typename edge_type::co_boundary_set old_co_boundary;
                typename edge_type::co_boundary_set new_co_boundary;
            };
            
            struct face_undo_info
            {
                face_key_type key;
                typename face_type::boundary_list old_boundary;
                typename face_type::boundary_list new_boundary;
                typename face_type::co_boundary_set old_co_boundary;
                typename face_type::co_boundary_set new_co_boundary;
            };
            
            struct tetrahedron_undo_info
            {
                tetrahedron_key_type key;
                typename tetrahedron_type::boundary_list old_boundary;
                typename tetrahedron_type::boundary_list new_boundary;
            };
            
        private:
            node_kernel_type*                  m_node_kernel;
            edge_kernel_type*                  m_edge_kernel;
            face_kernel_type*                  m_face_kernel;
            tetrahedron_kernel_type*           m_tetrahedron_kernel;
            
            std::vector<node_undo_info>        m_node_undo_stack;
            std::vector<edge_undo_info>        m_edge_undo_stack;
            std::vector<face_undo_info>        m_face_undo_stack;
            std::vector<tetrahedron_undo_info> m_tetrahedron_undo_stack;
            
            std::list<unsigned int>            m_node_mark_stack;
            std::list<unsigned int>            m_edge_mark_stack;
            std::list<unsigned int>            m_face_mark_stack;
            std::list<unsigned int>            m_tetrahedron_mark_stack;
            
            size_type                          m_uncompressed; //an estimate of the numbers of uncompressed simplices in the mesh
            
        private:
            
            /**
             *
             */
            node_type & lookup_simplex(node_key_type const & k)
            {
                return m_node_kernel->find(k);
            }
            
            /**
             *
             */
            edge_type & lookup_simplex(edge_key_type const & k)
            {
                return m_edge_kernel->find(k);
            }
            
            /**
             *
             */
            face_type & lookup_simplex(face_key_type const & k)
            {
                return m_face_kernel->find(k);
            }
            
            /**
             *
             */
            tetrahedron_type & lookup_simplex(tetrahedron_key_type const & k)
            {
                return m_tetrahedron_kernel->find(k);
            }
            
            /**
             * Helper function for labeling simplices in a connected component.
             * Is doubly recursive with traverse_co_boundary
             * Marek: Will it still work for the non-manifold case? I ain't sure.
             */
            template<typename seed_simplex_key
            , typename simplex_type>
            void label_co_co_bound(seed_simplex_key& ssk, simplex_type& s, int label)
            {
                assert(s.get_label() == 0 || !"traverse_co_co_bound called with simplex already labled");
                s.set_label(label);
                
                typename simplex_type::boundary_list bound = s.get_boundary();
                typename simplex_type::boundary_iterator bound_it = bound->begin();
                for(; bound_it != bound->end(); ++bound_it)
                {
                    typedef typename util::simplex_traits<mesh_type, simplex_type::dim-1>::simplex_type  boundary_type;
                    boundary_type& simplex = lookup_simplex(*bound_it);
                    if (simplex.get_label() == 0)
                    {
                        simplex_set_type s_boundary;
                        boundary(*bound_it, s_boundary);
                        if (s_boundary.contains(ssk))
                            label_co_bound(ssk, simplex, label);
                    }
                }
            }
            
            /**
             * Helper function for labeling simplices in a connected component.
             * Is doubly recursive with traverse_co_co_boundary
             * Marek: Will it still work for the non-manifold case? I ain't sure.
             */
            template<typename seed_simplex_key
            , typename simplex_type>
            void  label_co_bound(seed_simplex_key& ssk, simplex_type& s, int label)
            {
                assert(s.get_label() == 0 || !"traverse_co_bound called with simplex already labled");
                s.set_label(label);
                
                typename simplex_type::co_boundary_set co_bound = s.get_co_boundary();
                typename simplex_type::co_boundary_iterator co_bound_it = co_bound->begin();
                for( ; co_bound_it != co_bound->end() ; ++co_bound_it )
                {
                    //convinience typedefs
                    typedef typename util::simplex_traits<mesh_type, simplex_type::dim+1>::simplex_type  co_boundary_type;
                    co_boundary_type& simplex = lookup_simplex(*co_bound_it);
                    if (simplex.get_label() == 0)
                    {
                        simplex_set_type s_boundary;
                        boundary(*co_bound_it, s_boundary);
                        if (s_boundary.contains(ssk))
                            label_co_co_bound(ssk, simplex, label);
                    }
                }
            }
            
            /**
             * Marek
             * Helper function for boundary and closure.
             */
            void boundary_helper(tetrahedron_key_type const & k, simplex_set_type & set)
            {
                typename tetrahedron_type::boundary_list s_boundary = lookup_simplex(k).get_boundary();
                typename tetrahedron_type::boundary_iterator it = s_boundary->begin();
                while (it != s_boundary->end())
                {
                    set.insert(*it);
                    boundary_helper(*it, set);
                    ++it;
                }
            }
            
            void boundary_helper(face_key_type const & k, simplex_set_type & set)
            {
                typename face_type::boundary_list s_boundary = lookup_simplex(k).get_boundary();
                typename face_type::boundary_iterator it = s_boundary->begin();
                while (it != s_boundary->end())
                {
                    set.insert(*it);
                    boundary_helper(*it, set);
                    ++it;
                }
            }
            
            void boundary_helper(edge_key_type const & k, simplex_set_type & set)
            {
                typename edge_type::boundary_list s_boundary = lookup_simplex(k).get_boundary();
                typename edge_type::boundary_iterator it = s_boundary->begin();
                while (it != s_boundary->end())
                {
                    set.insert(*it);
                    ++it;
                }
            }
            
            void boundary_helper(node_key_type const & k, simplex_set_type & set) {}
            
            /**
             * Marek
             * Helper for the boundary of a k-manifold patch.
             */
            template<typename key_type_simplex
            , typename key_type_face>
            void boundary_helper2(simplex_set_type & patch, simplex_set_type & result_set)
            {
                typedef typename util::simplex_traits<mesh_type, key_type_simplex::dim>::simplex_type simplex_type;
                typename util::simplex_traits<mesh_type, key_type_simplex::dim>::simplex_tag tag;
                
                std::map<key_type_face, char> face_occurrences;
                typename simplex_set_type::iterator_type<key_type_simplex::dim>::iterator pit = patch.begin(tag);
                
                while (pit != patch.end(tag))
                {
                    typename simplex_type::boundary_list s_boundary = lookup_simplex(*pit).get_boundary();
                    typename simplex_type::boundary_iterator bit = s_boundary->begin();
                    while (bit != s_boundary->end())
                    {
                        face_occurrences[*bit]++;
                        ++bit;
                    }
                    ++pit;
                }
                
                typename std::map<key_type_face, char>::iterator foit = face_occurrences.begin();
                simplex_set_type boundary_faces;
                
                while (foit != face_occurrences.end())
                {
                    if (foit->second == 1) boundary_faces.insert(foit->first);
                    ++foit;
                }
                
                closure(boundary_faces, result_set);
            }
            
            /**
             * Marek
             * Helper function for the optimal performance closure of a simplex set routine
             */
            template<int dim>
            void closure_helper(simplex_set_type & input_set, 
                                simplex_set_type & to_be_evaluated_set, 
                                simplex_set_type & result_set)
            {
                if (dim == 0)
                {
                    typename simplex_set_type::iterator_type<0>::iterator nodes_it = input_set.nodes_begin();
                    while (nodes_it != input_set.nodes_end())
                    {
                        result_set.insert(*nodes_it);
                        ++nodes_it;
                    }
                    return;
                }
                
                typedef typename util::simplex_traits<mesh_type, dim>::simplex_type simplex_type;
                typename util::simplex_traits<mesh_type, dim>::simplex_tag tag;
                
                typename simplex_set_type::iterator_type<dim>::iterator simplex_it = input_set.begin(tag);
                
                while (simplex_it != input_set.end(tag))
                {
                    to_be_evaluated_set.insert(*simplex_it);
                    ++simplex_it;
                }
                
                simplex_it = to_be_evaluated_set.begin(tag);
                while (simplex_it != to_be_evaluated_set.end(tag))
                {
                    typename simplex_type::boundary_list s_boundary = lookup_simplex(*simplex_it).get_boundary();
                    typename simplex_type::boundary_iterator it = s_boundary->begin();
                    while (it != s_boundary->end())
                    {
                        if (it == s_boundary->begin())
                            result_set.insert(*it);
                        else
                            to_be_evaluated_set.insert(*it);
                        ++it;
                    }
                    ++simplex_it;
                }
                
                closure_helper<dim-1>(input_set, to_be_evaluated_set, result_set);
            }
            
            void closure_helper(simplex_set_type & input_set, simplex_set_type & set)
            {
                typename simplex_set_type::tetrahedron_set_iterator tit = input_set.tetrahedra_begin();
                
                while (tit != input_set.tetrahedra_end())
                {
                    set.insert(*tit);
                    ++tit;
                }
                
                tit = set.tetrahedra_begin();
                while (tit != set.tetrahedra_end())
                {
                    typename tetrahedron_type::boundary_list t_boundary = lookup_simplex(*tit).get_boundary();
                    typename tetrahedron_type::boundary_iterator it = t_boundary->begin();
                    while (it != t_boundary->end())
                    {
                        set.insert(*it);
                        ++it;
                    }
                    ++tit;
                }
                
                typename simplex_set_type::face_set_iterator fit = input_set.faces_begin();
                
                while (fit != input_set.faces_end())
                {
                    set.insert(*fit);
                    ++fit;
                }
                
                fit = set.faces_begin();
                while (fit != set.faces_end())
                {
                    typename face_type::boundary_list f_boundary = lookup_simplex(*fit).get_boundary();
                    typename face_type::boundary_iterator it = f_boundary->begin();
                    while (it != f_boundary->end())
                    {
                        set.insert(*it);
                        ++it;
                    }
                    ++fit;
                }
                
                typename simplex_set_type::edge_set_iterator eit = input_set.edges_begin();
                
                while (eit != input_set.edges_end())
                {
                    set.insert(*eit);
                    ++eit;
                }
                
                eit = set.edges_begin();
                while (eit != set.edges_end())
                {
                    typename edge_type::boundary_list e_boundary = lookup_simplex(*eit).get_boundary();
                    typename edge_type::boundary_iterator it = e_boundary->begin();
                    while (it != e_boundary->end())
                    {
                        set.insert(*it);
                        ++it;
                    }
                    ++eit;
                }
                
                typename simplex_set_type::node_set_iterator nit = input_set.nodes_begin();
                
                while (nit != input_set.nodes_end())
                {
                    set.insert(*nit);
                    ++nit;
                }
            }
            
            /**
             * Marek
             * Helper function performing a single transposition on simplex's boundary list.
             */
            template<typename key_type>
            void invert_orientation(key_type const & k)
            {
                if (key_type::dim == 0) return;
                
                typedef typename util::simplex_traits<mesh_type, key_type::dim>::simplex_type simplex_type;
                
                typename simplex_type::boundary_list boundary = lookup_simplex(k).get_boundary();
                typename simplex_type::boundary_iterator it = boundary->begin();
                
                ++it;
                
                typename simplex_type::boundary_key_type temp = *it;
                *it = *(boundary->begin());
                *(boundary->begin()) = temp;
            }
            
            /**
             * Marek
             * Helper function for orient_face[...] methods.
             * fk must be a face of sk, dim(sk) = dimension, dim(fk) = dimension-1.
             */
            template<typename key_type_simplex
            , typename key_type_face>
            void orient_face_helper(key_type_simplex const & sk, key_type_face const & fk, bool consistently)
            {
                assert(key_type_simplex::dim == (key_type_face::dim + 1) || !"fk is not a boundary face of sk.");
                assert(key_type_simplex::dim > 1 || !"Cannot induce dimension on vertices.");
                
                typedef typename util::simplex_traits<mesh_type, key_type_simplex::dim>::simplex_type simplex_type;
                typedef typename util::simplex_traits<mesh_type, key_type_face::dim>::simplex_type face_type;
                
                typename simplex_type::boundary_list simplex_boundary = lookup_simplex(sk).get_boundary();
                typename face_type::boundary_list face_boundary = lookup_simplex(fk).get_boundary();
                
                typename simplex_type::boundary_iterator sb_it = simplex_boundary->begin();
                typename face_type::boundary_iterator fb_it = face_boundary->begin();
                
                std::vector<typename face_type::boundary_key_type> new_face_boundary(face_boundary->size());
                
                unsigned char f_index = 0, i = 0;
                
                while (sb_it != simplex_boundary->end())
                {
                    if (*sb_it == fk)
                    {
                        f_index = i+1;
                    }
                    else
                    {
                        typename face_type::boundary_key_type ek;
                        bool res = get_intersection(fk, *sb_it, ek);
                        assert(res || !"Two faces of the same simplex do not intersect?!");
                        new_face_boundary[i] = ek;
                        ++fb_it;
                        ++i;
                    }
                    ++sb_it;
                }
                
                assert(f_index > 0 || !"fk is not a face of sk");
                
                i = 0;
                /**/
                face_boundary->clear();
                for (; i < new_face_boundary.size(); ++i)
                    face_boundary->push_back(new_face_boundary[i]);
                /*fb_it = face_boundary->begin();
                 
                 while (fb_it != face_boundary->end())
                 {
                 *fb_it = new_face_boundary[i];
                 ++i;
                 ++fb_it;
                 }*/
                
                f_index %= 2;
                if ((f_index == 0 && consistently) ||
                    (f_index == 1 && !consistently))
                    invert_orientation(fk);
            }
            
            /**
             * Marek
             * Helper function for orient_coface[...] methods.
             * sk must be a face of cfk, dim(sk) = dimension, dim(cfk) = dimension+1.
             */
            template<typename key_type_simplex
            , typename key_type_coface>
            void orient_coface_helper(key_type_simplex const & sk, key_type_coface const & cfk, bool consistently)
            {
                assert(key_type_simplex::dim == (key_type_coface::dim - 1) || !"sk is not a boundary face of cfk.");
                assert(key_type_simplex::dim < 3 || !"No simplices of dimension more than three.");
                assert(key_type_simplex::dim > 0 || !"Vertices are not oriented.");
                
                typedef typename util::simplex_traits<mesh_type, key_type_simplex::dim>::simplex_type simplex_type;
                
                typedef typename util::simplex_traits<mesh_type, key_type_coface::dim>::simplex_type coface_type;
                typedef typename util::simplex_traits<mesh_type, key_type_simplex::dim-1>::simplex_type face_type;
                
                typename simplex_type::boundary_list simplex_boundary = lookup_simplex(sk).get_boundary();
                typename coface_type::boundary_list coface_boundary = lookup_simplex(cfk).get_boundary();
                
                typename coface_type::boundary_iterator cfb_it = coface_boundary->begin();
                std::map<typename simplex_type::boundary_key_type, key_type_simplex> face_to_simplex;
                
                while (cfb_it != coface_boundary->end())
                {
                    if (*cfb_it != sk)
                    {
                        typename simplex_type::boundary_key_type k;
                        bool res = get_intersection(sk, *cfb_it, k);
                        assert(res || !"Two faces of the same simplex do not intersect?!");
                        face_to_simplex[k] = *cfb_it;
                    }
                    ++cfb_it;
                }
                
                cfb_it = coface_boundary->begin();
                *cfb_it = sk;
                ++cfb_it;
                
                typename simplex_type::boundary_iterator sb_it = simplex_boundary->begin();
                
                while (sb_it != simplex_boundary->end())
                {
                    *cfb_it = face_to_simplex[*sb_it];
                    ++cfb_it;
                    ++sb_it;
                }
                
                if (!consistently) invert_orientation(cfk);
            }
            
            /**
             * Marek
             */
        protected:
            node_key_type split_tetrahedron_helper(tetrahedron_key_type & t,
                                                   std::map<tetrahedron_key_type, tetrahedron_key_type> & new_tets)
            {
                orient_faces_oppositely(t);
                simplex_set_type t_boundary;
                boundary(t, t_boundary);
                
                unsafe_remove(t);
                
                node_key_type n = insert_node();
                lookup_simplex(n).set_compact(true);
                std::map<node_key_type, edge_key_type> node_2_edge_map;
                simplex_set_type::node_set_iterator nit = t_boundary.nodes_begin();
                while (nit != t_boundary.nodes_end())
                {
                    node_2_edge_map[*nit] = unsafe_insert_edge(*nit, n);
                    lookup_simplex(node_2_edge_map[*nit]).set_compact(true);
                    if (nit == t_boundary.nodes_begin())
                    {
                        typename node_type::co_boundary_set n_coboundary = lookup_simplex(n).get_co_boundary();
                        n_coboundary->insert(node_2_edge_map[*nit]);
                    }
                    ++nit;
                }
                std::map<edge_key_type, face_key_type> edge_2_face_map;
                simplex_set_type::edge_set_iterator eit = t_boundary.edges_begin();
                while (eit != t_boundary.edges_end())
                {
                    typename edge_type::boundary_list e_boundary = lookup_simplex(*eit).get_boundary();
                    assert(e_boundary->size() == 2 || !"Edge boundary corrupted");
                    typename edge_type::boundary_iterator ebit = e_boundary->begin();
                    node_key_type n1 = *ebit; ++ebit;
                    node_key_type n2 = *ebit;
                    edge_key_type e1 = node_2_edge_map[n1];
                    edge_key_type e2 = node_2_edge_map[n2];
                    face_key_type f = unsafe_insert_face(*eit, e1, e2);
                    edge_2_face_map[*eit] = f;
                    typename edge_type::co_boundary_set e1_coboundary = lookup_simplex(e1).get_co_boundary();
                    typename edge_type::co_boundary_set e2_coboundary = lookup_simplex(e2).get_co_boundary();
                    if (e1_coboundary->empty()) e1_coboundary->insert(f);
                    if (e2_coboundary->empty()) e2_coboundary->insert(f);
                    ++eit;
                }
                simplex_set_type::face_set_iterator fit = t_boundary.faces_begin();
                while (fit != t_boundary.faces_end())
                {
                    typename face_type::boundary_list f_boundary = lookup_simplex(*fit).get_boundary();
                    assert(f_boundary->size() == 3 || !"Face boundary corrupted");
                    typename face_type::boundary_iterator fbit = f_boundary->begin();
                    edge_key_type e1 = *fbit; ++fbit;
                    edge_key_type e2 = *fbit; ++fbit;
                    edge_key_type e3 = *fbit;
                    face_key_type f1 = edge_2_face_map[e1];
                    face_key_type f2 = edge_2_face_map[e2];
                    face_key_type f3 = edge_2_face_map[e3];
                    tetrahedron_key_type tet = unsafe_insert_tetrahedron(*fit, f1, f2, f3);
                    new_tets[tet] = t;
                    orient_coface_oppositely(*fit, tet);
                    ++fit;
                }
                
                return n;
            }
        public:
            /**
             * Marek
             */
            node_key_type split_face_helper(face_key_type const & f,
                                            std::map<tetrahedron_key_type, tetrahedron_key_type> & new_tets)
            {
                simplex_set_type region, shell;
                star(f, region);
                region.insert(f);
                closure(region, shell);
                shell.difference(region);
                repair_co_boundaries(region, shell);
                
                simplex_set_type::tetrahedron_set_iterator tit = region.tetrahedra_begin();
                std::map<face_key_type, tetrahedron_key_type> face_2_tet_map;
                while (tit != region.tetrahedra_end())
                {
                    orient_faces_oppositely(*tit);
                    typename tetrahedron_type::boundary_list tbnd = find_tetrahedron(*tit).get_boundary();
                    typename tetrahedron_type::boundary_iterator tbit = tbnd->begin();
                    while (tbit != tbnd->end())
                    {
                        if (*tbit != f)
                            face_2_tet_map[*tbit] = *tit;
                        ++tbit;
                    }
                    ++tit;
                }
                
                simplex_set_type region_boundary, f_closure;
                closure(f, f_closure);
                boundary(region, region_boundary);
                
                tit = region.tetrahedra_begin();
                while (tit != region.tetrahedra_end())
                {
                    unsafe_remove(*tit);
                    ++tit;
                }
                unsafe_erase(f);
                
                node_key_type n = insert_node();
                lookup_simplex(n).set_compact(true);
                std::map<node_key_type, edge_key_type> node_2_edge_map;
                simplex_set_type::node_set_iterator nit = region_boundary.nodes_begin();
                while (nit != region_boundary.nodes_end())
                {
                    node_2_edge_map[*nit] = unsafe_insert_edge(*nit, n);
                    lookup_simplex(node_2_edge_map[*nit]).set_compact(true);
                    if (nit == region_boundary.nodes_begin())
                    {
                        typename node_type::co_boundary_set n_coboundary = lookup_simplex(n).get_co_boundary();
                        n_coboundary->insert(node_2_edge_map[*nit]);
                    }
                    ++nit;
                }
                
                std::map<edge_key_type, face_key_type> edge_2_face_map;
                simplex_set_type::edge_set_iterator eit = region_boundary.edges_begin();
                while (eit != region_boundary.edges_end())
                {
                    typename edge_type::boundary_list e_boundary = lookup_simplex(*eit).get_boundary();
                    assert(e_boundary->size() == 2 || !"Edge boundary corrupted");
                    typename edge_type::boundary_iterator ebit = e_boundary->begin();
                    node_key_type n1 = *ebit; ++ebit;
                    node_key_type n2 = *ebit;
                    edge_key_type e1 = node_2_edge_map[n1];
                    edge_key_type e2 = node_2_edge_map[n2];
                    face_key_type f = unsafe_insert_face(*eit, e1, e2);
                    edge_2_face_map[*eit] = f;
                    typename edge_type::co_boundary_set e1_coboundary = lookup_simplex(e1).get_co_boundary();
                    typename edge_type::co_boundary_set e2_coboundary = lookup_simplex(e2).get_co_boundary();
                    if (e1_coboundary->empty()) e1_coboundary->insert(f);
                    if (e2_coboundary->empty()) e2_coboundary->insert(f);
                    ++eit;
                }
                
                simplex_set_type::face_set_iterator fit = region_boundary.faces_begin();
                
                while (fit != region_boundary.faces_end())
                {
                    if (*fit == f)
                    {
                        ++fit; 
                        continue;
                    }
                    typename face_type::boundary_list f_boundary = lookup_simplex(*fit).get_boundary();
                    assert(f_boundary->size() == 3 || !"Face boundary corrupted");
                    typename face_type::boundary_iterator fbit = f_boundary->begin();
                    edge_key_type e1 = *fbit; ++fbit;
                    edge_key_type e2 = *fbit; ++fbit;
                    edge_key_type e3 = *fbit;
                    face_key_type f1 = edge_2_face_map[e1];
                    face_key_type f2 = edge_2_face_map[e2];
                    face_key_type f3 = edge_2_face_map[e3];
                    tetrahedron_key_type tet = unsafe_insert_tetrahedron(*fit, f1, f2, f3);
                    new_tets[tet] = face_2_tet_map[*fit];
                    orient_coface_oppositely(*fit, tet);
                    ++fit;
                }
                
                simplex_set_type::edge_set_iterator ceit = f_closure.edges_begin();
                while (ceit != f_closure.edges_end())
                {
                    typename edge_type::co_boundary_set e_co_boundary = lookup_simplex(*ceit).get_co_boundary();
                    typename edge_type::co_boundary_iterator f_pos = e_co_boundary->find(f);
                    if (f_pos != e_co_boundary->end())
                    {
                        e_co_boundary->erase(f_pos);
                        e_co_boundary->insert(edge_2_face_map[*ceit]);
                    }
                    ++ceit;
                }
                
                return n;
            }
            
        public:
            /**
             * Marek
             */
            node_key_type split_edge_helper(const edge_key_type & edge,
                                            std::map<tetrahedron_key_type, tetrahedron_key_type> & new_tets)
            {
                edge_key_type e = edge;
                
                simplex_set_type st_e;
                star(e, st_e);
                st_e.insert(e);
                simplex_set_type shell;
                closure(st_e, shell);
                shell.difference(st_e);
                
                repair_co_boundaries(st_e, shell);
                
                std::map<face_key_type, tetrahedron_key_type> face_2_tet_map;
                simplex_set_type::tetrahedron_set_iterator tit = st_e.tetrahedra_begin();
                while (tit != st_e.tetrahedra_end())
                {
                    typename tetrahedron_type::boundary_list tbnd = find_tetrahedron(*tit).get_boundary();
                    typename tetrahedron_type::boundary_iterator tbit = tbnd->begin();
                    while (tbit != tbnd->end())
                    {
                        face_2_tet_map[*tbit] = *tit;
                        ++tbit;
                    }
                    ++tit;
                }
                
                std::map<edge_key_type, face_key_type> old_edge_2_face_map;
                std::map<edge_key_type, bool> non_link_edge;
                simplex_set_type::face_set_iterator fit = st_e.faces_begin();
                while (fit != st_e.faces_end())
                {
                    typename face_type::boundary_list f_boundary = lookup_simplex(*fit).get_boundary();
                    typename face_type::boundary_iterator fbit = f_boundary->begin();
                    while (fbit != f_boundary->end())
                    {
                        if (*fbit != e)
                        {
                            old_edge_2_face_map[*fbit] = *fit;
                            non_link_edge[*fbit] = true;
                        }
                        ++fbit;
                    }
                    orient_faces_oppositely(*fit);
                    ++fit;
                }
                
                tit = st_e.tetrahedra_begin();
                while (tit != st_e.tetrahedra_end())
                {
                    orient_faces_oppositely(*tit);
                    ++tit;
                }
                
                typename edge_type::boundary_list e_boundary = lookup_simplex(e).get_boundary();
                typename edge_type::boundary_iterator ebit = e_boundary->begin();
                node_key_type n1 = *ebit;
                ++ebit;
                node_key_type n2 = *ebit;
                
                node_key_type n = insert_node();
                find_node(n).set_compact(false);
                std::map<node_key_type, edge_key_type> node_2_edge_map;
                typename node_type::co_boundary_set n_coboundary = lookup_simplex(n).get_co_boundary();
                
                edge_key_type e1 = unsafe_insert_edge(n, n1);
                find_edge(e1).set_compact(false);
                node_2_edge_map[n1] = e1;
                n_coboundary->insert(e1);
                
                edge_key_type e2 = unsafe_insert_edge(n2, n);
                find_edge(e2).set_compact(false);
                node_2_edge_map[n2] = e2;
                n_coboundary->insert(e2);
                
                simplex_set_type::node_set_iterator nit = shell.nodes_begin();
                
                while (nit != shell.nodes_end())
                {
                    if ((*nit != n1) && (*nit != n2))
                    {
                        edge_key_type new_edge = unsafe_insert_edge(*nit, n);
                        find_edge(new_edge).set_compact(false);
                        node_2_edge_map[*nit] = new_edge;
                        n_coboundary->insert(new_edge);
                    }
                    ++nit;
                }
                
                std::map<edge_key_type, face_key_type> edge_2_face_map;
                simplex_set_type::edge_set_iterator eit = shell.edges_begin();
                while (eit != shell.edges_end())
                {
                    typename edge_type::boundary_list e_boundary = lookup_simplex(*eit).get_boundary();
                    assert(e_boundary->size() == 2 || !"Edge boundary corrupted");
                    typename edge_type::boundary_iterator ebit = e_boundary->begin();
                    node_key_type n1 = *ebit; ++ebit;
                    node_key_type n2 = *ebit;
                    edge_key_type e1 = node_2_edge_map[n1];
                    edge_key_type e2 = node_2_edge_map[n2];
                    face_key_type f = unsafe_insert_face(*eit, e1, e2);
                    edge_2_face_map[*eit] = f;
                    typename edge_type::co_boundary_set e1_coboundary = lookup_simplex(e1).get_co_boundary();
                    typename edge_type::co_boundary_set e2_coboundary = lookup_simplex(e2).get_co_boundary();
                    e1_coboundary->insert(f);
                    e2_coboundary->insert(f);
                    typename edge_type::co_boundary_set e_coboundary = lookup_simplex(*eit).get_co_boundary();
                    //assert (*(e_coboundary->begin()) != old_edge_2_face_map[*eit]);
                    if (non_link_edge[*eit] &&
                        (e_coboundary->find(old_edge_2_face_map[*eit]) != e_coboundary->end()))
                    {
                        e_coboundary->erase(old_edge_2_face_map[*eit]);
                        e_coboundary->insert(f);
                    }
                    orient_coface_oppositely(*eit, f);
                    ++eit;
                }
                
                simplex_set_type::face_set_iterator sfit = shell.faces_begin();
                
                while (sfit != shell.faces_end())
                {
                    typename face_type::boundary_list f_boundary = lookup_simplex(*sfit).get_boundary();
                    assert(f_boundary->size() == 3 || !"Face boundary corrupted");
                    typename face_type::boundary_iterator fbit = f_boundary->begin();
                    edge_key_type e1 = *fbit; ++fbit;
                    edge_key_type e2 = *fbit; ++fbit;
                    edge_key_type e3 = *fbit;
                    face_key_type f1 = edge_2_face_map[e1];
                    face_key_type f2 = edge_2_face_map[e2];
                    face_key_type f3 = edge_2_face_map[e3];
                    tetrahedron_key_type tet = unsafe_insert_tetrahedron(*sfit, f1, f2, f3);
                    new_tets[tet] = face_2_tet_map[*sfit];
                    orient_coface_oppositely(*sfit, tet);
                    ++sfit;
                }
                
                /******* unsafe_remove(e); *******/
                tit = st_e.tetrahedra_begin();
                while (tit != st_e.tetrahedra_end())
                {
                    unsafe_remove(*tit);
                    ++tit;
                }
                fit = st_e.faces_begin();
                while (fit != st_e.faces_end())
                {
                    unsafe_erase(*fit);
                    ++fit;
                }
                unsafe_erase(e);
                /**********************************/
                
                simplex_set_type starn;
                star(n, starn);
                starn.insert(n);
                compress(starn);
                
                return n;
            }
            
        private:
            /**
             * Marek
             */
            void multi_face_remove_helper(simplex_set_type & removed_faces,
                                          simplex_set_type & new_simplices)
            {
                simplex_set_type region, region_boundary, region_int, mf_boundary;
                star(removed_faces, region);
                closure(region, region_int);
                boundary(region, region_boundary);
                region_int.difference(region_boundary);
                
                repair_co_boundaries(region_int, region_boundary);
                
                std::vector<node_key_type> vv(2);
                mf_remove_get_apices(*(removed_faces.faces_begin()), vv);
                
                std::vector<std::map<node_key_type, edge_key_type> > node_2_edge_map(2);
                std::vector<std::map<edge_key_type, face_key_type> > edge_2_face_map(2);
                
                mf_remove_clear_interior(removed_faces, region_boundary, region_int, mf_boundary, vv, node_2_edge_map, edge_2_face_map);
                
                edge_key_type e = unsafe_insert_edge(vv[0], vv[1]);
                mf_remove_fill_hole(mf_boundary, e, node_2_edge_map, edge_2_face_map);
                
                simplex_set_type st_e;
                star(e, st_e);
                new_simplices.insert(e);
                new_simplices.add(st_e);
            }
            
            /**
             * Marek
             */
            void mf_remove_clear_interior(simplex_set_type & removed_faces,
                                          simplex_set_type & region_boundary,
                                          simplex_set_type & region_int,
                                          simplex_set_type & mf_boundary,
                                          std::vector<node_key_type> & vv,
                                          std::vector<std::map<node_key_type, edge_key_type> > & node_2_edge_map,
                                          std::vector<std::map<edge_key_type, face_key_type> > & edge_2_face_map)
            {
                simplex_set_type::tetrahedron_set_iterator tit = region_int.tetrahedra_begin();
                while (tit != region_int.tetrahedra_end())
                {
                    orient_faces_oppositely(*tit);
                    ++tit;
                }
                
                boundary_helper2<face_key_type, edge_key_type>(removed_faces, mf_boundary);
                region_boundary.difference(mf_boundary);
                
                unsafe_remove(region_int);
                
                simplex_set_type::edge_set_iterator eit = region_boundary.edges_begin();
                while (eit != region_boundary.edges_end())
                {
                    typename edge_type::boundary_list e_boundary = lookup_simplex(*eit).get_boundary();
                    typename edge_type::boundary_iterator ebit = e_boundary->begin();
                    int j = -1;
                    node_key_type n;
                    while (ebit != e_boundary->end())
                    {
                        if (*ebit == vv[0]) j = 0;
                        else if (*ebit == vv[1]) j = 1;
                        else n = *ebit;
                        ++ebit;
                    }
                    node_2_edge_map[j][n] = *eit;
                    ++eit;
                }
                
                simplex_set_type::face_set_iterator fit = region_boundary.faces_begin();
                while (fit != region_boundary.faces_end())
                {
                    typename face_type::boundary_list f_boundary = lookup_simplex(*fit).get_boundary();
                    typename face_type::boundary_iterator fbit = f_boundary->begin();
                    edge_key_type mf_boundary_edge;
                    std::vector<edge_key_type> apex_edges(2);
                    int i = 0;
                    while (fbit != f_boundary->end())
                    {
                        if (mf_boundary.contains(*fbit)) mf_boundary_edge = *fbit;
                        else
                        {
                            apex_edges[i] = *fbit;
                            ++i;
                        }
                        ++fbit;
                    }
                    typename edge_type::boundary_list e_boundary = lookup_simplex(mf_boundary_edge).get_boundary();
                    edge_key_type e = node_2_edge_map[0][*(e_boundary->begin())];
                    if (e == apex_edges[0] || e == apex_edges[1])
                        edge_2_face_map[0][mf_boundary_edge] = *fit;
                    else
                        edge_2_face_map[1][mf_boundary_edge] = *fit;
                    ++fit;
                }
            }
            
            /**
             * Marek
             */
            void mf_remove_fill_hole(simplex_set_type & mf_boundary,
                                     edge_key_type & e,
                                     std::vector<std::map<node_key_type, edge_key_type> > & node_2_edge_map,
                                     std::vector<std::map<edge_key_type, face_key_type> > & edge_2_face_map)
            {
                std::map<node_key_type, face_key_type> new_faces; 
                
                simplex_set_type::node_set_iterator nit = mf_boundary.nodes_begin();
                while (nit != mf_boundary.nodes_end())
                {
                    new_faces[*nit] = unsafe_insert_face(e, node_2_edge_map[0][*nit], node_2_edge_map[1][*nit]);
                    if (nit == mf_boundary.nodes_begin())
                    {
                        lookup_simplex(e).get_co_boundary()->insert(new_faces[*nit]);
                    }
                    ++nit;
                }
                
                simplex_set_type::edge_set_iterator mfb_eit = mf_boundary.edges_begin();
                
                while (mfb_eit != mf_boundary.edges_end())
                {
                    typename edge_type::boundary_list e_boundary = lookup_simplex(*mfb_eit).get_boundary();
                    typename edge_type::boundary_iterator ebit = e_boundary->begin();
                    node_key_type n1 = *ebit;    ++ebit;
                    node_key_type n2 = *ebit;
                    tetrahedron_key_type t = unsafe_insert_tetrahedron(edge_2_face_map[0][*mfb_eit], edge_2_face_map[1][*mfb_eit],
                                                                       new_faces[n1], new_faces[n2]);
                    orient_coface_oppositely(edge_2_face_map[0][*mfb_eit], t);
                    ++mfb_eit;
                }
            }
            
            /**
             * Marek
             */
            void mf_remove_get_apices(face_key_type const & f, std::vector<node_key_type> & vv)
            {
                simplex_set_type st_f;
                simplex_set_type bnd_f;
                star(f, st_f);
                boundary(f, bnd_f);
                simplex_set_type::tetrahedron_set_iterator tit = st_f.tetrahedra_begin();
                int i = 0;
                while (tit != st_f.tetrahedra_end())
                {
                    simplex_set_type bnd_t;
                    boundary(*tit, bnd_t);
                    bnd_t.difference(bnd_f);
                    vv[i] = *(bnd_t.nodes_begin());
                    ++i;
                    ++tit;
                }
            }
            
            /**
             * Marek
             */
            void repair_co_boundaries(simplex_set_type & interior, simplex_set_type & boundary)
            {
                std::map<node_key_type, char> node_repaired;
                std::map<edge_key_type, char> edge_repaired;
                simplex_set_type::face_set_iterator fit = boundary.faces_begin();
                while (fit != boundary.faces_end())
                {
                    typename face_type::boundary_list f_boundary = lookup_simplex(*fit).get_boundary();
                    typename face_type::boundary_iterator fbit = f_boundary->begin();
                    while (fbit != f_boundary->end())
                    {
                        if (edge_repaired[*fbit] == 0)
                        {
                            typename edge_type::co_boundary_set e_co_boundary = lookup_simplex(*fbit).get_co_boundary();
                            typename edge_type::co_boundary_iterator ecit = e_co_boundary->begin();
                            while (ecit != e_co_boundary->end())
                            {
                                if (interior.contains(*ecit))
                                {
                                    e_co_boundary->insert(*fit);
                                    e_co_boundary->erase(*ecit);
                                    break;
                                }
                                ++ecit;
                            }
                            edge_repaired[*fbit] = 1;
                        }
                        ++fbit;
                    }
                    ++fit;
                }
                
                simplex_set_type::edge_set_iterator eit = boundary.edges_begin();
                while (eit != boundary.edges_end())
                {
                    typename edge_type::boundary_list e_boundary = lookup_simplex(*eit).get_boundary();
                    typename edge_type::boundary_iterator ebit = e_boundary->begin();
                    while (ebit != e_boundary->end())
                    {
                        if (node_repaired[*ebit] == 0)
                        {
                            typename node_type::co_boundary_set n_co_boundary = lookup_simplex(*ebit).get_co_boundary();
                            typename node_type::co_boundary_iterator ncit = n_co_boundary->begin();
                            while (ncit != n_co_boundary->end())
                            {
                                if (interior.contains(*ncit))
                                {
                                    n_co_boundary->insert(*eit);
                                    n_co_boundary->erase(*ncit);
                                    break;
                                }
                                ++ncit;
                            }
                            node_repaired[*ebit] = 1;
                        }
                        ++ebit;
                    }
                    ++eit;
                }
            }
            
            /**
             * Marek
             */
            void remove_edge_helper(edge_key_type const & removed_edge, 
                                    std::vector<node_key_type> & new_edges_desc,
                                    simplex_set_type & new_simplices)
            {
                simplex_set_type region, region_boundary, region_int, re_link;
                link(removed_edge, re_link, region);
                region.insert(removed_edge);
                closure(region, region_boundary);
                region_boundary.difference(region);
                region_int.add(region);
                
                repair_co_boundaries(region_int, region_boundary);
                
                std::vector<node_key_type> vv(2);
                typename edge_type::boundary_list reb = lookup_simplex(removed_edge).get_boundary();
                typename edge_type::boundary_iterator rebit = reb->begin();
                vv[0] = *rebit;   
                ++rebit;  
                vv[1] = *rebit;
                
                region_boundary.difference(re_link);
                
                std::vector<std::map<node_key_type, edge_key_type> > node_2_edge_map(2);
                std::vector<std::map<edge_key_type, face_key_type> > edge_2_face_map(2);
                
                std::map<node_key_type, simplex_set_type> node_2_star_map;
                simplex_set_type::node_set_iterator nit = re_link.nodes_begin();
                while (nit != re_link.nodes_end())
                {
                    simplex_set_type set;
                    star(*nit, set);
                    node_2_star_map[*nit] = set;
                    ++nit;
                }
                
                remove_edge_clear_interior(region_boundary, region_int, re_link, vv, node_2_edge_map, edge_2_face_map);
                
                if (re_link.size_nodes() != re_link.size_edges()) remove_edge_close_link(re_link, new_edges_desc);
                
                remove_edge_fill_hole(region_boundary, re_link, new_edges_desc, node_2_edge_map, edge_2_face_map, node_2_star_map, new_simplices);
                
                simplex_set_type new_faces;
                simplex_set_type::face_set_iterator fit = re_link.faces_begin();
                while (fit != re_link.faces_end())
                {
                    new_faces.insert(*fit);
                    ++fit;
                }
                
                remove_edge_orient_new_tetrahedra(region_boundary, new_simplices);
            }
            
            /**
             * Marek
             */
            void remove_edge_close_link(simplex_set_type & re_link, std::vector<node_key_type> & new_edges_desc)
            {
                std::map<node_key_type, unsigned char> node_count;
                
                simplex_set_type::edge_set_iterator eit = re_link.edges_begin();
                
                while (eit != re_link.edges_end())
                {
                    typename edge_type::boundary_list e_boundary = lookup_simplex(*eit).get_boundary();
                    typename edge_type::boundary_iterator ebit = e_boundary->begin();
                    while (ebit != e_boundary->end())
                    {
                        ++node_count[*ebit];
                        ++ebit;
                    }
                    ++eit;
                }
                
                std::vector<node_key_type> vv(2);
                int i = 0;
                
                std::map<node_key_type, unsigned char>::iterator mit = node_count.begin();
                
                while (mit != node_count.end())
                {
                    if (mit->second < 2)
                    {
                        vv[i] = mit->first;
                        ++i;
                    }
                    ++mit;
                }
                assert (i == 2 || !"edge can't be removed!");
                
                new_edges_desc.push_back(vv[0]);
                new_edges_desc.push_back(vv[1]);
            }
            
            /**
             * Marek
             */
            void remove_edge_clear_interior(simplex_set_type & region_boundary,
                                            simplex_set_type & region_int,
                                            simplex_set_type & re_link,
                                            std::vector<node_key_type> & vv,
                                            std::vector<std::map<node_key_type, edge_key_type> > & node_2_edge_map,
                                            std::vector<std::map<edge_key_type, face_key_type> > & edge_2_face_map)
            {
                simplex_set_type::tetrahedron_set_iterator tit = region_int.tetrahedra_begin();
                while (tit != region_int.tetrahedra_end())
                {
                    orient_faces_oppositely(*tit);
                    ++tit;
                }
                
                unsafe_remove(region_int);
                
                simplex_set_type::edge_set_iterator eit = region_boundary.edges_begin();
                while (eit != region_boundary.edges_end())
                {
                    typename edge_type::boundary_list e_boundary = lookup_simplex(*eit).get_boundary();
                    typename edge_type::boundary_iterator ebit = e_boundary->begin();
                    int j = -1;
                    node_key_type n;
                    while (ebit != e_boundary->end())
                    {
                        if (*ebit == vv[0])
                            j = 0;
                        else if (*ebit == vv[1])
                            j = 1;
                        else
                            n = *ebit;
                        ++ebit;
                    }
                    node_2_edge_map[j][n] = *eit;
                    ++eit;
                }
                
                simplex_set_type::face_set_iterator fit = region_boundary.faces_begin();
                while (fit != region_boundary.faces_end())
                {
                    typename face_type::boundary_list f_boundary = lookup_simplex(*fit).get_boundary();
                    typename face_type::boundary_iterator fbit = f_boundary->begin();
                    edge_key_type link_edge;
                    std::vector<edge_key_type> apex_edges(2);
                    int i = 0;
                    while (fbit != f_boundary->end())
                    {
                        if (re_link.contains(*fbit)) link_edge = *fbit;
                        else
                        {
                            apex_edges[i] = *fbit;
                            ++i;
                        }
                        ++fbit;
                    }
                    typename edge_type::boundary_list e_boundary = lookup_simplex(link_edge).get_boundary();
                    edge_key_type e = node_2_edge_map[0][*(e_boundary->begin())];
                    if (e == apex_edges[0] || e == apex_edges[1])
                        edge_2_face_map[0][link_edge] = *fit;
                    else
                        edge_2_face_map[1][link_edge] = *fit;
                    ++fit;
                }
            }
            
            /**
             * Marek
             */
            void remove_edge_fill_hole(simplex_set_type & region_boundary,
                                       simplex_set_type & re_link,
                                       std::vector<node_key_type> & new_edges_desc,
                                       std::vector<std::map<node_key_type, edge_key_type> > & node_2_edge_map,
                                       std::vector<std::map<edge_key_type, face_key_type> > & edge_2_face_map,
                                       std::map<node_key_type, simplex_set_type> & node_2_star_map,
                                       simplex_set_type & new_simplices)
            {
                unsigned int no_edges = static_cast<unsigned int>(new_edges_desc.size()) / 2;
                assert (new_edges_desc.size() % 2 == 0 || !"new_edges_desc contains an odd number of node keys");
                
                std::vector<edge_key_type> new_edges(no_edges);
                std::map<node_key_type, simplex_set_type> node_2_new_edges;
                
                for (unsigned int i = 0; i < no_edges; ++i)
                {
                    node_key_type v0 = new_edges_desc[2*i];
                    node_key_type v1 = new_edges_desc[2*i+1];
                    edge_key_type e = unsafe_insert_edge(v0, v1);
                    node_2_new_edges[v0].insert(e);
                    node_2_new_edges[v1].insert(e);
                    re_link.insert(e);
                    new_simplices.insert(e);
                    new_edges[i] = e;
                    std::vector<face_key_type> faces(node_2_edge_map.size());
                    for (unsigned int k = 0; k < node_2_edge_map.size(); ++k)
                    {
                        faces[k] = unsafe_insert_face(e, node_2_edge_map[k][v0], node_2_edge_map[k][v1]);
                        new_simplices.insert(faces[k]);
                        edge_2_face_map[k][e] = faces[k];
                    }
                    lookup_simplex(e).get_co_boundary()->insert(faces[0]);
                }
                
                if (no_edges == 0) //2-3 flip
                {
                    simplex_set_type::edge_set_iterator link_eit = re_link.edges_begin();
                    std::vector<edge_key_type> link_edges(3);
                    for (int i = 0; i < 3; ++i)
                    {
                        link_edges[i] = *link_eit;
                        ++link_eit;
                    }
                    assert (link_eit == re_link.edges_end() || !"Removed edge's link is not a triangle, but no triangulation provided.");
                    face_key_type f = unsafe_insert_face(link_edges[0], link_edges[1], link_edges[2]);
                    new_simplices.insert(f);
                    re_link.insert(f);
                }
                else
                {
                    std::map<edge_key_type, bool> used_edges;
                    
                    for (unsigned int i = 0; i < no_edges; ++i)
                    {
                        edge_key_type e = new_edges[i];   
                        used_edges[e] = true;
                        typename edge_type::boundary_list e_boundary = lookup_simplex(e).get_boundary();
                        typename edge_type::boundary_iterator ebit = e_boundary->begin();
                        node_key_type n1, n2;
                        n1 = *ebit;     ++ebit;     n2 = *ebit;
                        simplex_set_type st1, st2;
                        st1.add(node_2_star_map[n1]);
                        st2.add(node_2_star_map[n2]);
                        st1.intersection(re_link);
                        st2.intersection(re_link);
                        //
                        st1.add(node_2_new_edges[n1]);
                        st2.add(node_2_new_edges[n2]);
                        simplex_set_type::edge_set_iterator e1_it = st1.edges_begin();
                        while (e1_it != st1.edges_end())
                        {
                            if ((*e1_it != e) && !used_edges[*e1_it])
                            {
                                simplex_set_type::edge_set_iterator e2_it = st2.edges_begin();
                                while (e2_it != st2.edges_end())
                                {
                                    node_key_type n;
                                    if ((*e2_it != e) && (get_intersection(*e1_it, *e2_it, n)) && !used_edges[*e2_it])
                                    {
                                        face_key_type f = unsafe_insert_face(e, *e1_it, *e2_it);
                                        new_simplices.insert(f);
                                        re_link.insert(f);
                                    }
                                    ++e2_it;
                                }
                            }
                            ++e1_it;
                        }
                    }
                }
                
                simplex_set_type::face_set_iterator lf_it = re_link.faces_begin();
                while (lf_it != re_link.faces_end())
                {
                    face_key_type f = *lf_it;
                    typename face_type::boundary_list f_boundary = lookup_simplex(f).get_boundary();
                    typename face_type::boundary_iterator fbit = f_boundary->begin();
                    std::vector<std::vector<face_key_type> > adj(edge_2_face_map.size());
                    for (char i = 0; i < 3; ++i)
                    {
                        for (unsigned int k = 0; k < adj.size(); ++k)
                            adj[k].push_back(edge_2_face_map[k][*fbit]);
                        ++fbit;
                    }
                    for (unsigned int k = 0; k < adj.size(); ++k)
                    {
                        tetrahedron_key_type t = unsafe_insert_tetrahedron(adj[k][0], adj[k][1], adj[k][2], f);
                        new_simplices.insert(t);
                    }
                    ++lf_it;
                }
            }
            
            /**
             * Marek
             */
            void remove_edge_orient_new_tetrahedra(simplex_set_type & region_boundary,
                                                   simplex_set_type & new_tetrahedra)
            {
                std::map<face_key_type, bool> face_oriented;
                
                simplex_set_type::face_set_iterator fit = region_boundary.faces_begin();
                while (fit != region_boundary.faces_end())
                {
                    face_oriented[*fit] = true;
                    ++fit;
                }
                
                simplex_set_type tets;
                while (new_tetrahedra.size_tetrahedra() > 0)
                {
                    simplex_set_type corrected_tets;
                    simplex_set_type::tetrahedron_set_iterator tit = new_tetrahedra.tetrahedra_begin();
                    
                    while (tit != new_tetrahedra.tetrahedra_end())
                    {
                        typename tetrahedron_type::boundary_list t_boundary = lookup_simplex(*tit).get_boundary();
                        typename tetrahedron_type::boundary_iterator tbit = t_boundary->begin();
                        while (tbit != t_boundary->end())
                        {
                            if (face_oriented[*tbit])
                            {
                                orient_coface_oppositely(*tbit, *tit);
                                orient_faces_consistently(*tit);
                                corrected_tets.insert(*tit);
                                break;
                            }
                            ++tbit;
                        }
                        
                        if (tbit != t_boundary->end())
                        {
                            tbit = t_boundary->begin();
                            while (tbit != t_boundary->end())
                            {
                                face_oriented[*tbit] = true;
                                ++tbit;
                            }
                        }
                        
                        ++tit;
                    }
                    
                    new_tetrahedra.difference(corrected_tets);
                    tets.add(corrected_tets);
                }
                
                new_tetrahedra.add(tets);
            }
            
            /**
             * Marek
             */
            void multi_face_retriangulation_helper(simplex_set_type & removed_faces, 
                                                   std::vector<node_key_type> & new_edges_desc,
                                                   simplex_set_type & new_faces,
                                                   simplex_set_type & new_simplices)
            {
                simplex_set_type region, region_boundary, region_int, mf_boundary;
                star(removed_faces, region);
                closure(region, region_int);
                boundary(region, region_boundary);
                region_int.difference(region_boundary);
                
                std::vector<node_key_type> vv;
                mfrt_get_apices(*(removed_faces.faces_begin()), vv);
                
                simplex_set_type multi_face;
                closure(removed_faces, multi_face);
                boundary_2manifold(removed_faces, mf_boundary);
                multi_face.difference(mf_boundary);
                region_int.add(multi_face);
                region_boundary.difference(multi_face);
                
                repair_co_boundaries(region_int, region_boundary);
                
                std::vector<std::map<node_key_type, edge_key_type> > node_2_edge_map(vv.size());
                std::vector<std::map<edge_key_type, face_key_type> > edge_2_face_map(vv.size());
                edge_key_type e;
                
                std::map<node_key_type, simplex_set_type> node_2_star_map;
                simplex_set_type::node_set_iterator nit = region_boundary.nodes_begin();
                while (nit != region_boundary.nodes_end())
                {
                    bool b = true;
                    for (unsigned int i = 0; i < vv.size(); ++i) b = b && (*nit != vv[i]);
                    if (b)
                    {
                        simplex_set_type set;
                        star(*nit, set);
                        node_2_star_map[*nit] = set;
                    }
                    ++nit;
                }
                
                region_boundary.difference(mf_boundary);
                
                mfrt_clear_interior(removed_faces, region_boundary, region_int, mf_boundary, vv, node_2_edge_map, edge_2_face_map);
                
                remove_edge_fill_hole(region_boundary, mf_boundary, new_edges_desc, node_2_edge_map, edge_2_face_map, node_2_star_map, new_simplices);
                
                simplex_set_type::face_set_iterator fit = mf_boundary.faces_begin();
                while (fit != mf_boundary.faces_end())
                {
                    new_faces.insert(*fit);
                    ++fit;
                }
                
                remove_edge_orient_new_tetrahedra(region_boundary, new_simplices);
            }
            
            /**
             * Marek
             */
            void mfrt_get_apices(face_key_type const & f, std::vector<node_key_type> & vv)
            {
                simplex_set_type st_f;
                simplex_set_type bnd_f;
                star(f, st_f);
                boundary(f, bnd_f);
                simplex_set_type::tetrahedron_set_iterator tit = st_f.tetrahedra_begin();
                while (tit != st_f.tetrahedra_end())
                {
                    simplex_set_type bnd_t;
                    boundary(*tit, bnd_t);
                    bnd_t.difference(bnd_f);
                    vv.push_back(*(bnd_t.nodes_begin()));
                    ++tit;
                }
            }
            
            /**
             * Marek
             */
            void mfrt_clear_interior(simplex_set_type & removed_faces,
                                     simplex_set_type & region_boundary,
                                     simplex_set_type & region_int,
                                     simplex_set_type & mf_boundary,
                                     std::vector<node_key_type> & vv,
                                     std::vector<std::map<node_key_type, edge_key_type> > & node_2_edge_map,
                                     std::vector<std::map<edge_key_type, face_key_type> > & edge_2_face_map)
            {
                simplex_set_type::tetrahedron_set_iterator tit = region_int.tetrahedra_begin();
                while (tit != region_int.tetrahedra_end())
                {
                    orient_faces_oppositely(*tit);
                    ++tit;
                }
                
                unsafe_remove(region_int);
                
                simplex_set_type::edge_set_iterator eit = region_boundary.edges_begin();
                while (eit != region_boundary.edges_end())
                {
                    typename edge_type::boundary_list e_boundary = lookup_simplex(*eit).get_boundary();
                    typename edge_type::boundary_iterator ebit = e_boundary->begin();
                    int j = -1;
                    node_key_type n;
                    while (ebit != e_boundary->end())
                    {
                        if (*ebit == vv[0]) j = 0;
                        else if ((vv.size() > 1) && (*ebit == vv[1])) j = 1;
                        else n = *ebit;
                        ++ebit;
                    }
                    node_2_edge_map[j][n] = *eit;
                    ++eit;
                }
                
                simplex_set_type::face_set_iterator fit = region_boundary.faces_begin();
                while (fit != region_boundary.faces_end())
                {
                    typename face_type::boundary_list f_boundary = lookup_simplex(*fit).get_boundary();
                    typename face_type::boundary_iterator fbit = f_boundary->begin();
                    edge_key_type mf_boundary_edge;
                    std::vector<edge_key_type> apex_edges(2);
                    int i = 0;
                    while (fbit != f_boundary->end())
                    {
                        if (mf_boundary.contains(*fbit)) mf_boundary_edge = *fbit;
                        else
                        {
                            apex_edges[i] = *fbit;
                            ++i;
                        }
                        ++fbit;
                    }
                    
                    typename edge_type::boundary_list e_boundary = lookup_simplex(mf_boundary_edge).get_boundary();
                    edge_key_type e = node_2_edge_map[0][*(e_boundary->begin())];
                    
                    if (e == apex_edges[0] || e == apex_edges[1])
                        edge_2_face_map[0][mf_boundary_edge] = *fit;
                    else
                        edge_2_face_map[1][mf_boundary_edge] = *fit;
                    
                    ++fit;
                }
            }
            
            /**
             * Marek
             * so far for the manifold edge ONLY!
             */
            bool edge_collapse_precond(edge_key_type & e,
                                       node_key_type const & n1,
                                       node_key_type const & n2)
            {
                simplex_set_type lk_e, lk1, lk12;
                link(e, lk_e);
                link(n1, lk1);
                link(n2, lk12);
                lk12.intersection(lk1);
                lk12.difference(lk_e);
                if (lk12.size_nodes() == 0 &&
                    lk12.size_edges() == 0 &&
                    lk12.size_faces() == 0)
                    return true;
                return false;
            }
            
        public:
            /**
             * Marek
             * so far for the manifold edge ONLY!
             */
            node_key_type edge_collapse_helper(edge_key_type & e,
                                               node_key_type const & n1,
                                               node_key_type const & n2)
            {
                if (!edge_collapse_precond(e,n1,n2))
                    return -1;
                
                simplex_set_type st2, lk1, lk2, st_e, lk_e, st_e_boundary;
                star(n2, st2);
                link(n1, lk1);
                link(n2, lk2);
                star(e, st_e);
                st_e.insert(e);
                link(e, lk_e);
                boundary(st_e, st_e_boundary);
                st_e_boundary.difference(st_e);
                
                repair_co_boundaries(st_e, st_e_boundary);
                
                std::map<edge_key_type, edge_key_type> edge_2_edge_map;
                std::map<face_key_type, face_key_type> face_2_face_map;
                
                edge_collapse_clear_interior(n1, n2, e, st_e, st_e_boundary, lk1, lk2, edge_2_edge_map, face_2_face_map);
                edge_collapse_sew_hole_up(n1, n2, st2, st_e, lk1, edge_2_edge_map, face_2_face_map);
                
                return n1;
            }
            
        private:
            /**
             * Marek
             */
            void edge_collapse_clear_interior(node_key_type const & n1,
                                              node_key_type const & n2,
                                              edge_key_type & e,
                                              simplex_set_type & st_e,
                                              simplex_set_type & st_e_boundary,
                                              simplex_set_type & lk1,
                                              simplex_set_type & lk2,
                                              std::map<edge_key_type, edge_key_type> & edge_2_edge_map,
                                              std::map<face_key_type, face_key_type> & face_2_face_map)
            {
                simplex_set_type::tetrahedron_set_iterator tit = st_e.tetrahedra_begin();
                while (tit != st_e.tetrahedra_end())
                {
                    face_key_type f1, f2;
                    typename tetrahedron_type::boundary_list t_boundary = lookup_simplex(*tit).get_boundary();
                    typename tetrahedron_type::boundary_iterator tbit = t_boundary->begin();
                    while (tbit != t_boundary->end())
                    {
                        if (!st_e.contains(*tbit))
                        {
                            if (lk1.contains(*tbit))
                            {
                                f2 = *tbit;
                            }
                            if (lk2.contains(*tbit))
                            {
                                f1 = *tbit;
                            }
                        }
                        ++tbit;
                    }
                    face_2_face_map[f2] = f1;
                    unsafe_remove(*tit);
                    ++tit;
                }
                
                simplex_set_type::face_set_iterator fit = st_e.faces_begin();
                while (fit != st_e.faces_end())
                {
                    edge_key_type e1, e2;
                    typename face_type::boundary_list f_boundary = lookup_simplex(*fit).get_boundary();
                    typename face_type::boundary_iterator fbit = f_boundary->begin();
                    while (fbit != f_boundary->end())
                    {
                        if (*fbit != e)
                        {
                            if (lk1.contains(*fbit))
                            {
                                e2 = *fbit;
                            }
                            if (lk2.contains(*fbit))
                            {
                                e1 = *fbit;
                            }
                        }
                        ++fbit;
                    }
                    edge_2_edge_map[e2] = e1;
                    unsafe_erase(*fit);
                    ++fit;
                }
                
                unsafe_erase(e);
            }
            
            /**
             * Marek
             */
            void edge_collapse_sew_hole_up(node_key_type const & n1,
                                           node_key_type const & n2,
                                           simplex_set_type & st2,
                                           simplex_set_type & st_e,
                                           simplex_set_type & lk1,
                                           std::map<edge_key_type, edge_key_type> & edge_2_edge_map,
                                           std::map<face_key_type, face_key_type> & face_2_face_map)
            {
                simplex_set_type to_be_removed;
                std::map<edge_key_type, edge_key_type>::iterator meeit = edge_2_edge_map.begin();
                while (meeit != edge_2_edge_map.end())
                {
                    to_be_removed.insert(meeit->first);
                    ++meeit;
                }
                std::map<face_key_type, face_key_type>::iterator mffit = face_2_face_map.begin();
                while (mffit != face_2_face_map.end())
                {
                    to_be_removed.insert(mffit->first);
                    ++mffit;
                }
                
                simplex_set_type::edge_set_iterator eit = st2.edges_begin();
                while (eit != st2.edges_end())
                {
                    if ((!st_e.contains(*eit)) && (!to_be_removed.contains(*eit)))
                    {
                        typename edge_type::boundary_list e_boundary = lookup_simplex(*eit).get_boundary();
                        typename edge_type::boundary_iterator ebit = e_boundary->begin();
                        while (ebit != e_boundary->end())
                        {
                            if (*ebit == n2)
                            {
                                *ebit = n1;
                                break;
                            }
                            ++ebit;
                        }       
                    }
                    ++eit;
                }
                
                simplex_set_type::face_set_iterator fit = st2.faces_begin();
                while (fit != st2.faces_end())
                {
                    if ((!st_e.contains(*fit)) && (!to_be_removed.contains(*fit)))
                    {
                        typename face_type::boundary_list f_boundary = lookup_simplex(*fit).get_boundary();
                        typename face_type::boundary_iterator fbit = f_boundary->begin();
                        while (fbit != f_boundary->end())
                        {
                            if (to_be_removed.contains(*fbit))
                            {
                                *fbit = edge_2_edge_map[*fbit];
                                //break;
                            }
                            ++fbit;
                        }
                    }
                    ++fit;
                }
                
                simplex_set_type::tetrahedron_set_iterator tit = st2.tetrahedra_begin();
                while (tit != st2.tetrahedra_end())
                {
                    if (!st_e.contains(*tit))
                    {
                        typename tetrahedron_type::boundary_list t_boundary = lookup_simplex(*tit).get_boundary();
                        typename tetrahedron_type::boundary_iterator tbit = t_boundary->begin();
                        while (tbit != t_boundary->end())
                        {
                            if (to_be_removed.contains(*tbit))
                            {
                                face_key_type f = face_2_face_map[*tbit];
                                *tbit = f;
                                lookup_simplex(f).get_co_boundary()->insert(*tit);
                                //break;
                            }
                            ++tbit;
                        }
                    }
                    ++tit;
                }
                
                std::map<face_key_type, face_key_type>::iterator ffit = face_2_face_map.begin();
                while (ffit != face_2_face_map.end())
                {
                    typename face_type::boundary_list f_boundary = lookup_simplex(ffit->first).get_boundary();
                    typename face_type::boundary_iterator fbit = f_boundary->begin();
                    while (fbit != f_boundary->end())
                    {
                        typename edge_type::co_boundary_set e_coboundary = lookup_simplex(*fbit).get_co_boundary();
                        assert (e_coboundary->size() == 1);
                        typename edge_type::co_boundary_iterator ecit = e_coboundary->begin();
                        typename edge_type::co_boundary_set new_coboundary = new typename edge_type::set_type;
                        while (ecit != e_coboundary->end())
                        {
                            if (*ecit == ffit->first)
                            {
                                new_coboundary->insert(ffit->second);
                                //*ecit = ffit->second;
                                //break;
                            }
                            else
                            {
                                new_coboundary->insert(*ecit);
                            }
                            ++ecit;
                        }
                        lookup_simplex(*fbit).get_co_boundary()->clear();
                        typename edge_type::co_boundary_iterator ncbit = new_coboundary->begin();
                        while (ncbit != new_coboundary->end())
                        {
                            lookup_simplex(*fbit).get_co_boundary()->insert(*ncbit);
                            ++ncbit;
                        }
                        delete new_coboundary;
                        ++fbit;
                    }
                    unsafe_erase(ffit->first);
                    ++ffit;
                }
                
                std::map<edge_key_type, edge_key_type>::iterator eeit = edge_2_edge_map.begin();
                while (eeit != edge_2_edge_map.end())
                {
                    typename edge_type::boundary_list e_boundary = lookup_simplex(eeit->first).get_boundary();
                    typename edge_type::boundary_iterator ebit = e_boundary->begin();
                    while (ebit != e_boundary->end())
                    {
                        typename node_type::co_boundary_set n_coboundary = lookup_simplex(*ebit).get_co_boundary();
                        assert (n_coboundary->size() == 1);
                        typename node_type::co_boundary_iterator ncit = n_coboundary->begin();
                        typename node_type::co_boundary_set new_coboundary = new typename node_type::set_type;
                        while (ncit != n_coboundary->end())
                        {
                            if (*ncit == eeit->first)
                            {
                                new_coboundary->insert(eeit->second);
                                //*ncit = eeit->second;
                                //break;
                            }
                            else
                                new_coboundary->insert(*ncit);
                            ++ncit;
                        }
                        find_node(*ebit).get_co_boundary()->clear();
                        typename node_type::co_boundary_iterator ncbit = new_coboundary->begin();
                        while (ncbit != new_coboundary->end())
                        {
                            find_node(*ebit).get_co_boundary()->insert(*ncbit);
                            ++ncbit;
                        }
                        delete new_coboundary;
                        ++ebit;
                    }
                    unsafe_erase(eeit->first);
                    ++eeit;
                }
                
                unsafe_erase(n2);
            }
            
            /**
             *
             */
            template<typename simplex_type>
            void reset_label(simplex_type & s)
            {
                s.reset_label();
                typename simplex_type::co_boundary_set co_bound = s.get_co_boundary();
                typename simplex_type::co_boundary_iterator co_bound_it = co_bound->begin();
                
                for( ; co_bound_it != co_bound->end() ; ++co_bound_it )
                {
                    typedef typename util::simplex_traits<mesh_type, simplex_type::dim+1>::simplex_type  co_boundary_type;
                    
                    co_boundary_type& simplex = lookup_simplex(*co_bound_it);
                    if (simplex.get_label() != 0) reset_co_label(simplex);
                }
            }
            
            /**
             *
             */
            template<typename simplex_type>
            void reset_co_label(simplex_type & s)
            {
                s.reset_label();
                typename simplex_type::boundary_list bound = s.get_boundary();
                typename simplex_type::boundary_iterator bound_it = bound->begin();
                
                for( ; bound_it != bound->end() ; ++bound_it )
                {
                    typedef typename util::simplex_traits<mesh_type, simplex_type::dim-1>::simplex_type  boundary_type;
                    
                    boundary_type& simplex = lookup_simplex(*bound_it);
                    if (simplex.get_label() != 0) reset_label(simplex);
                }
            }
            
            /**
             * Returns whether a face (f) is in the boundary of a simplex (s).
             */
            template<typename key_type_face, typename key_type_simplex>
            bool in_boundary(key_type_face const & f, key_type_simplex const & s)
            {
                if (key_type_face::dim >= key_type_simplex::dim)
                {
                    //cannot be in boundary as dimensions are wrong
                    return false;
                }
                //for convinience
                typedef typename util::simplex_traits<mesh_type, key_type_simplex::dim>::simplex_type simplex_type;
                typename simplex_type::boundary_list b_list = lookup_simplex(s).get_boundary();
                if (key_type_face::dim+1 < key_type_simplex::dim)
                {
                    //too far apart.. need to call recursively
                    bool in_bound = false;
                    typename simplex_type::boundary_iterator bound_it = b_list->begin();
                    ++bound_it;
                    for( ; bound_it != b_list->end() ; ++bound_it)
                    {
                        in_bound = in_bound || in_boundary(f, *(bound_it));
                        if (in_bound) break;
                    }
                    return in_bound;
                }
                //this is the right dimensions.. search the boundary list
                typename simplex_type::boundary_iterator bound_it = b_list->begin();
                for( ; bound_it != b_list->end() ; ++bound_it)
                {
                    if (f == *bound_it) return true;
                }
                return false;
            }
            
            template<typename key_type_face>
            bool in_boundary(key_type_face const & f, node_key_type const & s)
            { //no simplex is in the boundary of a node...
                return false;
            }
            
            /**
             * Helper for the star operation. Searches up in the simplex hieracy.
             * Assumes that mesh is compressed. Should work in a non-compressed state too.
             * Template should traverse nodes and edges.
             *
             * @param s The original simplex that we are calulating the star of
             * @param t The current simplex that we are searching through
             * @param set The result is returned here.
             */
            template<typename simplex_key_s, typename simplex_key_t>
            void star_helper(simplex_key_s const & s, simplex_key_t const & t,  simplex_set_type & set)
            {
                assert(simplex_key_s::dim < simplex_key_t::dim || !"Star traversed a wrong dimension simplex");
                
                typedef typename util::simplex_traits<mesh_type, simplex_key_t::dim>::simplex_type simplex_type;
                
                simplex_type& simplex = lookup_simplex(t);
                set.insert(t); //add ourself to the set
                simplex.set_label(1);
                
                //iterate up - dim lower than 3
                star_helper_recurse_up(s, t, set);
                //iterate down through the structure in a recursive manner
                star_helper_recurse_down(s, t, set);
            }
            
            /**
             *
             */
            template<typename simplex_key_s>
            void star_helper_recurse_up(simplex_key_s const & s, tetrahedron_key_type const & t, simplex_set_type & set) {}
            
            /**
             *
             */
            template<typename simplex_key_s, typename simplex_key_t>
            void star_helper_recurse_up(simplex_key_s const & s, simplex_key_t const & t, simplex_set_type & set)
            {
                typedef typename util::simplex_traits<mesh_type, simplex_key_t::dim>::simplex_type simplex_type;
                typename simplex_type::co_boundary_set co_boundary = lookup_simplex(t).get_co_boundary();
                typename simplex_type::co_boundary_iterator co_b_it = co_boundary->begin();
                for( ; co_b_it != co_boundary->end() ; ++co_b_it )
                {
                    //only recurse if it has not been visited previously - no need to check boundary relations as a co-face
                    //of t will have s on it's boundary if t has it on it's boundary - and it does
                    if(lookup_simplex(*co_b_it).get_label() == 0)
                    {
                        //recurse through children
                        star_helper(s, *co_b_it, set);
                    }
                }
            }
            
            
            template<typename N, typename M>
            void star_helper_recurse_down(N const &, M const &, simplex_set_type &)
            { }
            
            
            /**
             *
             */
            void star_helper_recurse_down(edge_key_type const & s,
                                          tetrahedron_key_type const & t,
                                          simplex_set_type & set)
            {
                star_helper_recurse_down_(s, t, set);
            }
            
            /**
             *
             */
            void star_helper_recurse_down(node_key_type const & s,
                                          tetrahedron_key_type const & t,
                                          simplex_set_type & set)
            {
                star_helper_recurse_down_(s, t, set);
            }
            
            /**
             *
             */
            void star_helper_recurse_down(node_key_type const & s,
                                          face_key_type const & t,
                                          simplex_set_type & set)
            {
                star_helper_recurse_down_(s, t, set);
            }
            
            /**
             *
             */
            template<typename simplex_key_s, typename simplex_key_t>
            void star_helper_recurse_down_(simplex_key_s const & s, simplex_key_t const & t,  simplex_set_type & set)
            {
                typedef typename util::simplex_traits<mesh_type, simplex_key_t::dim>::simplex_type simplex_type;
                typename simplex_type::boundary_list boundary = lookup_simplex(t).get_boundary();
                typename simplex_type::boundary_iterator b_it = boundary->begin();
                for( ; b_it != boundary->end() ; ++b_it )
                {
                    //only recurse if the simplex is a co-face* of s and if it has not been visited previously
                    if(lookup_simplex(*b_it).get_label() == 0 && in_boundary(s, *b_it))
                    {
                        //recurse through children
                        star_helper(s, *b_it, set);
                    }
                }
            }
            
            /**
             * Inserts an edge into the mesh without updating any of the boundary nodes co-boundary relation.
             * Might leave the mesh in an inconsistent state.
             */
            edge_key_type unsafe_insert_edge(node_key_type node1, node_key_type node2)
            {
                edge_iterator edge = m_edge_kernel->create();
                //edge->set_compact(true);
                //the nodes might be uncompressed
                edge->add_face(node1);
                edge->add_face(node2);
                return edge.key();
            }
            
            /**
             * Inserts a face into the mesh without updating any of the boundary edges co-boundary relation
             * Might leave the mesh in an inconsistent state.
             */
            face_key_type unsafe_insert_face(edge_key_type edge1, edge_key_type edge2, edge_key_type edge3)
            {
                face_iterator face = m_face_kernel->create();
                face->add_face(edge1);
                face->add_face(edge2);
                face->add_face(edge3);
                return face.key();
            }
            
            /**
             * Inserts a tetrahedron into the mesh. 
             * Updates boundary relation both ways, but don't uncompresses anything.
             * Might leave the mesh in an inconsistent state.
             */
            tetrahedron_key_type unsafe_insert_tetrahedron(face_key_type face1, face_key_type face2, face_key_type face3, face_key_type face4)
            {
                tetrahedron_iterator tetrahedron = m_tetrahedron_kernel->create();
                m_face_kernel->find(face1).add_co_face(tetrahedron.key());
                m_face_kernel->find(face2).add_co_face(tetrahedron.key());
                m_face_kernel->find(face3).add_co_face(tetrahedron.key());
                m_face_kernel->find(face4).add_co_face(tetrahedron.key());
                tetrahedron->add_face(face1);
                tetrahedron->add_face(face2);
                tetrahedron->add_face(face3);
                tetrahedron->add_face(face4);
                return tetrahedron.key();
            }
            
            /**
             *
             */
            void unsafe_erase(tetrahedron_key_type const & key)
            {
                m_tetrahedron_kernel->erase(key);
            }
            
            /**
             *
             */
            void unsafe_erase(face_key_type const & key)
            {
                m_face_kernel->erase(key);
            }
            
            /**
             *
             */
            void unsafe_erase(edge_key_type const & key)
            {
                m_edge_kernel->erase(key);
            }
            
            /**
             *
             */
            void unsafe_erase(node_key_type const & key)
            {
                m_node_kernel->erase(key);
            }

			/**
			 *
			 */
            node_key_type vertex_insertion_helper(simplex_set_type & removed_tets,
                                                  simplex_set_type & new_simplices)
            {
                simplex_set_type region_boundary, region_int;
                closure(removed_tets, region_int);
				boundary(removed_tets, region_boundary);
				region_int.difference(region_boundary);

				repair_co_boundaries(region_int, region_boundary);

				vertex_insertion_clear_interior(region_boundary, region_int);

				node_key_type n = insert_node();

				vertex_insertion_fill_hole(region_boundary, n, new_simplices);

				return n;
			}

			/**
			 *
			 */
			void vertex_insertion_clear_interior(simplex_set_type & region_boundary,
												 simplex_set_type & region_int)
			{
				unsafe_remove(region_int);
			}

			/**
			 *
			 */
			void vertex_insertion_fill_hole(simplex_set_type & region_boundary,
											node_key_type & n,
											simplex_set_type & new_simplices)
			{
				std::map<node_key_type, edge_key_type> node_2_edge_map;
				std::map<edge_key_type, face_key_type> edge_2_face_map;
				simplex_set_type::node_set_iterator nit = region_boundary.nodes_begin();

				new_simplices.insert(n);

				while (nit != region_boundary.nodes_end())
				{
					node_2_edge_map[*nit] = unsafe_insert_edge(n, *nit);
					if (nit == region_boundary.nodes_begin())
					lookup_simplex(n).get_co_boundary()->insert(node_2_edge_map[*nit]);
					new_simplices.insert(node_2_edge_map[*nit]);
					++nit;
				}

				std::map<edge_key_type, bool> edge_processed;
				simplex_set_type::edge_set_iterator eit = region_boundary.edges_begin();
				while (eit != region_boundary.edges_end())
				{
					typename edge_type::boundary_list e_boundary = lookup_simplex(*eit).get_boundary();
					typename edge_type::boundary_iterator ebit = e_boundary->begin();
					node_key_type n1 = *ebit;	  ++ebit;	
					node_key_type n2 = *ebit;
					edge_2_face_map[*eit] = unsafe_insert_face(*eit, node_2_edge_map[n1], node_2_edge_map[n2]);
					if (!edge_processed[node_2_edge_map[n1]])
					{
						lookup_simplex(node_2_edge_map[n1]).get_co_boundary()->insert(edge_2_face_map[*eit]);
						edge_processed[node_2_edge_map[n1]] = true;
					}
					if (!edge_processed[node_2_edge_map[n2]])
					{
						lookup_simplex(node_2_edge_map[n2]).get_co_boundary()->insert(edge_2_face_map[*eit]);
						edge_processed[node_2_edge_map[n2]] = true;
					}
					new_simplices.insert(edge_2_face_map[*eit]);
					++eit;
				}

				simplex_set_type::face_set_iterator fit = region_boundary.faces_begin();
				while (fit != region_boundary.faces_end())
				{
					typename face_type::boundary_list f_boundary = lookup_simplex(*fit).get_boundary();
					typename face_type::boundary_iterator fbit = f_boundary->begin();
					edge_key_type e1 = *fbit;   ++fbit;
					edge_key_type e2 = *fbit;	  ++fbit;
					edge_key_type e3 = *fbit;
					tetrahedron_key_type t = unsafe_insert_tetrahedron(edge_2_face_map[e1], edge_2_face_map[e2], edge_2_face_map[e3], *fit);
					new_simplices.insert(t);
					++fit;
				}
			}
         
		public:
            /**
             *
             */
            void unsafe_remove(tetrahedron_key_type const & key)
            {
                tetrahedron_type& tet = lookup_simplex(key);
                typename tetrahedron_type::boundary_iterator itr = tet.get_boundary()->begin(); 
                for( ; itr != tet.get_boundary()->end(); ++itr)
                {
                    lookup_simplex(*itr).remove_co_face(key);
                }
                m_tetrahedron_kernel->erase(key);
            }
            
		private:
            /**
             *
             */
            void unsafe_remove(face_key_type & key)
            {
                face_type& face = lookup_simplex(key);
                //first remove all simplices in co-boundary
                typename face_type::co_boundary_iterator c_itr = face.get_co_boundary()->begin();
                while( c_itr != face.get_co_boundary()->end() )
                {
                    tetrahedron_key_type tmp = *c_itr; //copy the value because the iterate will be invalidated during the operation.
                    unsafe_remove(tmp); 
                    c_itr = face.get_co_boundary()->begin(); //reset iterator as the co-boundary has changed.
                }
                //don't touch boundary relations.. they should be fixed elsewhere...
                //finally remove the actual face
                m_face_kernel->erase(key);
            }
            
            /**
             *
             */
            void unsafe_remove(edge_key_type & key)
            {
                edge_type& edge = lookup_simplex(key);
                //first uncompress the edge
                uncompress(key);
                //first remove all simplices in co-boundary
                typename edge_type::co_boundary_iterator c_itr = edge.get_co_boundary()->begin();
                for( ; c_itr != edge.get_co_boundary()->end() ; ++c_itr)
                {
                    unsafe_remove(*c_itr);
                }
                //don't touch boundary relations.. they should be fixed elsewhere...
                //finally remove the actual face
                m_edge_kernel->erase(key);
            }
            
            /**
             *
             */
            void unsafe_remove(node_key_type & key)
            {
                node_type& node = lookup_simplex(key);
                //first uncompress the node
                uncompress(key);
                //first remove all simplices in co-boundary
                typename node_type::co_boundary_iterator c_itr = node.get_co_boundary()->begin();
                for( ; c_itr != node.get_co_boundary()->end() ; ++c_itr)
                {
                    unsafe_remove(*c_itr); 
                }
                //finally remove the actual node
                m_node_kernel->erase(key);
            }
            
            /**
             * Marek
             */
            void unsafe_remove(simplex_set_type & set)
            { 
                simplex_set_type::tetrahedron_set_iterator tit = set.tetrahedra_begin();
                while (tit != set.tetrahedra_end())
                {
                    unsafe_remove(*tit);
                    ++tit;
                }
                
                simplex_set_type::face_set_iterator fit = set.faces_begin();
                while (fit != set.faces_end())
                {
                    unsafe_erase(*fit);
                    ++fit;
                }
                
                simplex_set_type::edge_set_iterator eit = set.edges_begin();
                while (eit != set.edges_end())
                {
                    unsafe_erase(*eit);
                    ++eit;
                }
                
                simplex_set_type::node_set_iterator nit = set.nodes_begin();
                while (nit != set.nodes_end())
                {
                    unsafe_erase(*nit);
                    ++nit;
                }
            }
            
        public:
            
            /**
             *
             */
            t4mesh() {
                m_node_kernel = new node_kernel_type();
                m_edge_kernel = new edge_kernel_type();
                m_face_kernel = new face_kernel_type();
                m_tetrahedron_kernel = new tetrahedron_kernel_type();
                m_uncompressed = 0;
            }
            
            /**
             *
             */
            ~t4mesh()
            {
                delete m_tetrahedron_kernel;
                delete m_face_kernel;
                delete m_edge_kernel;
                delete m_node_kernel;
            }
            
            /**
             *
             */
            void clear()
            {
                m_node_kernel->clear();
                m_edge_kernel->clear();
                m_face_kernel->clear();
                m_tetrahedron_kernel->clear();
            }
            
            /**
             *
             */
            node_type & find_node(const node_key_type k) { return m_node_kernel->find(k); }
            
            /**
             *
             */
            edge_type & find_edge(const edge_key_type k) { return m_edge_kernel->find(k); }
            
            /**
             *
             */
            face_type & find_face(const face_key_type k) { return m_face_kernel->find(k); }
            
            /**
             *
             */
            tetrahedron_type & find_tetrahedron(const tetrahedron_key_type k) { return m_tetrahedron_kernel->find(k); }
            
            /**
             *
             */
            typename node_kernel_type::iterator nodes_begin() { return m_node_kernel->begin(); }
            
            /**
             *
             */
            typename node_kernel_type::iterator nodes_end() { return m_node_kernel->end(); } 
            
            /**
             *
             */
            typename edge_kernel_type::iterator edges_begin() { return m_edge_kernel->begin(); }
            
            /**
             *
             */
            typename edge_kernel_type::iterator edges_end() { return m_edge_kernel->end(); } 
            
            /**
             *
             */
            typename face_kernel_type::iterator faces_begin() { return m_face_kernel->begin(); }
            
            /**
             *
             */
            typename face_kernel_type::iterator faces_end() { return m_face_kernel->end(); } 
            
            /**
             *
             */
            typename tetrahedron_kernel_type::iterator tetrahedra_begin() { return m_tetrahedron_kernel->begin(); }
            
            /**
             *
             */
            typename tetrahedron_kernel_type::iterator tetrahedra_end() { return m_tetrahedron_kernel->end(); } 
            
            /**
             * Inserts a node into the mesh. Trivial.
             */
            node_key_type insert_node(bool is_compact =false)
            {
                node_iterator node = m_node_kernel->create();
                node->set_compact(is_compact);
                return node.key();
            }
            
            /**
             * Inserts an edge into the mesh. Updates the co-boundary of the boundary nodes with the newly created edge.
             * Leaves the closure of the edge in an uncompressed state.
             */
            edge_key_type insert_edge(node_key_type node1, node_key_type node2, bool is_compact =false)
            {
                edge_iterator edge = m_edge_kernel->create();
                //first uncompress boundary
                uncompress(node1);
                uncompress(node2);
                //add the new simplex to the co-boundary relation of the boundary simplices
                m_node_kernel->find(node1).add_co_face(edge.key());
                m_node_kernel->find(node2).add_co_face(edge.key());
                //set the boundary relation        
                edge->add_face(node1);
                edge->add_face(node2);
                edge->set_compact(is_compact);
                return edge.key();
            }
            
            /**
             * Inserts a face into the mesh. Updates the co-boundary of the boundary faces with the newly created face.
             * Leaves the closure of the face in an uncompressed state.
             */
            face_key_type insert_face(edge_key_type edge1, edge_key_type edge2, edge_key_type edge3)
            {
                face_iterator face = m_face_kernel->create();
                //first uncompress full boundary - fast if allready uncompressed, else might be heavy
                simplex_set_type set;
                closure(edge1, set);
                closure(edge2, set);
                closure(edge3, set);
                uncompress(set);
                //update relations
                m_edge_kernel->find(edge1).add_co_face(face.key());
                m_edge_kernel->find(edge2).add_co_face(face.key());
                m_edge_kernel->find(edge3).add_co_face(face.key());
                face->add_face(edge1);
                face->add_face(edge2);
                face->add_face(edge3);
                return face.key();
            }
            
            /**
             * Inserts a tetrahedron into the mesh. Updates the co-boundary of the boundary edges with the newly created tetrahedron.
             * Leaves the closure of the tetrahedron in an uncompressed state.
             */
            tetrahedron_key_type insert_tetrahedron(face_key_type face1, face_key_type face2, face_key_type face3, face_key_type face4)
            {
                tetrahedron_iterator tetrahedron = m_tetrahedron_kernel->create();
                //first uncompress full boundary - fast if allready uncompressed, else might be heavy
                simplex_set_type set;
                closure(face1, set);
                closure(face2, set);
                closure(face3, set);
                closure(face4, set);
                uncompress(set);
                //update relations
                m_face_kernel->find(face1).add_co_face(tetrahedron.key());
                m_face_kernel->find(face2).add_co_face(tetrahedron.key());
                m_face_kernel->find(face3).add_co_face(tetrahedron.key());
                m_face_kernel->find(face4).add_co_face(tetrahedron.key());
                tetrahedron->add_face(face1);
                tetrahedron->add_face(face2);
                tetrahedron->add_face(face3);
                tetrahedron->add_face(face4);
                return tetrahedron.key();
            }
            
            void remove(tetrahedron_key_type const & key)
            {
                //first uncompress any affected simplices
                simplex_set_type cls;
                closure(key, cls);
                uncompress(cls);
                //now remove the simplex
                tetrahedron_type& tet = lookup_simplex(key);
                typename tetrahedron_type::boundary_iterator itr = tet.get_boundary()->begin(); 
                for( ; itr != tet.get_boundary()->end(); ++itr)
                {
                    lookup_simplex(*itr).remove_co_face(key);
                }
                m_tetrahedron_kernel->erase(key);
                //compress
                compress_all();
            }
            
            void remove(face_key_type const & key)
            {
                //first uncompress any affected simplices
                simplex_set_type cls;
                closure(key, cls);
                uncompress(cls);
                //now remove the simplex
                face_type& face = lookup_simplex(key);
                //first remove all simplices in co-boundary
                typename face_type::co_boundary_iterator c_itr = face.get_co_boundary()->begin();
                while( c_itr != face.get_co_boundary()->end() )
                {
                    tetrahedron_key_type tmp = *c_itr; //copy so that the value is stored on stack
                    remove(tmp);
                    c_itr = face.get_co_boundary()->begin(); //co-boundary has changed
                }
                //before we remove the face, we should uncompress boundary because ramification might change
                simplex_set_type set;
                closure(key, set);
                uncompress(set);
                //now remove relation in boundary
                typename face_type::boundary_iterator b_itr = face.get_boundary()->begin();
                for( ; b_itr != face.get_boundary()->end(); ++b_itr)
                {
                    lookup_simplex(*b_itr).remove_co_face(key);
                }
                //finally remove the actual face
                m_face_kernel->erase(key);
                //compress
                compress_all();
            }
            
            void remove(edge_key_type & key)
            {
                //first uncompress any affected simplices
                simplex_set_type cls;
                closure(key, cls);
                uncompress(cls);
                //now remove the simplex
                edge_type& edge = lookup_simplex(key);
                //first remove all simplices in co-boundary
                typename edge_type::co_boundary_iterator c_itr = edge.get_co_boundary()->begin();
                while( c_itr != edge.get_co_boundary()->end() )
                {
                    face_key_type tmp = *c_itr; //copy so that the value is stored on stack
                    remove(tmp);
                    c_itr = edge.get_co_boundary()->begin(); //co-boundary has changed
                }
                //before we remove the edge, we should uncompress boundary because ramification might change
                //now remove relation in boundary
                typename edge_type::boundary_iterator b_itr = edge.get_boundary()->begin();
                for( ; b_itr != edge.get_boundary()->end(); ++b_itr)
                {
                    uncompress(*b_itr);
                    lookup_simplex(*b_itr).remove_co_face(key);
                }
                //finally remove the actual face
                m_edge_kernel->erase(key);
                //compress
                compress_all();
            }
            
            void remove(node_key_type & key)
            {
                //first uncompress any affected simplices
                simplex_set_type cls;
                closure(key, cls);
                uncompress(cls);
                //now remove the simplex
                node_type& node = lookup_simplex(key);
                //first remove all simplices in co-boundary
                typename node_type::co_boundary_iterator c_itr = node.get_co_boundary()->begin();
                while( c_itr != node.get_co_boundary()->end() )
                {
                    edge_key_type tmp = *c_itr; //copy so that the value is stored on stack
                    remove(tmp);
                    c_itr = node.get_co_boundary()->begin(); //co-boundary has changed
                }
                //finally remove the actual face
                m_face_kernel->erase(key);
                //compress
                compress_all();
            }
            
            size_type size_nodes() { return m_node_kernel->size(); }
            size_type size_edges() { return m_edge_kernel->size(); }
            size_type size_faces() { return m_face_kernel->size(); }
            size_type size_tetrahedra() { return m_tetrahedron_kernel->size(); }
            size_type size() { return size_nodes() + size_edges() + size_faces() + size_tetrahedra(); }
            
            /**
             * Makes sure that the mesh is in compressed format, according to the
             * principles behind the Incidence Simplicial data structure, as described
             * by Hui and de Floriani.
             */
            void compress_all()
            {
                //call compress with a new simplex set containing the entire mesh
                //only compress if more than 10% is uncompressed. Only nodes and edges can be uncompressed
                if ( (m_uncompressed * 5) > (size_nodes() + size_edges())  )
                {
                    std::cout << "Compressing all..." << std::endl;
                    simplex_set_type set;
                    for (node_iterator iter = m_node_kernel->begin(); iter != m_node_kernel->end(); ++iter)
                        set.insert(iter.key());
                    for (edge_iterator iter = m_edge_kernel->begin(); iter != m_edge_kernel->end(); ++iter)
                        set.insert(iter.key());
                    for (face_iterator iter = m_face_kernel->begin(); iter != m_face_kernel->end(); ++iter)
                        set.insert(iter.key());
                    for (tetrahedron_iterator iter = m_tetrahedron_kernel->begin(); iter != m_tetrahedron_kernel->end(); ++iter)
                        set.insert(iter.key());
                    compress(set);
                }
            }
            
            /**
             * Returns the restricted star of a simplex.
             */
            void star(node_key_type const & key, simplex_set_type & s_set)
            {
                typedef typename util::simplex_traits<mesh_type, 0>::simplex_type simplex_type;
                simplex_type& simplex = lookup_simplex(key);
                typename simplex_type::co_boundary_set co_boundary = simplex.get_co_boundary();
                typename simplex_type::co_boundary_iterator co_bit = co_boundary->begin();
                for( ; co_bit != co_boundary->end() ; ++co_bit )
                {
                    star_helper(key, *co_bit, s_set);
                }
                //now that we are done, remember to reset all labels
                //reset_label(simplex);
                //reset for each level - exept tets - so we get the entire component reset
                for (simplex_set_type::edge_set_iterator eit = s_set.edges_begin(); eit != s_set.edges_end(); ++eit) 
                    reset_label(lookup_simplex(*eit));
                for (simplex_set_type::face_set_iterator fit = s_set.faces_begin(); fit != s_set.faces_end(); ++fit) 
                    reset_label(lookup_simplex(*fit));
                for (simplex_set_type::tetrahedron_set_iterator tit = s_set.tetrahedra_begin(); tit != s_set.tetrahedra_end(); ++tit) 
                    lookup_simplex(*tit).reset_label();
            } 
            
            
            void star(edge_key_type const & key, simplex_set_type & s_set)
            {
                typedef typename util::simplex_traits<mesh_type, 1>::simplex_type simplex_type;
                simplex_type& simplex = lookup_simplex(key);
                typename simplex_type::co_boundary_set co_boundary = simplex.get_co_boundary();
                typename simplex_type::co_boundary_iterator co_bit = co_boundary->begin();
                for( ; co_bit != co_boundary->end() ; ++co_bit )
                {
                    star_helper(key, *co_bit, s_set);
                }
                //now that we are done, remember to reset all labels
                //reset_label(simplex);
                //reset for each level - exept tets - so we get the entire component reset
                for (simplex_set_type::face_set_iterator fit = s_set.faces_begin(); fit != s_set.faces_end(); ++fit) 
                    reset_label(lookup_simplex(*fit));
                for (simplex_set_type::tetrahedron_set_iterator tit = s_set.tetrahedra_begin(); tit != s_set.tetrahedra_end(); ++tit) 
                    lookup_simplex(*tit).reset_label();
            }
            
            /*  template<>
             void star<edge_key_type>(edge_key_type const & e, simplex_set_type & s_set)
             {
             edge_type& simplex = lookup_simplex(e);
             std::list<face_key_type> face_stack;
             edge_type::co_boundary_set co_boundary = simplex.get_co_boundary();
             edge_type::co_boundary_iterator co_bit = co_boundary->begin();
             for( ; co_bit != co_boundary->end() ; ++co_bit )
             {
             face_stack.push_back(*co_bit);
             }
             while (!face_stack.empty())
             {
             face_key_type fk = face_stack.back();
             face_stack.pop_back();
             face_type & f = lookup_simplex(fk);
             f.set_label(1);
             s_set.insert(fk);
             face_type::co_boundary_iterator fcit = f.get_co_boundary()->begin();
             while (fcit != f.get_co_boundary()->end())
             {
             tetrahedron_type & t = lookup_simplex(*fcit);
             if (t.get_label() == 0)
             {
             t.set_label(1);
             s_set.insert(*fcit);
             tetrahedron_type::boundary_iterator tbit = t.get_boundary()->begin();
             while (tbit != t.get_boundary()->end())
             {
             if ((lookup_simplex(*tbit).get_label() == 0) && (in_boundary(e, *tbit)))
             {
             face_stack.push_back(*tbit);
             }
             ++tbit;
             }
             }
             ++fcit;
             }
             }
             
             for (simplex_set_type::face_set_iterator fit = s_set.faces_begin(); fit != s_set.faces_end(); ++fit) 
             reset_label(lookup_simplex(*fit));
             for (simplex_set_type::tetrahedron_set_iterator tit = s_set.tetrahedra_begin(); tit != s_set.tetrahedra_end(); ++tit) 
             lookup_simplex(*tit).reset_label();
             }*/
            
            void star(face_key_type const & f, simplex_set_type & s_set)
            {
                face_type& simplex = lookup_simplex(f);
                typename face_type::co_boundary_set co_boundary = simplex.get_co_boundary();
                typename face_type::co_boundary_iterator co_bit = co_boundary->begin();
                for( ; co_bit != co_boundary->end() ; ++co_bit )
                {
                    s_set.insert(*co_bit);
                }
            }
            
            void star(tetrahedron_key_type const &, simplex_set_type &)
            { /* do nothing */ }
            
            /**
             * Marek
             */
            void star(simplex_set_type & set, simplex_set_type & result_set)
            {
                simplex_set_type::node_set_iterator nit = set.nodes_begin();
                while (nit != set.nodes_end())
                {
                    simplex_set_type st_n;
                    star(*nit, st_n);
                    result_set.add(st_n);
                    ++nit;
                }
                
                simplex_set_type::edge_set_iterator eit = set.edges_begin();
                while (eit != set.edges_end())
                {
                    if (!result_set.contains(*eit))
                    {
                        simplex_set_type st_e;
                        star(*eit, st_e);
                        result_set.add(st_e);
                    }
                    ++eit;
                }
                
                simplex_set_type::face_set_iterator fit = set.faces_begin();
                while (fit != set.faces_end())
                {
                    if (!result_set.contains(*fit))
                    {
                        simplex_set_type st_f;
                        star(*fit, st_f);
                        result_set.add(st_f);
                    }
                    ++fit;
                }
                
                simplex_set_type::tetrahedron_set_iterator tit = set.tetrahedra_begin();
                while (tit != set.tetrahedra_end())
                {
                    result_set.insert(*tit);
                    ++tit;
                }
            }
            
            /**
             * Initial compress - might need to be refactored, a lot...
             * Only compresses a node that is non-compact.
             */
            void compress(simplex_set_type & s)
            {
                //by definition tetrahedra and faces are already "compressed".. so only compress nodes and edges.
                //we handle nodes first to not destroy the full coboudary of edges while we handle nodes
                typename simplex_set_type::node_set_iterator node_itr = s.nodes_begin();
                for( ; node_itr != s.nodes_end() ; ++node_itr)
                {
                    //first we need to label all edges for each connected component in the star of the node
                    node_type& node = m_node_kernel->find(*node_itr);
                    if (node.is_compact()) continue;
                    node.set_compact(true);
                    typename node_type::co_boundary_set cob_set = node.get_co_boundary();
                    std::vector<edge_key_type> edge_vec(cob_set->size());
                    std::copy(cob_set->begin(), cob_set->end(), edge_vec.begin());
                    typename std::vector<edge_key_type>::iterator edge_itr = edge_vec.begin();
                    //co_boundary_iterator edge_itr = cob_set->begin();
                    int label = 1;
                    for( ;edge_itr != edge_vec.end() ; ++edge_itr)
                    {
                        //find an iterator to the actual edge
                        edge_type& edge = m_edge_kernel->find(*edge_itr);
                        //only proceed if we havn't assigned a label yet
                        if (edge.get_label() == 0)
                        {
                            //enter labeling routine - after this call the entire connected component has been labeled.
                            label_co_bound(*node_itr, edge, label);
                            ++label; //used that label, increase to next...
                        }
                    }
                    
                    //with all the components now labeled, we can proceed to compress the co-boundary relation
                    edge_itr = edge_vec.begin(); //reset iterator
                    label = 1;               //reset label
                    node.get_co_boundary()->clear(); //reset the co-boundary
                    //labels should come in increasing order starting from 1
                    for( ;edge_itr != edge_vec.end() ; ++edge_itr)
                    {
                        edge_type& edge = m_edge_kernel->find(*edge_itr);
                        assert(edge.get_label() <= label || !"Label ordering is wacked while compressing nodes");
                        if (edge.get_label() == label)
                        {
                            node.add_co_face(*edge_itr);
                            ++label;
                        }
                        //reset labels
                        reset_label(edge);
                    }
                    //done compressing that node's co_boundary.. next node...
                    m_uncompressed = 0; //reset counting label
                }
                
                //now for edges... more or less same procedure - could be generalized into function, but...
                typename simplex_set_type::edge_set_iterator edge_itr = s.edges_begin();
                for( ; edge_itr != s.edges_end() ; ++edge_itr)
                {
                    //first we need to label all edges for each connected component in the star of the node
                    edge_type& edge = m_edge_kernel->find(*edge_itr);
                    if (edge.is_compact()) continue;
                    edge.set_compact(true);
                    typename edge_type::co_boundary_set cob_set = edge.get_co_boundary();
                    std::vector<face_key_type> face_vec(cob_set->size());
                    std::copy(cob_set->begin(), cob_set->end(), face_vec.begin());
                    typename std::vector<face_key_type>::iterator face_itr = face_vec.begin();
                    //co_boundary_iterator edge_itr = cob_set->begin();
                    int label = 1;
                    for( ;face_itr != face_vec.end() ; ++face_itr)
                    {
                        //find an iterator to the actual edge
                        face_type& face = m_face_kernel->find(*face_itr);
                        //only proceed if we havn't assigned a label yet
                        if (face.get_label() == 0)
                        {
                            //enter labeling routine - after this call the entire connected component has been labeled.
                            label_co_bound(*edge_itr, face, label);
                            ++label; //used that label, increase to next...
                        }
                    }
                    
                    //with all the components now labeled, we can proceed to compress the co-boundary relation
                    face_itr = face_vec.begin(); //reset iterator
                    label = 1;               //reset label
                    edge.get_co_boundary()->clear(); //reset the co-boundary
                    //labels should come in increasing order starting from 1
                    for( ;face_itr != face_vec.end() ; ++face_itr)
                    {
                        face_type& face = m_face_kernel->find(*face_itr);
                        assert(face.get_label() <= label || !"Label ordering is wacked while compressing nodes");
                        if (face.get_label() == label)
                        {
                            edge.add_co_face(*face_itr);
                            ++label;
                        }
                        reset_label(face);
                    }
                    //done compressing that node's co_boundary.. next node...
                }
            } //compress(simplex_set_type)
            
            
            void uncompress(const tetrahedron_key_type & t) {}
            void uncompress(const face_key_type & f) {}
            
            //// NOT TESTED!!!!!
            void uncompress(const edge_key_type & edge_k)
            { 
                //assert(0);
                ++m_uncompressed;
                edge_type& edge = lookup_simplex(edge_k);
                if (!edge.is_compact()) return; //only uncompress compressed nodes.
                edge.set_compact(false);
                simplex_set_type star_set;
                star(edge_k, star_set);
                typename simplex_set_type::face_set_iterator face_itr = star_set.faces_begin();
                for( ; face_itr != star_set.faces_end(); ++face_itr)
                {
                    if(in_boundary(edge_k, *face_itr))
                        edge.add_co_face(*face_itr);
                }
            }
            
            //// NOT TESTED!!!!!!
            void uncompress(const node_key_type & node_k)
            { 
                //assert(0);
                ++m_uncompressed;
                node_type& node = lookup_simplex(node_k);
                if (!node.is_compact()) return; //only uncompress compressed nodes.
                node.set_compact(false);
                simplex_set_type star_set;
                star(node_k, star_set);
                typename simplex_set_type::edge_set_iterator edge_itr = star_set.edges_begin();
                for( ; edge_itr != star_set.edges_end(); ++edge_itr)
                {
                    if(in_boundary(node_k, *edge_itr))
                        node.add_co_face(*edge_itr);
                }
            }
            
            
            void uncompress(simplex_set_type & s)
            {
                //first uncompress edges
                for (typename simplex_set_type::edge_set_iterator edge_itr = s.edges_begin(); edge_itr != s.edges_end() ; ++edge_itr )
                {
                    uncompress(*edge_itr);
                }
                //uncompress nodes
                for (typename simplex_set_type::node_set_iterator node_itr = s.nodes_begin(); node_itr != s.nodes_end() ; ++node_itr )
                {
                    uncompress(*node_itr);
                }
            }
            
            double uncompressed_ratio()
            {
                return ( (double) m_uncompressed / (double) (size_nodes() + size_edges()) );
            }
            
            /**
             * This method will print the status of the mesh to the console.
             */
            void debug_print()
            {
                std::cout << "Dumping state of is_mesh::t4kernel" << std::endl;
                for (node_iterator iter = m_node_kernel->begin(); iter != m_node_kernel->end(); ++iter)
                {
                    std::cout << "Node [" << iter.key() << "] label : " << iter->get_label() << std::endl;
                    std::cout << "  Compressed : " << iter->is_compact() << std::endl;
                    std::cout << "  Co-boundary : ";
                    typename node_type::co_boundary_iterator c_iter = iter->get_co_boundary()->begin();
                    for(; c_iter != iter->get_co_boundary()->end(); ++c_iter)
                    {
                        std::cout << "[" << (*c_iter) << "]";
                    }
                    std::cout << std::endl;
                }
                for (edge_iterator iter = m_edge_kernel->begin(); iter != m_edge_kernel->end(); ++iter)
                {
                    std::cout << "Edge [" << iter.key() << "] label : " << iter->get_label()  << std::endl;
                    std::cout << "  Compressed : " << iter->is_compact() << std::endl;
                    std::cout << "  Boundary    : ";
                    typename edge_type::boundary_iterator b_iter = iter->get_boundary()->begin();
                    for(; b_iter != iter->get_boundary()->end(); ++b_iter)
                    {
                        std::cout << "[" << (*b_iter) << "]";
                    }
                    std::cout << std::endl;
                    std::cout << "  Co-boundary : ";
                    typename edge_type::co_boundary_iterator c_iter = iter->get_co_boundary()->begin();
                    for( ; c_iter != iter->get_co_boundary()->end(); ++c_iter)
                    {
                        std::cout << "[" << (*c_iter) << "]";
                    }
                    std::cout << std::endl;
                }
                for (face_iterator iter = m_face_kernel->begin(); iter != m_face_kernel->end(); ++iter)
                {
                    std::cout << "Face [" << iter.key() << "] label : " << iter->get_label()  << std::endl;
                    std::cout << "  Compressed : " << iter->is_compact() << std::endl;
                    std::cout << "  Boundary    : ";
                    typename face_type::boundary_iterator b_iter = iter->get_boundary()->begin();
                    for(; b_iter != iter->get_boundary()->end(); ++b_iter)
                    {
                        std::cout << "[" << (*b_iter) << "]";
                    }
                    std::cout << std::endl;
                    std::cout << "  Co-boundary : ";
                    typename face_type::co_boundary_iterator c_iter = iter->get_co_boundary()->begin();
                    for(; c_iter != iter->get_co_boundary()->end(); ++c_iter)
                    {
                        std::cout << "[" << (*c_iter) << "]";
                    }
                    std::cout << std::endl;
                }
                for (tetrahedron_iterator iter = m_tetrahedron_kernel->begin(); iter != m_tetrahedron_kernel->end(); ++iter)
                {
                    std::cout << "Tet  [" << iter.key() << "] label : " << iter->get_label() << std::endl;
                    std::cout << "  Compressed : " << iter->is_compact() << std::endl;
                    std::cout << "  Boundary    : ";
                    typename tetrahedron_type::boundary_iterator c_iter = iter->get_boundary()->begin();
                    for(; c_iter != iter->get_boundary()->end(); ++c_iter)
                    {
                        std::cout << "[" << (*c_iter) << "]";
                    }
                    std::cout << std::endl;
                }
            }
            
            /**
             * Marek
             * Boundary of a simplex.
             */
            template<typename key_type>
            void boundary(key_type const & k, simplex_set_type & result_set)
            {
                boundary_helper(k, result_set);
            }
            
            /**
             * Marek
             * Boundary of a set of tetrahedra.
             */
            void boundary(simplex_set_type & tetrahedra, simplex_set_type & result_set)
            {
                boundary_helper2<tetrahedron_key_type, face_key_type>(tetrahedra, result_set);
            }
            
            /**
             * Marek
             */
            void boundary_2manifold(simplex_set_type & faces, simplex_set_type & result_set)
            {
                boundary_helper2<face_key_type, edge_key_type>(faces, result_set);
            }
            
            /**
             * Marek
             * Closure of a simplex.
             */
            template<typename key_type>
            void closure(key_type const & k, simplex_set_type & result_set)
            {
                boundary_helper(k, result_set);
                result_set.insert(k); 
            }
            
            /**
             * Marek.
             * Closure of a simplex set.
             */
            void closure(simplex_set_type & input_set, simplex_set_type & result_set)
            {
                closure_helper(input_set, result_set);
            }
            
            /**
             * Marek
             * Induces consistent orientations on all faces of a simplex sk.
             */
            template<typename key_type>
            void orient_faces_consistently(key_type const & sk)
            {
                typedef typename util::simplex_traits<mesh_type, key_type::dim>::simplex_type simplex_type;
                
                typename simplex_type::boundary_list boundary = lookup_simplex(sk).get_boundary();
                typename simplex_type::boundary_iterator it = boundary->begin();
                
                while (it != boundary->end())
                {
                    orient_face_helper(sk, *it, true);
                    ++it;
                }
            }
            
            /**
             * Marek
             * Induces consistent orientation on a face fk of a simplex sk.
             */
            template<typename key_type_simplex
            , typename key_type_face>
            void orient_face_consistently(key_type_simplex const & sk, key_type_face const & fk)
            {
                orient_face_helper(sk, fk, true);
            }
            
            /**
             * Marek
             * Sets the orientation of the simplex sk so that is consistent with the orientation of its boundary face fk.
             */
            template<typename key_type_simplex
            , typename key_type_coface>
            void orient_coface_consistently(key_type_simplex const & fk, key_type_coface const & sk)
            {
                orient_coface_helper(fk, sk, true);
            }
            
            /**
             * Marek
             * Induces opposite orientations on all faces of a simplex sk.
             */
            template<typename key_type>
            void orient_faces_oppositely(key_type const & sk)
            {
                typedef typename util::simplex_traits<mesh_type, key_type::dim>::simplex_type simplex_type;
                
                typename simplex_type::boundary_list boundary = lookup_simplex(sk).get_boundary();
                typename simplex_type::boundary_iterator it = boundary->begin();
                
                while (it != boundary->end())
                {
                    orient_face_helper(sk, *it, false);
                    ++it;
                }
            }
            
            /**
             * Marek
             * Induces opposite orientation on a face fk of a simplex sk.
             */
            template<typename key_type_simplex
            , typename key_type_face>
            void orient_face_oppositely(key_type_simplex const & sk, key_type_face const & fk)
            {
                orient_face_helper(sk, fk, false);
            }
            
            /**
             * Marek
             * Sets the orientation of the simplex sk so that is opposite to the orientation of its boundary face fk.
             */
            template<typename key_type_simplex
            , typename key_type_coface>
            void orient_coface_oppositely(key_type_simplex const & fk, key_type_coface const & sk)
            {
                orient_coface_helper(fk, sk, false);
            }
            
            /**
             * Marek
             * Provided that simplices k1 and k2 are (dimension-1)-adjacent, finds the shared (dimension-1)-simplex k.
             */
            template<typename key_type_simplex
            , typename key_type_face>
            bool get_intersection(key_type_simplex const & k1, key_type_simplex const & k2, key_type_face & k)
            {
                assert(k1 != k2 || !"The same key for both input simplices");
                assert(key_type_simplex::dim > 0 || !"Cannot intersect two vertices");
                
                typedef typename util::simplex_traits<mesh_type, key_type_simplex::dim>::simplex_type simplex_type;
                
                typename simplex_type::boundary_list b1 = lookup_simplex(k1).get_boundary();
                typename simplex_type::boundary_list b2 = lookup_simplex(k2).get_boundary();
                
                typename simplex_type::boundary_iterator bi1 = b1->begin();
                while (bi1 != b1->end())
                {
                    typename simplex_type::boundary_iterator bi2 = b2->begin();
                    while (bi2 != b2->end())
                    {
                        if (*bi1 == *bi2)
                        {
                            k = *bi1;
                            return true;
                        }
                        ++bi2;
                    }
                    ++bi1;
                }
                
                return false;
            }
            
            /**
             * Marek
             */
            void remove_edge(edge_key_type const & removed_edge, 
                             std::vector<node_key_type> & new_edges_desc,
                             simplex_set_type & new_simplices)
            {
                remove_edge_helper(removed_edge, new_edges_desc, new_simplices);
            }
            
            /**
             * Marek
             */
            void multi_face_remove(simplex_set_type & removed_faces,
                                   simplex_set_type & new_simplices)
            {
                multi_face_remove_helper(removed_faces, new_simplices);
            }
            
            /**
             * Marek
             */
            void multi_face_retriangulation(simplex_set_type & removed_faces,
                                            std::vector<node_key_type> & new_edges_desc,
                                            simplex_set_type & new_faces,
                                            simplex_set_type & new_simplices)
            {
                multi_face_retriangulation_helper(removed_faces, new_edges_desc, new_faces, new_simplices);
            }

			/**
			 *
			 */
			node_key_type vertex_insertion(simplex_set_type & removed_tets,
										   simplex_set_type & new_simplices)
			{
				return vertex_insertion_helper(removed_tets, new_simplices);
			}
            
            /**
             * Marek
             */
            node_key_type split_tetrahedron(tetrahedron_key_type & t)
            {
                std::map<tetrahedron_key_type, tetrahedron_key_type> new_tets;
                return split_tetrahedron_helper(t, new_tets);
            }
            
            /**
             * Marek
             */
            node_key_type split_face(face_key_type & f)
            {
                std::map<tetrahedron_key_type, tetrahedron_key_type> new_tets;
                return split_face_helper(f, new_tets);
            }
            
            /**
             * Marek
             */
            node_key_type split_edge(edge_key_type & e)
            {
                std::map<tetrahedron_key_type, tetrahedron_key_type> new_tets;
                return split_edge_helper(e, new_tets);
            }
            
            /**
             * Marek
             */
            node_key_type edge_collapse(edge_key_type & e)
            {
                typename edge_type::boundary_list e_boundary = lookup_simplex(e).get_boundary();
                typename edge_type::boundary_iterator ebit = e_boundary->begin();
                node_key_type n1 = *ebit;
                ++ebit;
                node_key_type n2 = *ebit;
                return edge_collapse_helper(e, n1, n2);
            }
            
            void link(tetrahedron_key_type const & k, simplex_set_type & result){}
            
            void link(face_key_type const & f, simplex_set_type & result)
            {
                typename util::simplex_traits<mesh_type, 0>::simplex_tag tag;
                
                simplex_set_type st_f, cl_f;
                star(f, st_f);
                closure(st_f, result);
                closure(f, cl_f);
                result.difference(cl_f);
                result.difference(st_f);
                result.filter(tag);
            }
            
            void link(edge_key_type const & e, simplex_set_type & result, simplex_set_type & st_e)
            {
                typename util::simplex_traits<mesh_type, 0>::simplex_tag tag;
                
                simplex_set_type cl_e, temp;
                star(e, st_e);
                closure(st_e, temp);
                closure(e, cl_e);
                node_key_type n1, n2;
                simplex_set_type::node_set_iterator nit = cl_e.nodes_begin();
                n1 = *nit;  ++nit;  n2 = *nit;
                temp.difference(st_e);
                temp.difference(cl_e);
                simplex_set_type::edge_set_iterator eit = temp.edges_begin();
                while (eit != temp.edges_end())
                {
                    typename edge_type::boundary_list ebnd = find_edge(*eit).get_boundary();
                    typename edge_type::boundary_iterator ebit = ebnd->begin();
                    if (*ebit == n1 || *ebit == n2)
                    {
                        ++eit;
                        continue;   
                    }
                    ++ebit;
                    if (*ebit == n1 || *ebit == n2)
                    {
                        ++eit;
                        continue;
                    }
                    result.insert(*eit);
                    ++eit;
                }
                temp.filter(tag);
                result.add(temp);
            }
            
            void link(edge_key_type const & e, simplex_set_type & result)
            {
                simplex_set_type st_e;
                link(e, result, st_e);
            }
            
            void link(node_key_type const & n, simplex_set_type & result)
            {
                simplex_set_type st_n;
                star(n, st_n);
                st_n.insert(n);
                closure(st_n, result);
                result.difference(st_n);
            }
            
            /**
             * Marek
             */
            void vertices(tetrahedron_key_type const & t, std::vector<node_key_type> & verts)
            {
                typename tetrahedron_type::boundary_list t_boundary = lookup_simplex(t).get_boundary();
                typename tetrahedron_type::boundary_iterator tbit = t_boundary->begin();
                std::vector<node_key_type> f_verts(3);
                vertices(*tbit, f_verts);
                verts[1] = f_verts[0];
                verts[2] = f_verts[1];
                verts[3] = f_verts[2];
                ++tbit;
                vertices(*tbit, f_verts);
                int k = -1, k1 = -1, k2 = -1;
                for (k = 0; k < 3; ++k)
                {
                    if (f_verts[k] == verts[1] || f_verts[k] == verts[2] || f_verts[k] == verts[3])
                        continue;
                    else
                        k1 = k;
                }
                for (k = 0; k < 4; ++k)
                {
                    if (verts[k] == f_verts[0] || verts[k] == f_verts[1] || verts[k] == f_verts[2])
                        continue;
                    else
                        k2 = k;
                }
                assert((k != -1 && k1 != -1 && k2 != -1) || !"Vertex lists corrupted!");
                verts[0] = f_verts[k1];
                node_key_type temp = verts[1];
                verts[1] = verts[k2];
                verts[k2] = temp;
                ++tbit;
                vertices(*tbit, f_verts);
                for (k = 0; k < 4; ++k)
                {
                    if (verts[k] == f_verts[0] || verts[k] == f_verts[1] || verts[k] == f_verts[2])
                        continue;
                    else
                        k1 = k;
                }
                temp = verts[2];
                verts[2] = verts[k1];
                verts[k1] = temp;
            }
            
            /**
             *
             */
            void vertices(face_key_type const & f, std::vector<node_key_type> & verts)
            {
                typename face_type::boundary_list f_boundary = lookup_simplex(f).get_boundary();
                typename face_type::boundary_iterator fbit = f_boundary->begin();
                std::vector<node_key_type> e_verts(2);
                vertices(*fbit, e_verts);
                verts[1] = e_verts[1];
                verts[2] = e_verts[0];
                ++fbit;
                vertices(*fbit, e_verts);
                if (e_verts[0] == verts[1] || e_verts[0] == verts[2])
                    verts[0] = e_verts[1];
                else
                    verts[0] = e_verts[0];
                
                if (verts[1] == e_verts[0] || verts[1] == e_verts[1])
                {
                    node_key_type temp = verts[1];
                    verts[1] = verts[2];
                    verts[2] = temp;
                }
            }
            
            /**
             *
             */
            void vertices(edge_key_type const & e, std::vector<node_key_type> & verts) 
            {
                typename edge_type::boundary_list e_boundary = lookup_simplex(e).get_boundary();
                typename edge_type::boundary_iterator ebit = e_boundary->begin();
                int i = 1;
                while (ebit != e_boundary->end())
                {
                    verts[i] = *ebit;
                    --i;
                    ++ebit;
                }
            }
            
            /**
             *
             */
            void vertices(node_key_type const & n, std::vector<node_key_type> & verts) {}
            
            /**
             *
             */
            bool is_boundary(tetrahedron_key_type & t) { return false; }
            
            /**
             *
             */
            bool is_boundary(face_key_type & f)
            {
                typename face_type::co_boundary_set f_coboundary = lookup_simplex(f).get_co_boundary();
                if (f_coboundary->size() == 2) return false;
                return true;
            }
            
            /**
             *
             */
            bool is_boundary(edge_key_type & e)
            {
                simplex_set_type ste;
                star(e, ste);
                simplex_set_type::face_set_iterator fit = ste.faces_begin();
                if (ste.faces_begin() == ste.faces_end()) return true;
                while (fit != ste.faces_end())
                {
                    if (is_boundary(*fit)) return true;
                    ++fit;
                }
                return false;
            }
            
            /**
             *
             */
            bool is_boundary(node_key_type & n)
            {
                simplex_set_type stn;
                star(n, stn);
                simplex_set_type::face_set_iterator fit = stn.faces_begin();
                if (stn.faces_begin() == stn.faces_end()) return true;
                while (fit != stn.faces_end())
                {
                    if (is_boundary(*fit)) return true;
                    ++fit;
                }
                return false;
            }
            
            /**
             *
             */
            bool exists(tetrahedron_key_type const & t)
            {
                return m_tetrahedron_kernel->is_valid(t);
            }
            
            /**
             *
             */
            bool exists(face_key_type const & f)
            {
                return m_face_kernel->is_valid(f);
            }
            
            /**
             *
             */
            bool exists(edge_key_type const & e)
            {
                return m_edge_kernel->is_valid(e);
            }
            
            /**
             *
             */
            bool exists(node_key_type const & n)
            {
                return m_node_kernel->is_valid(n);
            }
            
            /**
             *
             */
            void validate_nodes()
            {
                typename node_kernel_type::iterator nit = nodes_begin();
                typename node_kernel_type::iterator nn_it = nodes_end();
                
                while (nit != nn_it)
                {
                    typename node_type::co_boundary_set n_coboundary = nit->get_co_boundary();
                    typename node_type::co_boundary_iterator ncit = n_coboundary->begin();
                    while (ncit != n_coboundary->end())
                    {
                        edge_key_type ek = *ncit;
                        typename edge_type::boundary_list e_boundary = lookup_simplex(ek).get_boundary();
                        typename edge_type::boundary_iterator ebit = e_boundary->begin();
                        bool b = false;
                        while (ebit != e_boundary->end())
                        {
                            if (*ebit == nit.key())
                                b = true;
                            ++ebit;
                        }
                        assert( b || !"Node is not in the boundary of its coboundary simplex" );
                        ++ncit;
                    }
                    ++nit;
                }
            }
            
            /**
             *
             */
            void validate_edges()
            {
                typename edge_kernel_type::iterator eit = edges_begin();
                typename edge_kernel_type::iterator ee_it = edges_end();
                
                while (eit != ee_it)
                {
                    typename edge_type::boundary_list e_boundary = eit->get_boundary();
                    assert( e_boundary->size() == 2 || !"Boundary of the edge corrupted!" );
                    typename edge_type::boundary_iterator ebit = e_boundary->begin();
                    while (ebit != e_boundary->end())
                    {
                        lookup_simplex(*ebit);
                        ++ebit;
                    }
                    
                    typename edge_type::co_boundary_set e_coboundary = eit->get_co_boundary();
                    typename edge_type::co_boundary_iterator ecit = e_coboundary->begin();
                    while (ecit != e_coboundary->end())
                    {
                        face_key_type fk = *ecit;
                        typename face_type::boundary_list f_boundary = lookup_simplex(fk).get_boundary();
                        typename face_type::boundary_iterator fbit = f_boundary->begin();
                        bool b = false;
                        while (fbit != f_boundary->end())
                        {
                            if (*fbit == eit.key())
                                b = true;
                            ++fbit;
                        }
                        assert( b || !"Edge is not in the boundary of its coboundary simplex" );
                        ++ecit;
                    }
                    ++eit;
                }
            }
            
            /**
             *
             */
            void validate_faces()
            {
                typename face_kernel_type::iterator fit = faces_begin();
                typename face_kernel_type::iterator ff_it = faces_end();
                
                while (fit != ff_it)
                {
                    typename face_type::boundary_list f_boundary = fit->get_boundary();
                    assert( f_boundary->size() == 3 || !"Boundary of the face corrupted!" );
                    typename face_type::boundary_iterator fbit = f_boundary->begin();
                    while (fbit != f_boundary->end())
                    {
                        lookup_simplex(*fbit);
                        ++fbit;
                    }
                    
                    typename face_type::co_boundary_set f_coboundary = fit->get_co_boundary();
                    assert( f_coboundary->size() < 3 || !"Co-boundary of the face corrupted!" );
                    assert( f_coboundary->size() > 0 || !"Mesh should be manifold!" );
                    typename face_type::co_boundary_iterator fcit = f_coboundary->begin();
                    while (fcit != f_coboundary->end())
                    {
                        tetrahedron_key_type tk = *fcit;
                        typename tetrahedron_type::boundary_list t_boundary = lookup_simplex(tk).get_boundary();
                        typename tetrahedron_type::boundary_iterator tbit = t_boundary->begin();
                        bool b = false;
                        while (tbit != t_boundary->end())
                        {
                            if (*tbit == fit.key())
                                b = true;
                            ++tbit;
                        }
                        assert( b || !"Face is not in the boundary of its coboundary simplex" );
                        ++fcit;
                    }
                    ++fit;
                }
            }
            
            /**
             *
             */
            void validate_tetrahedra()
            {
                typename tetrahedron_kernel_type::iterator tit = tetrahedra_begin();
                typename tetrahedron_kernel_type::iterator tt_it = tetrahedra_end();
                
                while (tit != tt_it)
                {
                    typename tetrahedron_type::boundary_list t_boundary = tit->get_boundary();
                    assert( t_boundary->size() == 4 || !"Boundary of the face corrupted!" );
                    typename tetrahedron_type::boundary_iterator tbit = t_boundary->begin();
                    while (tbit != t_boundary->end())
                    {
                        lookup_simplex(*tbit);
                        ++tbit;
                    }
                    
                    ++tit;
                }
            }
            
            /**
             *
             */
            void find_connected_component(face_key_type & f,
                                          simplex_set_type & multi_face,
                                          simplex_set_type & connected_component)
            {
                bool changes = true;
                std::map<face_key_type, bool> visited;
                std::map<face_key_type, bool> added;
                
                assert ( multi_face.contains(f) || !"Multi-face doesn't contain given face!");
                
                connected_component.insert(f);
                added[f] = true;
                
                while (changes)
                {
                    changes = false;
                    
                    simplex_set_type::face_set_iterator fit = connected_component.faces_begin();
                    simplex_set_type adjacent_faces;
                    
                    while (fit != connected_component.faces_end())
                    {
                        if (!visited[*fit])
                        {
                            simplex_set_type::face_set_iterator mfit = multi_face.faces_begin();
                            while (mfit != multi_face.faces_end())
                            {
                                if (!visited[*mfit] && !added[*mfit])
                                {
                                    edge_key_type e;
                                    if (get_intersection(*fit, *mfit, e))
                                    {
                                        adjacent_faces.insert(*mfit);
                                        added[*mfit] = true;
                                        changes = true;
                                    }
                                }
                                ++mfit;
                            }
                            visited[*fit] = true;
                        }
                        ++fit;
                    }
                    
                    connected_component.add(adjacent_faces);
                }
            }
            
            /**
             *
             */
            void find_min_multi_face(face_key_type const & f,
                                     simplex_set_type & multi_face,
                                     simplex_set_type & min_multi_face)
            {
                simplex_set_type::face_set_iterator fit = multi_face.faces_begin();
                while (fit != multi_face.faces_end())
                {
                    if (*fit != f)
                    {
                        edge_key_type e;
                        if (get_intersection(f, *fit, e))
                            min_multi_face.insert(f);
                    }
                    ++fit;
                }
                min_multi_face.insert(f);
            }
            
            /**
             *
             */
            void find_connected_component(face_key_type const & f,
                                          simplex_set_type & multi_face,
                                          simplex_set_type & feature_edges,
                                          simplex_set_type & connected_component)
            {
                bool changes = true;
                std::map<face_key_type, bool> visited;
                std::map<face_key_type, bool> added;
                std::map<face_key_type, bool> rejected;
                
                assert ( multi_face.contains(f) || !"Multi-face doesn't contain given face!");
                
                connected_component.insert(f);
                added[f] = true;
                
                while (changes)
                {
                    changes = false;
                    
                    simplex_set_type::face_set_iterator fit = connected_component.faces_begin();
                    simplex_set_type adjacent_faces;
                    
                    while (fit != connected_component.faces_end())
                    {
                        if (!visited[*fit])
                        {
                            simplex_set_type::face_set_iterator mfit = multi_face.faces_begin();
                            while (mfit != multi_face.faces_end())
                            {
                                if (!visited[*mfit] && !added[*mfit] && !rejected[*mfit])
                                {
                                    edge_key_type e;
                                    if (get_intersection(*fit, *mfit, e))
                                    {
                                        if (feature_edges.contains(e))
                                        {
                                            rejected[*mfit] = true;
                                        }
                                        else
                                        {
                                            adjacent_faces.insert(*mfit);
                                            added[*mfit] = true;
                                            changes = true;
                                        }
                                    }
                                }
                                ++mfit;
                            }
                            visited[*fit] = true;
                        }
                        ++fit;
                    }
                    
                    connected_component.add(adjacent_faces);
                }
            }
            
            /**
             *
             */
            void set_undo_mark(simplex_set_type & marked_simplices)
            {
                if (marked_simplices.size_nodes() > 0)
                {
                    unsigned int i = static_cast<unsigned int>(m_node_undo_stack.size());
                    m_node_undo_stack.resize(m_node_undo_stack.size() + marked_simplices.size_nodes());
                    m_node_mark_stack.push_front(static_cast<unsigned int>(marked_simplices.size_nodes()));
                    
                    m_node_kernel->set_undo_mark(marked_simplices.nodes_begin(), marked_simplices.nodes_end());
                    
                    simplex_set_type::node_set_iterator nit = marked_simplices.nodes_begin();
                    while (nit != marked_simplices.nodes_end())
                    {
                        node_type & n = lookup_simplex(*nit);
                        m_node_undo_stack[i].key = *nit;
                        m_node_undo_stack[i].old_co_boundary = n.get_co_boundary();
                        m_node_undo_stack[i].new_co_boundary = new typename node_type::set_type;
                        typename node_type::co_boundary_iterator it = m_node_undo_stack[i].old_co_boundary->begin();
                        while (it != m_node_undo_stack[i].old_co_boundary->end())
                        {
                            m_node_undo_stack[i].new_co_boundary->insert(*it);
                            ++it;
                        }
                        n.set_co_boundary_set(m_node_undo_stack[i].new_co_boundary);
                        ++i;
                        ++nit;
                    }
                }
                
                if (marked_simplices.size_edges() > 0)
                {
                    unsigned int i = static_cast<unsigned int>(m_edge_undo_stack.size());
                    m_edge_undo_stack.resize(m_edge_undo_stack.size() + marked_simplices.size_edges());
                    m_edge_mark_stack.push_front(static_cast<unsigned int>(marked_simplices.size_edges()));
                    
                    m_edge_kernel->set_undo_mark(marked_simplices.edges_begin(), marked_simplices.edges_end());
                    
                    simplex_set_type::edge_set_iterator eit = marked_simplices.edges_begin();
                    while (eit != marked_simplices.edges_end())
                    {
                        edge_type & e = lookup_simplex(*eit);
                        m_edge_undo_stack[i].key = *eit;
                        m_edge_undo_stack[i].old_boundary = e.get_boundary();
                        m_edge_undo_stack[i].new_boundary = new typename edge_type::list_type;
                        m_edge_undo_stack[i].old_co_boundary = e.get_co_boundary();
                        m_edge_undo_stack[i].new_co_boundary = new typename edge_type::set_type;
                        
                        typename edge_type::co_boundary_iterator cit = m_edge_undo_stack[i].old_co_boundary->begin();
                        while (cit != m_edge_undo_stack[i].old_co_boundary->end())
                        {
                            m_edge_undo_stack[i].new_co_boundary->insert(*cit);
                            ++cit;
                        }
                        e.set_co_boundary_set(m_edge_undo_stack[i].new_co_boundary);
                        
                        typename edge_type::boundary_iterator bit = m_edge_undo_stack[i].old_boundary->begin();
                        while (bit != m_edge_undo_stack[i].old_boundary->end())
                        {
                            m_edge_undo_stack[i].new_boundary->push_back(*bit);
                            ++bit;
                        }
                        e.set_boundary_list(m_edge_undo_stack[i].new_boundary);
                        
                        ++i;
                        ++eit;
                    }
                }
                
                if (marked_simplices.size_faces() > 0)
                {
                    unsigned int i = static_cast<unsigned int>(m_face_undo_stack.size());
                    m_face_undo_stack.resize(m_face_undo_stack.size() + marked_simplices.size_faces());
                    m_face_mark_stack.push_front(static_cast<unsigned int>(marked_simplices.size_faces()));
                    
                    m_face_kernel->set_undo_mark(marked_simplices.faces_begin(), marked_simplices.faces_end());
                    
                    simplex_set_type::face_set_iterator fit = marked_simplices.faces_begin();
                    while (fit != marked_simplices.faces_end())
                    {
                        face_type & f = lookup_simplex(*fit);
                        m_face_undo_stack[i].key = *fit;
                        m_face_undo_stack[i].old_boundary = f.get_boundary();
                        m_face_undo_stack[i].new_boundary = new typename face_type::list_type;
                        m_face_undo_stack[i].old_co_boundary = f.get_co_boundary();
                        m_face_undo_stack[i].new_co_boundary = new typename face_type::set_type;
                        
                        typename face_type::co_boundary_iterator cit = m_face_undo_stack[i].old_co_boundary->begin();
                        while (cit != m_face_undo_stack[i].old_co_boundary->end())
                        {
                            m_face_undo_stack[i].new_co_boundary->insert(*cit);
                            ++cit;
                        }
                        f.set_co_boundary_set(m_face_undo_stack[i].new_co_boundary);
                        
                        typename face_type::boundary_iterator bit = m_face_undo_stack[i].old_boundary->begin();
                        while (bit != m_face_undo_stack[i].old_boundary->end())
                        {
                            m_face_undo_stack[i].new_boundary->push_back(*bit);
                            ++bit;
                        }
                        f.set_boundary_list(m_face_undo_stack[i].new_boundary);
                        
                        ++i;
                        ++fit;
                    }
                }
                if (marked_simplices.size_tetrahedra() > 0)
                {
                    unsigned int i = static_cast<unsigned int>(m_tetrahedron_undo_stack.size());
                    m_tetrahedron_undo_stack.resize(m_tetrahedron_undo_stack.size() + marked_simplices.size_tetrahedra());
                    m_tetrahedron_mark_stack.push_front(static_cast<unsigned int>(marked_simplices.size_tetrahedra()));
                    
                    m_tetrahedron_kernel->set_undo_mark(marked_simplices.tetrahedra_begin(), marked_simplices.tetrahedra_end());
                    
                    simplex_set_type::tetrahedron_set_iterator tit = marked_simplices.tetrahedra_begin();
                    while (tit != marked_simplices.tetrahedra_end())
                    {
                        tetrahedron_type & t = lookup_simplex(*tit);
                        m_tetrahedron_undo_stack[i].key = *tit;
                        m_tetrahedron_undo_stack[i].old_boundary = t.get_boundary();
                        m_tetrahedron_undo_stack[i].new_boundary = new typename tetrahedron_type::list_type;
                        
                        typename tetrahedron_type::boundary_iterator bit = m_tetrahedron_undo_stack[i].old_boundary->begin();
                        while (bit != m_tetrahedron_undo_stack[i].old_boundary->end())
                        {
                            m_tetrahedron_undo_stack[i].new_boundary->push_back(*bit);
                            ++bit;
                        }
                        t.set_boundary_list(m_tetrahedron_undo_stack[i].new_boundary);
                        
                        ++i;
                        ++tit;
                    }
                }
            }
            
            /**
             *
             */
            void undo()
            {
                unsigned int cnt;
                
                if (!m_node_mark_stack.empty())
                {
                    m_node_kernel->undo();
                    cnt = m_node_mark_stack.front();
                    
                    for (unsigned int i = 0; i < cnt; ++i)
                    {
                        unsigned int k = static_cast<unsigned int>(m_node_undo_stack.size()) - (i+1);
                        node_type & n = lookup_simplex(m_node_undo_stack[k].key);
                        n.set_co_boundary_set(m_node_undo_stack[k].old_co_boundary);
                        delete m_node_undo_stack[k].new_co_boundary;
                    }
                    
                    m_node_undo_stack.resize(m_node_undo_stack.size() - cnt);
                    m_node_mark_stack.pop_front();
                }
                
                if (!m_edge_mark_stack.empty())
                {
                    m_edge_kernel->undo();
                    cnt = m_edge_mark_stack.front();
                    
                    for (unsigned int i = 0; i < cnt; ++i)
                    {
                        unsigned int k = static_cast<unsigned int>(m_edge_undo_stack.size()) - (i+1);
                        edge_type & e = lookup_simplex(m_edge_undo_stack[k].key);
                        e.set_co_boundary_set(m_edge_undo_stack[k].old_co_boundary);
                        e.set_boundary_list(m_edge_undo_stack[k].old_boundary);
                        delete m_edge_undo_stack[k].new_co_boundary;
                        delete m_edge_undo_stack[k].new_boundary;
                    }
                    
                    m_edge_undo_stack.resize(m_edge_undo_stack.size() - cnt);
                    m_edge_mark_stack.pop_front();
                }
                
                if (!m_face_mark_stack.empty())
                {
                    m_face_kernel->undo();
                    cnt = m_face_mark_stack.front();
                    
                    for (unsigned int i = 0; i < cnt; ++i)
                    {
                        unsigned int k = static_cast<unsigned int>(m_face_undo_stack.size()) - (i+1);
                        face_type & f = lookup_simplex(m_face_undo_stack[k].key);
                        f.set_co_boundary_set(m_face_undo_stack[k].old_co_boundary);
                        f.set_boundary_list(m_face_undo_stack[k].old_boundary);
                        delete m_face_undo_stack[k].new_co_boundary;
                        delete m_face_undo_stack[k].new_boundary;
                    }
                    
                    m_face_undo_stack.resize(m_face_undo_stack.size() - cnt);
                    m_face_mark_stack.pop_front();
                }
                
                if (!m_tetrahedron_mark_stack.empty())
                {
                    m_tetrahedron_kernel->undo();
                    cnt = m_tetrahedron_mark_stack.front();
                    
                    for (unsigned int i = 0; i < cnt; ++i)
                    {
                        unsigned int k = static_cast<unsigned int>(m_tetrahedron_undo_stack.size()) - (i+1);
                        tetrahedron_type & t = lookup_simplex(m_tetrahedron_undo_stack[k].key);
                        t.set_boundary_list(m_tetrahedron_undo_stack[k].old_boundary);
                        delete m_tetrahedron_undo_stack[k].new_boundary;
                    }
                    
                    m_tetrahedron_undo_stack.resize(m_tetrahedron_undo_stack.size() - cnt);
                    m_tetrahedron_mark_stack.pop_front();
                }
            }
            
            /**
             *
             */
            void undo_all()
            {
                while (!m_node_mark_stack.empty() ||
                       !m_edge_mark_stack.empty() ||
                       !m_face_mark_stack.empty() ||
                       !m_tetrahedron_mark_stack.empty())
                    undo();
            }
            
            /**
             *
             */
            void garbage_collect()
            {
                m_node_kernel->garbage_collect();
                for (unsigned int i = 0; i < m_node_undo_stack.size(); ++i)
                {
                    delete m_node_undo_stack[i].old_co_boundary;
                }
                m_node_undo_stack.clear();
                m_node_mark_stack.clear();
                
                m_edge_kernel->garbage_collect();
                for (unsigned int i = 0; i < m_edge_undo_stack.size(); ++i)
                {
                    delete m_edge_undo_stack[i].old_boundary;
                    delete m_edge_undo_stack[i].old_co_boundary;
                }
                m_edge_undo_stack.clear();
                m_edge_mark_stack.clear();
                
                m_face_kernel->garbage_collect();
                for (unsigned int i = 0; i < m_face_undo_stack.size(); ++i)
                {
                    delete m_face_undo_stack[i].old_boundary;
                    delete m_face_undo_stack[i].old_co_boundary;
                }
                m_face_undo_stack.clear();
                m_face_mark_stack.clear();
                
                m_tetrahedron_kernel->garbage_collect();
                for (unsigned int i = 0; i < m_tetrahedron_undo_stack.size(); ++i)
                {
                    delete m_tetrahedron_undo_stack[i].old_boundary;
                }
                m_tetrahedron_undo_stack.clear();
                m_tetrahedron_mark_stack.clear();
            }
            
            /**
             *
             */
            void commit()
            {
                m_node_kernel->commit();
                if (!m_node_mark_stack.empty())
                {
                    unsigned int i = m_node_mark_stack.front();
                    unsigned int new_size = m_node_undo_stack.size() - i;
                    for ( ; i > 0; --i)
                    {
                        delete m_node_undo_stack[new_size + i - 1].old_co_boundary;
                    }
                    m_node_undo_stack.resize(new_size);
                    m_node_mark_stack.pop_front();
                }
                
                m_edge_kernel->commit();
                if (!m_edge_mark_stack.empty())
                {
                    unsigned int i = m_edge_mark_stack.front();
                    unsigned int new_size = m_edge_undo_stack.size() - i;
                    for ( ; i > 0; --i)
                    {
                        delete m_edge_undo_stack[new_size + i - 1].old_co_boundary;
                        delete m_edge_undo_stack[new_size + i - 1].old_boundary;
                    }
                    m_edge_undo_stack.resize(new_size);
                    m_edge_mark_stack.pop_front();
                }
                
                m_face_kernel->commit();
                if (!m_face_mark_stack.empty())
                {
                    unsigned int i = m_face_mark_stack.front();
                    unsigned int new_size = m_face_undo_stack.size() - i;
                    for ( ; i > 0; --i)
                    {
                        delete m_face_undo_stack[new_size + i - 1].old_co_boundary;
                        delete m_face_undo_stack[new_size + i - 1].old_boundary;
                    }
                    m_face_undo_stack.resize(new_size);
                    m_face_mark_stack.pop_front();
                }
                
                m_tetrahedron_kernel->commit();
                if (!m_tetrahedron_mark_stack.empty())
                {
                    unsigned int i = m_tetrahedron_mark_stack.front();
                    unsigned int new_size = m_tetrahedron_undo_stack.size() - i;
                    for ( ; i > 0; --i)
                    {
                        delete m_tetrahedron_undo_stack[new_size + i - 1].old_boundary;
                    }
                    m_tetrahedron_undo_stack.resize(new_size);
                    m_tetrahedron_mark_stack.pop_front();
                }
            }
            
            /**
             *
             */
            void commit_all()
            {
                m_node_kernel->commit_all();
                for (unsigned int i = 0; i < m_node_undo_stack.size(); ++i)
                {
                    delete m_node_undo_stack[i].old_co_boundary;
                }
                m_node_undo_stack.clear();
                m_node_mark_stack.clear();
                
                m_edge_kernel->commit_all();
                for (unsigned int i = 0; i < m_edge_undo_stack.size(); ++i)
                {
                    delete m_edge_undo_stack[i].old_boundary;
                    delete m_edge_undo_stack[i].old_co_boundary;
                }
                m_edge_undo_stack.clear();
                m_edge_mark_stack.clear();
                
                m_face_kernel->commit_all();
                for (unsigned int i = 0; i < m_face_undo_stack.size(); ++i)
                {
                    delete m_face_undo_stack[i].old_boundary;
                    delete m_face_undo_stack[i].old_co_boundary;
                }
                m_face_undo_stack.clear();
                m_face_mark_stack.clear();
                
                m_tetrahedron_kernel->commit_all();
                for (unsigned int i = 0; i < m_tetrahedron_undo_stack.size(); ++i)
                {
                    delete m_tetrahedron_undo_stack[i].old_boundary;
                }
                m_tetrahedron_undo_stack.clear();
                m_tetrahedron_mark_stack.clear();
            }
            
            /**
             * Inverts orientation of all tetrahedra in the mesh.
             */
            void invert_all()
            {
                typename tetrahedron_kernel_type::iterator tit = tetrahedra_begin();
                while (tit != tetrahedra_end())
                {
                    invert_orientation(tit.key());
                    orient_faces_consistently(tit.key());
                    ++tit;
                }
            }
            
        }; //class t4mesh
        
    } //namespace is_mesh
    
} //namespace OpenTissue

#endif //PROGRESSIVE_MESH_INCIDENCE_SIMPLICIAL_H