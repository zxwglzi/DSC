#pragma once

#include <set>
#include <is_mesh/key.h>

namespace is_mesh
{
    ///////////////////////////////////////////////////////////////////////////////
    // S I M P L E X   B A S E   C L A S S
    ///////////////////////////////////////////////////////////////////////////////
    /**
     * Base class for all simplex classes
     */
    template<typename boundary_key_type, typename co_boundary_key_type>
    class Simplex
    {
    public:
        typedef         std::vector<boundary_key_type>          boundary_list;
        typedef         std::set<co_boundary_key_type>          co_boundary_list;
        
    protected:
        boundary_list* m_boundary = nullptr;
        co_boundary_list* m_co_boundary = nullptr;
        bool             		m_is_compact;
        int              		m_label;       //used in coloring - to identify connected components.
        
    public:
        
        Simplex() : m_is_compact(false), m_label(0)
        {
            m_boundary    = new boundary_list();
            m_co_boundary = new co_boundary_list();
        }
        
        ~Simplex()
        {
            if(m_boundary)
            {
                delete m_boundary;
            }
            if(m_co_boundary)
            {
                delete m_co_boundary;
            }
        }
        
        void set_label(const int& l)
        {
            m_label = l;
        }
        
        int get_label()
        {
            return m_label;
        }
        
        void reset_label()
        {
            m_label = 0;
        }
        
        //copy constructor - needed because simplices are stored in STL-like containers, ie. the kernel.
        Simplex(const Simplex& s) //: m_co_boundary(0), m_boundary(0)
        {
            m_is_compact = s.m_is_compact;
            m_label      = s.m_label;
            m_boundary   = 0;
            m_co_boundary= 0;
            if (s.m_boundary != 0)
            {
                m_boundary = new std::vector<boundary_key_type>();
                std::copy(s.m_boundary->begin(), s.m_boundary->end(), m_boundary->begin());
            }
            if (s.m_co_boundary != 0)
            {
                m_co_boundary = new co_boundary_list();
                for (auto &sb : *s.m_co_boundary)
                {
                    m_co_boundary->insert(sb);
                }
                //std::copy(s.m_co_boundary->begin(), s.m_co_boundary->end(), m_co_boundary->begin());
            }
        }
        
    public:
        
        co_boundary_list* get_co_boundary() const
        {
            return m_co_boundary;
        }
        boundary_list* get_boundary() const
        {
            return m_boundary;
        }
        co_boundary_list* get_co_boundary()
        {
            return m_co_boundary;
        }
        boundary_list* get_boundary()
        {
            return m_boundary;
        }
        
        void add_co_face(co_boundary_key_type n)
        {
            m_co_boundary->insert(n);
        }
        
        void add_face(boundary_key_type n)
        {
            m_boundary->push_back(n);
        }
        
        void remove_co_face(co_boundary_key_type const & n)
        {
            m_co_boundary->erase(n);
        }
        
        void remove_face(boundary_key_type const & n)
        {
            m_boundary->erase(n);
        }
        
        bool is_compact(){ return m_is_compact; }
        void set_compact(bool c) { m_is_compact = c; }
    };
    
    ///////////////////////////////////////////////////////////////////////////////
    ///  N O D E
    ///////////////////////////////////////////////////////////////////////////////
    template<typename NodeTraits, typename Mesh>
    class Node : public NodeTraits, public Simplex<Key, EdgeKey>
    {
    public:
        typedef NodeTraits  type_traits;
        
        Node() : Simplex<Key, EdgeKey>()
        {
            
        }
        Node(const type_traits & t) : type_traits(t), Simplex<Key, EdgeKey>()
        {
            
        }
    };
    
    ///////////////////////////////////////////////////////////////////////////////
    ///  E D G E
    ///////////////////////////////////////////////////////////////////////////////
    template<typename EdgeTraits, typename Mesh>
    class Edge : public EdgeTraits, public Simplex<NodeKey, FaceKey>
    {
    public:
        typedef EdgeTraits type_traits;
        
        Edge() : Simplex<NodeKey, FaceKey>()
        {
            
        }
        Edge(const type_traits & t) : type_traits(t), Simplex<NodeKey, FaceKey>()
        {
            
        }
    };
    
    ///////////////////////////////////////////////////////////////////////////////
    //  F A C E
    ///////////////////////////////////////////////////////////////////////////////
    template<typename FaceTraits, typename Mesh>
    class Face : public FaceTraits, public Simplex<EdgeKey, TetrahedronKey>
    {
    public:
        typedef FaceTraits type_traits;
        
        Face() : Simplex<EdgeKey, TetrahedronKey>()
        {
            Simplex::set_compact(true);
        }
        Face(const type_traits & t) : type_traits(t), Simplex<EdgeKey, TetrahedronKey>()
        {
            Simplex::set_compact(true);
        }
    };
    
    ///////////////////////////////////////////////////////////////////////////////
    // T E T R A H E D R O N
    ///////////////////////////////////////////////////////////////////////////////
    template<typename TetrahedronTraits, typename Mesh>
    class Tetrahedron : public TetrahedronTraits, public Simplex<FaceKey, Key>
    {
    public:
        typedef TetrahedronTraits  type_traits;
        
        Tetrahedron() : Simplex<FaceKey, Key>()
        {
            Simplex::set_compact(true);
        }
        Tetrahedron(const type_traits & t) : type_traits(t), Simplex<FaceKey, Key>()
        {
            Simplex::set_compact(true);
        }
    };
}