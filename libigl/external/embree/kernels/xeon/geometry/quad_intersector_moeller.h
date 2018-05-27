// ======================================================================== //
// Copyright 2009-2015 Intel Corporation                                    //
//                                                                          //
// Licensed under the Apache License, Version 2.0 (the "License");          //
// you may not use this file except in compliance with the License.         //
// You may obtain a copy of the License at                                  //
//                                                                          //
//     http://www.apache.org/licenses/LICENSE-2.0                           //
//                                                                          //
// Unless required by applicable law or agreed to in writing, software      //
// distributed under the License is distributed on an "AS IS" BASIS,        //
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. //
// See the License for the specific language governing permissions and      //
// limitations under the License.                                           //
// ======================================================================== //

#pragma once

#include "quadv.h"
#include "triangle_intersector_moeller.h"

namespace embree
{
  namespace isa
  {
    template<int M>
      struct QuadHitM 
      {
        __forceinline QuadHitM(const vbool<M>& valid,
                               const vfloat<M>& U, 
                               const vfloat<M>& V, 
                               const vfloat<M>& T, 
                               const vfloat<M>& absDen, 
                               const Vec3<vfloat<M>>& Ng, 
                               const vbool<M>& flags)
          : valid(valid), U(U), V(V), T(T), absDen(absDen), tri_Ng(Ng), flags(flags) {}
      
        __forceinline void finalize() 
        {
          const vfloat<M> rcpAbsDen = rcp(absDen);
          vt = T * rcpAbsDen;
          const vfloat<M> u = U * rcpAbsDen;
          const vfloat<M> v = V * rcpAbsDen;
          const vfloat<M> u1 = vfloat<M>(1.0f) - u;
          const vfloat<M> v1 = vfloat<M>(1.0f) - v;
#if !defined(__AVX__) // FIXME: incorrect for default template instantiation for QuadMIntersector1MoellerTrumbore
          vu = select(flags,u1,u); 
          vv = select(flags,v1,v);
          vNg = Vec3<vfloat<M>>(tri_Ng.x,tri_Ng.y,tri_Ng.z);
#else
          const vfloat<M> flip = select(flags,vfloat<M>(-1.0f),vfloat<M>(1.0f));
          vv = select(flags,u1,v);
          vu = select(flags,v1,u);
          vNg = Vec3<vfloat<M>>(flip*tri_Ng.x,flip*tri_Ng.y,flip*tri_Ng.z);
#endif
        }

        __forceinline Vec2f uv(const size_t i) 
        { 
          const float u = vu[i];
          const float v = vv[i];
          return Vec2f(u,v);
        }

        __forceinline float   t(const size_t i) { return vt[i]; }
        __forceinline Vec3fa Ng(const size_t i) { return Vec3fa(vNg.x[i],vNg.y[i],vNg.z[i]); }
      
      private:
        vfloat<M> U;
        vfloat<M> V;
        vfloat<M> T;
        vfloat<M> absDen;
        Vec3<vfloat<M>> tri_Ng;
      
      public:
        vbool<M> valid;
        vfloat<M> vu;
        vfloat<M> vv;
        vfloat<M> vt;
        Vec3<vfloat<M>> vNg;

      public:
        const vbool<M> flags;
      };

    template<int K>
      struct QuadHitK
      {
        __forceinline QuadHitK(const vfloat<K>& U, 
                               const vfloat<K>& V, 
                               const vfloat<K>& T, 
                               const vfloat<K>& absDen, 
                               const Vec3<vfloat<K>>& Ng, 
                               const vbool<K>& flags)
          : U(U), V(V), T(T), absDen(absDen), flags(flags), tri_Ng(Ng) {}
      
        __forceinline std::tuple<vfloat<K>,vfloat<K>,vfloat<K>,Vec3<vfloat<K>>> operator() () const
        {
          const vfloat<K> rcpAbsDen = rcp(absDen);
          const vfloat<K> t = T * rcpAbsDen;
          const vfloat<K> u0 = U * rcpAbsDen;
          const vfloat<K> v0 = V * rcpAbsDen;
          const vfloat<K> u1 = vfloat<K>(1.0f) - u0;
          const vfloat<K> v1 = vfloat<K>(1.0f) - v0;
          const vfloat<K> u = select(flags,u1,u0); 
          const vfloat<K> v = select(flags,v1,v0);
          const Vec3<vfloat<K>> Ng(tri_Ng.x,tri_Ng.y,tri_Ng.z);
          return std::make_tuple(u,v,t,Ng);
        }

      private:
        const vfloat<K> U;
        const vfloat<K> V;
        const vfloat<K> T;
        const vfloat<K> absDen;
        const vbool<K> flags;
        const Vec3<vfloat<K>> tri_Ng;      
      };

    /* ----------------------------- */
    /* -- single ray intersectors -- */
    /* ----------------------------- */

      struct MoellerTrumboreIntersectorTriangle1
      {
        template<int M, typename Epilog>
          static __forceinline bool intersect(Ray& ray, 
                                              const Vec3<vfloat<M>>& tri_v0, 
                                              const Vec3<vfloat<M>>& tri_e1, 
                                              const Vec3<vfloat<M>>& tri_e2, 
                                              const Vec3<vfloat<M>>& tri_Ng,
                                              const vbool<M>& flags,
                                              const Epilog& epilog)
        {
          /* calculate denominator */
          typedef Vec3<vfloat<M>> Vec3vfM;
          const Vec3vfM O = Vec3vfM(ray.org);
          const Vec3vfM D = Vec3vfM(ray.dir);
          const Vec3vfM C = Vec3vfM(tri_v0) - O;
          const Vec3vfM R = cross(D,C);
          const vfloat<M> den = dot(Vec3vfM(tri_Ng),D);
          const vfloat<M> absDen = abs(den);
          const vfloat<M> sgnDen = signmsk(den);
          
          /* perform edge tests */
          const vfloat<M> U = dot(R,Vec3vfM(tri_e2)) ^ sgnDen;
          const vfloat<M> V = dot(R,Vec3vfM(tri_e1)) ^ sgnDen;
          
          /* perform backface culling */
#if defined(RTCORE_BACKFACE_CULLING)
          vbool<M> valid = (den > vfloat<M>(zero)) & (U >= 0.0f) & (V >= 0.0f) & (U+V<=absDen);
#else
          vbool<M> valid = (den != vfloat<M>(zero)) & (U >= 0.0f) & (V >= 0.0f) & (U+V<=absDen);
#endif
          if (likely(none(valid))) return false;
          
          /* perform depth test */
          const vfloat<M> T = dot(Vec3vfM(tri_Ng),C) ^ sgnDen;
          valid &= (T > absDen*vfloat<M>(ray.tnear)) & (T < absDen*vfloat<M>(ray.tfar));
          if (likely(none(valid))) return false;
          
          /* update hit information */
          QuadHitM<M> hit(valid,U,V,T,absDen,tri_Ng, flags);
          return epilog(valid,hit);
        }
        
        template<int M, typename Epilog>
          static __forceinline bool intersect(Ray& ray, 
                                       const Vec3<vfloat<M>>& v0, 
                                       const Vec3<vfloat<M>>& v1, 
                                       const Vec3<vfloat<M>>& v2, 
                                       const vbool<M>& flags,
                                       const Epilog& epilog)
        {
          const Vec3<vfloat<M>> e1 = v0-v1;
          const Vec3<vfloat<M>> e2 = v2-v0;
          const Vec3<vfloat<M>> Ng = cross(e1,e2);
          return intersect(ray,v0,e1,e2,Ng,flags,epilog);
        }
      };

    template<int M, bool filter>
      struct QuadMIntersector1MoellerTrumbore;

    /*! Intersects M quads with 1 ray */
    template<int M, bool filter>
      struct QuadMIntersector1MoellerTrumbore
    {
      __forceinline QuadMIntersector1MoellerTrumbore(const Ray& ray, const void* ptr) {}

      __forceinline void intersect(Ray& ray, const Vec3<vfloat<M>>& v0, const Vec3<vfloat<M>>& v1, const Vec3<vfloat<M>>& v2, const Vec3<vfloat<M>>& v3, 
                                   const vint<M>& geomID, const vint<M>& primID, Scene* scene, const unsigned* geomID_to_instID) const
      {
        //Intersect1Epilog<M,M,filter> epilog(ray,geomID,primID,scene,geomID_to_instID);
        //MoellerTrumboreIntersectorTriangle1::intersect(ray,v0,v1,v3,vbool<M>(false),epilog);
        //MoellerTrumboreIntersectorTriangle1::intersect(ray,v2,v3,v1,vbool<M>(true ),epilog);

        MoellerTrumboreHitM<M> hit;
        MoellerTrumboreIntersector1<M> intersector(ray,nullptr);
        Intersect1Epilog<M,M,filter> epilog(ray,geomID,primID,scene,geomID_to_instID);

        /* intersect first triangle */
        if (intersector.intersect(ray,v0,v1,v3,hit)) 
          epilog(hit.valid,hit);

        /* intersect second triangle */
        if (intersector.intersect(ray,v2,v3,v1,hit)) 
        {
          hit.U = hit.absDen - hit.U;
          hit.V = hit.absDen - hit.V;
          epilog(hit.valid,hit);
        }
      }
      
      __forceinline bool occluded(Ray& ray, const Vec3<vfloat<M>>& v0, const Vec3<vfloat<M>>& v1, const Vec3<vfloat<M>>& v2, const Vec3<vfloat<M>>& v3, 
                                  const vint<M>& geomID, const vint<M>& primID, Scene* scene, const unsigned* geomID_to_instID) const
      {
        MoellerTrumboreHitM<M> hit;
        MoellerTrumboreIntersector1<M> intersector(ray,nullptr);
        Occluded1Epilog<M,M,filter> epilog(ray,geomID,primID,scene,geomID_to_instID);

        /* intersect first triangle */
        if (intersector.intersect(ray,v0,v1,v3,hit)) 
        {
          if (epilog(hit.valid,hit))
            return true;
        }

        /* intersect second triangle */
        if (intersector.intersect(ray,v2,v3,v1,hit)) 
        {
          hit.U = hit.absDen - hit.U;
          hit.V = hit.absDen - hit.V;
          if (epilog(hit.valid,hit))
            return true;
        }
        return false;
      }
    };

#if defined(__AVX512F__)

    /*! Intersects 4 quads with 1 ray using AVX512 */
    template<bool filter>
      struct QuadMIntersector1MoellerTrumbore<4,filter>
    {
      __forceinline QuadMIntersector1MoellerTrumbore(const Ray& ray, const void* ptr) {}

      template<typename Epilog>
        __forceinline bool intersect(Ray& ray, const Vec3vf4& v0, const Vec3vf4& v1, const Vec3vf4& v2, const Vec3vf4& v3, const Epilog& epilog) const
      {
        const Vec3vf16 vtx0(select(0x0f0f,vfloat16(v0.x),vfloat16(v2.x)),
                            select(0x0f0f,vfloat16(v0.y),vfloat16(v2.y)),
                            select(0x0f0f,vfloat16(v0.z),vfloat16(v2.z)));
        const Vec3vf16 vtx1(vfloat16(v1.x),vfloat16(v1.y),vfloat16(v1.z));
        const Vec3vf16 vtx2(vfloat16(v3.x),vfloat16(v3.y),vfloat16(v3.z));
        const vbool16 flags(0xf0f0);

        MoellerTrumboreHitM<16> hit;
        MoellerTrumboreIntersector1<16> intersector(ray,nullptr);
        if (unlikely(intersector.intersect(ray,vtx0,vtx1,vtx2,hit))) 
        {
          vfloat16 U = hit.U, V = hit.V, absDen = hit.absDen;
          hit.U = select(flags,absDen-V,U);
          hit.V = select(flags,absDen-U,V);
          hit.vNg *= select(flags,vfloat16(-1.0f),vfloat16(1.0f)); // FIXME: use XOR
          if (likely(epilog(hit.valid,hit)))
            return true;
        }
        return false;
      }
      
      __forceinline bool intersect(Ray& ray, const Vec3vf4& v0, const Vec3vf4& v1, const Vec3vf4& v2, const Vec3vf4& v3, 
                                   const vint4& geomID, const vint4& primID, Scene* scene, const unsigned* geomID_to_instID) const
      {
        return intersect(ray,v0,v1,v2,v3,Intersect1Epilog<8,16,filter>(ray,vint8(geomID),vint8(primID),scene,geomID_to_instID));
      }
      
      __forceinline bool occluded(Ray& ray, const Vec3vf4& v0, const Vec3vf4& v1, const Vec3vf4& v2, const Vec3vf4& v3, 
                                  const vint4& geomID, const vint4& primID, Scene* scene, const unsigned* geomID_to_instID) const
      {
        return intersect(ray,v0,v1,v2,v3,Occluded1Epilog<8,16,filter>(ray,vint8(geomID),vint8(primID),scene,geomID_to_instID));
      }
    };

#elif defined (__AVX__)

    /*! Intersects 4 quads with 1 ray using AVX */
    template<bool filter>
      struct QuadMIntersector1MoellerTrumbore<4,filter>
    {
      __forceinline QuadMIntersector1MoellerTrumbore(const Ray& ray, const void* ptr) {}
      
      template<typename Epilog>
        __forceinline bool intersect(Ray& ray, const Vec3vf4& v0, const Vec3vf4& v1, const Vec3vf4& v2, const Vec3vf4& v3, const Epilog& epilog) const
      {
        const Vec3vf8 vtx0(vfloat8(v0.x,v2.x),vfloat8(v0.y,v2.y),vfloat8(v0.z,v2.z));
        const Vec3vf8 vtx1(vfloat8(v1.x),vfloat8(v1.y),vfloat8(v1.z));
        const Vec3vf8 vtx2(vfloat8(v3.x),vfloat8(v3.y),vfloat8(v3.z));        
        MoellerTrumboreHitM<8> hit;
        MoellerTrumboreIntersector1<8> intersector(ray,nullptr);
        const vbool8 flags(0,0,0,0,1,1,1,1);
        if (unlikely(intersector.intersect(ray,vtx0,vtx1,vtx2,hit)))
        {
          vfloat8 U = hit.U, V = hit.V, absDen = hit.absDen;
          hit.U = select(flags,absDen-V,U);
          hit.V = select(flags,absDen-U,V);
          hit.vNg *= select(flags,vfloat8(-1.0f),vfloat8(1.0f)); // FIXME: use XOR
          if (unlikely(epilog(hit.valid,hit)))
            return true;
        }
        return false;
      }
      
      __forceinline bool intersect(Ray& ray, const Vec3vf4& v0, const Vec3vf4& v1, const Vec3vf4& v2, const Vec3vf4& v3, 
                                   const vint4& geomID, const vint4& primID, Scene* scene, const unsigned* geomID_to_instID) const
      {
        return intersect(ray,v0,v1,v2,v3,Intersect1Epilog<8,8,filter>(ray,vint8(geomID),vint8(primID),scene,geomID_to_instID));
      }
      
      __forceinline bool occluded(Ray& ray, const Vec3vf4& v0, const Vec3vf4& v1, const Vec3vf4& v2, const Vec3vf4& v3, 
                                  const vint4& geomID, const vint4& primID, Scene* scene, const unsigned* geomID_to_instID) const
      {
        return intersect(ray,v0,v1,v2,v3,Occluded1Epilog<8,8,filter>(ray,vint8(geomID),vint8(primID),scene,geomID_to_instID));
      }
    };

#endif

    /* ----------------------------- */
    /* -- ray packet intersectors -- */
    /* ----------------------------- */


    struct MoellerTrumboreIntersector1KTriangleM
    {
      /*! Intersect k'th ray from ray packet of size K with M triangles. */
      template<int M, int K, typename Epilog>
       static  __forceinline bool intersect(RayK<K>& ray, 
                                     size_t k,
                                     const Vec3<vfloat<M>>& tri_v0, 
                                     const Vec3<vfloat<M>>& tri_e1, 
                                     const Vec3<vfloat<M>>& tri_e2, 
                                     const Vec3<vfloat<M>>& tri_Ng,
                                     const vbool<M>& flags,                                     
                                     const Epilog& epilog)
      {
        /* calculate denominator */
        typedef Vec3<vfloat<M>> Vec3vfM;
        const Vec3vfM O = broadcast<vfloat<M>>(ray.org,k);
        const Vec3vfM D = broadcast<vfloat<M>>(ray.dir,k);
        const Vec3vfM C = Vec3vfM(tri_v0) - O;
        const Vec3vfM R = cross(D,C);
        const vfloat<M> den = dot(Vec3vfM(tri_Ng),D);
        const vfloat<M> absDen = abs(den);
        const vfloat<M> sgnDen = signmsk(den);
        
        /* perform edge tests */
        const vfloat<M> U = dot(R,Vec3vfM(tri_e2)) ^ sgnDen;
        const vfloat<M> V = dot(R,Vec3vfM(tri_e1)) ^ sgnDen;
        
        /* perform backface culling */
#if defined(RTCORE_BACKFACE_CULLING)
        vbool<M> valid = (den > vfloat<M>(zero)) & (U >= 0.0f) & (V >= 0.0f) & (U+V<=absDen);
#else
        vbool<M> valid = (den != vfloat<M>(zero)) & (U >= 0.0f) & (V >= 0.0f) & (U+V<=absDen);
#endif
        if (likely(none(valid))) return false;
        
        /* perform depth test */
        const vfloat<M> T = dot(Vec3vfM(tri_Ng),C) ^ sgnDen;
        valid &= (T > absDen*vfloat<M>(ray.tnear[k])) & (T < absDen*vfloat<M>(ray.tfar[k]));
        if (likely(none(valid))) return false;
        
        /* calculate hit information */
        QuadHitM<M> hit(valid,U,V,T,absDen,tri_Ng,flags);
        return epilog(valid,hit);
      }
      
      template<int M, int K, typename Epilog>
        static __forceinline bool intersect1(RayK<K>& ray, 
                                    size_t k,
                                    const Vec3<vfloat<M>>& v0, 
                                    const Vec3<vfloat<M>>& v1, 
                                    const Vec3<vfloat<M>>& v2, 
                                    const vbool<M>& flags,
                                    const Epilog& epilog)
      {
        const Vec3<vfloat<M>> e1 = v0-v1;
        const Vec3<vfloat<M>> e2 = v2-v0;
        const Vec3<vfloat<M>> Ng = cross(e1,e2);
        return intersect(ray,k,v0,e1,e2,Ng,flags,epilog);
      }
    };

    template<int M, int K, bool filter>
    struct QuadMIntersectorKMoellerTrumboreBase
    {
      __forceinline QuadMIntersectorKMoellerTrumboreBase(const vbool<K>& valid, const RayK<K>& ray) {}
            
      /*! Intersects K rays with one of M triangles. */
      template<typename Epilog>
        __forceinline vbool<K> intersectK(const vbool<K>& valid0, 
                                          RayK<K>& ray, 
                                          const Vec3<vfloat<K>>& tri_v0, 
                                          const Vec3<vfloat<K>>& tri_e1, 
                                          const Vec3<vfloat<K>>& tri_e2, 
                                          const Vec3<vfloat<K>>& tri_Ng, 
                                          const vbool<K>& flags,
                                          const Epilog& epilog) const
      {
        /* type shortcuts */
        typedef Vec3<vfloat<K>> Vec3vfK;
        
        /* calculate denominator */
        vbool<K> valid = valid0;
        const Vec3vfK C = tri_v0 - ray.org;
        const Vec3vfK R = cross(ray.dir,C);
        const vfloat<K> den = dot(tri_Ng,ray.dir);
        const vfloat<K> absDen = abs(den);
        const vfloat<K> sgnDen = signmsk(den);
        
        /* test against edge p2 p0 */
        const vfloat<K> U = dot(R,tri_e2) ^ sgnDen;
        valid &= U >= 0.0f;
        if (likely(none(valid))) return false;
        
        /* test against edge p0 p1 */
        const vfloat<K> V = dot(R,tri_e1) ^ sgnDen;
        valid &= V >= 0.0f;
        if (likely(none(valid))) return false;
        
        /* test against edge p1 p2 */
        const vfloat<K> W = absDen-U-V;
        valid &= W >= 0.0f;
        if (likely(none(valid))) return false;
        
        /* perform depth test */
        const vfloat<K> T = dot(tri_Ng,C) ^ sgnDen;
        valid &= (T >= absDen*ray.tnear) & (absDen*ray.tfar >= T);
        if (unlikely(none(valid))) return false;
        
        /* perform backface culling */
#if defined(RTCORE_BACKFACE_CULLING)
        valid &= den > vfloat<K>(zero);
        if (unlikely(none(valid))) return false;
#else
        valid &= den != vfloat<K>(zero);
        if (unlikely(none(valid))) return false;
#endif
        
        /* calculate hit information */
        QuadHitK<K> hit(U,V,T,absDen,tri_Ng,flags);
        return epilog(valid,hit);
      }
      
      /*! Intersects K rays with one of M quads. */
      template<typename Epilog>
      __forceinline vbool<K> intersectK(const vbool<K>& valid0, 
                                        RayK<K>& ray, 
                                        const Vec3<vfloat<K>>& tri_v0, 
                                        const Vec3<vfloat<K>>& tri_v1, 
                                        const Vec3<vfloat<K>>& tri_v2, 
                                        const vbool<K>& flags,
                                        const Epilog& epilog) const
      {
        typedef Vec3<vfloat<K>> Vec3vfK;
        const Vec3vfK e1 = tri_v0-tri_v1;
        const Vec3vfK e2 = tri_v2-tri_v0;
        const Vec3vfK Ng = cross(e1,e2);
        return intersectK(valid0,ray,tri_v0,e1,e2,Ng,flags,epilog);
      }

      /*! Intersects K rays with one of M quads. */
      template<typename Epilog>
      __forceinline bool intersectK(const vbool<K>& valid0, 
                                    RayK<K>& ray, 
                                    const Vec3<vfloat<K>>& v0, 
                                    const Vec3<vfloat<K>>& v1, 
                                    const Vec3<vfloat<K>>& v2, 
                                    const Vec3<vfloat<K>>& v3, 
                                    const Epilog& epilog) const
      {
        intersectK(valid0,ray,v0,v1,v3,vbool<K>(false),epilog);
        if (none(valid0)) return true;
        intersectK(valid0,ray,v2,v3,v1,vbool<K>(true ),epilog);
        return none(valid0);
      }
    };

    template<int M, int K, bool filter>
      struct QuadMIntersectorKMoellerTrumbore : public QuadMIntersectorKMoellerTrumboreBase<M,K,filter>
    {
      __forceinline QuadMIntersectorKMoellerTrumbore(const vbool<K>& valid, const RayK<K>& ray)
        : QuadMIntersectorKMoellerTrumboreBase<M,K,filter>(valid,ray) {}

      __forceinline void intersect1(RayK<K>& ray, size_t k, const Vec3<vfloat<M>>& v0, const Vec3<vfloat<M>>& v1, const Vec3<vfloat<M>>& v2, const Vec3<vfloat<M>>& v3, 
                                    const vint<M>& geomID, const vint<M>& primID, Scene* scene) const
      {
        Intersect1KEpilog<M,M,K,filter> epilog(ray,k,geomID,primID,scene);
        MoellerTrumboreIntersector1KTriangleM::intersect1(ray,k,v0,v1,v3,vbool<M>(false),epilog);
        MoellerTrumboreIntersector1KTriangleM::intersect1(ray,k,v2,v3,v1,vbool<M>(true ),epilog);
      }
      
      __forceinline bool occluded1(RayK<K>& ray, size_t k, const Vec3<vfloat<M>>& v0, const Vec3<vfloat<M>>& v1, const Vec3<vfloat<M>>& v2, const Vec3<vfloat<M>>& v3, 
                                   const vint<M>& geomID, const vint<M>& primID, Scene* scene) const
      {
        Occluded1KEpilog<M,M,K,filter> epilog(ray,k,geomID,primID,scene);
        if (MoellerTrumboreIntersector1KTriangleM::intersect1(ray,k,v0,v1,v3,vbool<M>(false),epilog)) return true;
        if (MoellerTrumboreIntersector1KTriangleM::intersect1(ray,k,v2,v3,v1,vbool<M>(true ),epilog)) return true;
        return false;
      }
    };


#if defined(__AVX512F__)

    /*! Intersects 4 quads with 1 ray using AVX512 */
    template<int K, bool filter>
      struct QuadMIntersectorKMoellerTrumbore<4,K,filter> : public QuadMIntersectorKMoellerTrumboreBase<4,K,filter>
    {
      __forceinline QuadMIntersectorKMoellerTrumbore(const vbool<K>& valid, const RayK<K>& ray)
        : QuadMIntersectorKMoellerTrumboreBase<4,K,filter>(valid,ray) {}

      template<typename Epilog>
        __forceinline bool intersect1(RayK<K>& ray, size_t k, const Vec3vf4& v0, const Vec3vf4& v1, const Vec3vf4& v2, const Vec3vf4& v3, const Epilog& epilog) const
      {
        const Vec3vf16 vtx0(select(0x0f0f,vfloat16(v0.x),vfloat16(v2.x)),
                            select(0x0f0f,vfloat16(v0.y),vfloat16(v2.y)),
                            select(0x0f0f,vfloat16(v0.z),vfloat16(v2.z)));
        const Vec3vf16 vtx1(vfloat16(v1.x),vfloat16(v1.y),vfloat16(v1.z));
        const Vec3vf16 vtx2(vfloat16(v3.x),vfloat16(v3.y),vfloat16(v3.z));
        const vbool16 flags(0xf0f0);
        return MoellerTrumboreIntersector1KTriangleM::intersect1(ray,k,vtx0,vtx1,vtx2,flags,epilog);
      }
      
      __forceinline bool intersect1(RayK<K>& ray, size_t k, const Vec3vf4& v0, const Vec3vf4& v1, const Vec3vf4& v2, const Vec3vf4& v3, 
                                   const vint4& geomID, const vint4& primID, Scene* scene) const
      {
        return intersect1(ray,k,v0,v1,v2,v3,Intersect1KEpilog<8,16,K,filter>(ray,k,vint8(geomID),vint8(primID),scene));
      }
      
      __forceinline bool occluded1(RayK<K>& ray, size_t k, const Vec3vf4& v0, const Vec3vf4& v1, const Vec3vf4& v2, const Vec3vf4& v3, 
                                  const vint4& geomID, const vint4& primID, Scene* scene) const
      {
        return intersect1(ray,k,v0,v1,v2,v3,Occluded1KEpilog<8,16,K,filter>(ray,k,vint8(geomID),vint8(primID),scene));
      }
    };

#elif defined (__AVX__)

    /*! Intersects 4 quads with 1 ray using AVX */
     template<int K, bool filter>
       struct QuadMIntersectorKMoellerTrumbore<4,K,filter> : public QuadMIntersectorKMoellerTrumboreBase<4,K,filter>
    {
      __forceinline QuadMIntersectorKMoellerTrumbore(const vbool<K>& valid, const RayK<K>& ray)
        : QuadMIntersectorKMoellerTrumboreBase<4,K,filter>(valid,ray) {}
      
      template<typename Epilog>
        __forceinline bool intersect1(RayK<K>& ray, size_t k, const Vec3vf4& v0, const Vec3vf4& v1, const Vec3vf4& v2, const Vec3vf4& v3, const Epilog& epilog) const
      {
        const Vec3vf8 vtx0(vfloat8(v0.x,v2.x),vfloat8(v0.y,v2.y),vfloat8(v0.z,v2.z));
        const Vec3vf8 vtx1(vfloat8(v1.x),vfloat8(v1.y),vfloat8(v1.z));
        const Vec3vf8 vtx2(vfloat8(v3.x),vfloat8(v3.y),vfloat8(v3.z));
        const vbool8 flags(0,0,0,0,1,1,1,1);
        return MoellerTrumboreIntersector1KTriangleM::intersect1(ray,k,vtx0,vtx1,vtx2,flags,epilog); 
      }
      
      __forceinline bool intersect1(RayK<K>& ray, size_t k, const Vec3vf4& v0, const Vec3vf4& v1, const Vec3vf4& v2, const Vec3vf4& v3, 
                                   const vint4& geomID, const vint4& primID, Scene* scene) const
      {
        return intersect1(ray,k,v0,v1,v2,v3,Intersect1KEpilog<8,8,K,filter>(ray,k,vint8(geomID),vint8(primID),scene));
      }
      
      __forceinline bool occluded1(RayK<K>& ray, size_t k, const Vec3vf4& v0, const Vec3vf4& v1, const Vec3vf4& v2, const Vec3vf4& v3, 
                                  const vint4& geomID, const vint4& primID, Scene* scene) const
      {
        return intersect1(ray,k,v0,v1,v2,v3,Occluded1KEpilog<8,8,K,filter>(ray,k,vint8(geomID),vint8(primID),scene));
      }
    };

#endif
  }
}

