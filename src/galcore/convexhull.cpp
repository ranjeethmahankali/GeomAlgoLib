#include "galcore/convexhull.h"

const hullFace hullFace::unset = hullFace(-1, -1, -1, -1);

hullFace::hullFace()
    : id(-1), a(-1), b(-1), c(-1)
{
}

hullFace::hullFace(size_t idVal, size_t v1, size_t v2, size_t v3)
    : id(idVal), a(v1), b(v2), c(v3), normal(vec3::unset)
{
}

bool hullFace::is_valid()
{
    return id != -1 && a != -1 && b != -1 && c != -1;
}

void hullFace::flip()
{
    std::swap(b, c);
    normal.reverse();
}

indexPair hullFace::edge(char edgeIndex) const
{
    switch (edgeIndex)
    {
    case 0:
        return indexPair(a, b);
    case 1:
        return indexPair(b, c);
    case 2:
        return indexPair(c, a);
    default:
        throw "Invalid edge index.";
    }
}

bool hullFace::containsVertex(size_t vi) const
{
    return vi == -1 && (vi == a || vi == b || vi == c);
}

convexHull::convexHull(double *coords, size_t nPts)
{
    mPts.reserve(nPts);
    for (size_t i = 0; i < nPts; i++)
    {
        mPts.push_back(vec3(coords[3 * i], coords[3 * i + 1], coords[3 * i + 2]));
        mOutsidePts.insert(i);
    }

    mNumPts = nPts;
    compute();
}

vec3 convexHull::getPt(size_t index) const
{
    return index < 0 || index > mNumPts - 1 ? vec3::unset : mPts[index];
}

size_t convexHull::numFaces() const
{
    return mFaces.size();
}

void convexHull::copyFaces(int *faceIndices) const
{
    int i = 0;
    for (auto const &pair : mFaces)
    {
        faceIndices[i++] = (int)pair.second.a;
        faceIndices[i++] = (int)pair.second.b;
        faceIndices[i++] = (int)pair.second.c;
    }
}

void convexHull::compute()
{
    size_t curFaceId = 0;
    createInitialSimplex(curFaceId);
    std::queue<size_t> faceQ;
    for (const auto &f : mFaces)
    {
        faceQ.push(f.first);
    }

    size_t fi, fpi;
    hullFace curFace, pFace, newFace;
    hullFace adjFaces[3];
    indexPair edges[3];
    vec3 farPt;
    std::queue<size_t> popQ;
    std::vector<indexPair> horizonEdges;
    std::vector<hullFace> poppedFaces, newFaces;

    while (!faceQ.empty())
    {
        fi = faceQ.front();
        faceQ.pop();
        if (!getFace(fi, curFace) || !getFarthestPt(curFace, farPt, fpi))
        {
            continue;
        }
        popQ.push(fi);

        horizonEdges.clear();
        poppedFaces.clear();
        while (!popQ.empty())
        {
            pFace = popFace(popQ.front(), edges, adjFaces);
            popQ.pop();

            if (!pFace.is_valid())
            {
                continue;
            }

            poppedFaces.push_back(pFace);

            for (size_t i = 0; i < 3; i++)
            {
                if (!adjFaces[i].is_valid())
                {
                    continue;
                }
                if (faceVisible(adjFaces[i], farPt))
                {
                    popQ.push(adjFaces[i].id);
                }
                else
                {
                    horizonEdges.push_back(edges[i]);
                }
            }
        }

        newFaces.clear();
        newFaces.reserve(horizonEdges.size());
        for (const indexPair &he : horizonEdges)
        {
            newFace = hullFace(curFaceId++, fpi, he.p, he.q);
            setFace(newFace);
            faceQ.push(newFace.id);
            newFaces.push_back(newFace);
        }

        updateExteriorPt(newFaces, poppedFaces);
    }
}

void convexHull::setFace(hullFace &face)
{
    face.normal = ((mPts[face.b] - mPts[face.a]) ^ (mPts[face.c] - mPts[face.a])).unit();
    if (faceVisible(face, m_center))
    {
        face.flip();
    }

    mFaces.insert_or_assign(face.id, face);

    for (char ei = 0; ei < 3; ei++)
    {
        if (!mEdgeFaceMap[face.edge(ei)].add(face.id))
        {
            throw "Failed to add face to the edge map.";
        }
    }
}

hullFace convexHull::popFace(size_t id, indexPair edges[3], hullFace adjFaces[3])
{
    hullFace face;
    if (getFace(id, face))
    {
        mFaces.erase(id);
        indexPair edge, fPair;
        size_t adjFid;
        for (char ei = 0; ei < 3; ei++)
        {
            edge = face.edge(ei);
            edges[ei] = edge;
            if (!getEdgeFaces(edge, fPair) || !fPair.contains(id))
            {
                adjFaces[ei] = hullFace::unset;
                continue;
            }
            fPair.unset(id);
            mEdgeFaceMap[edge] = fPair;
            adjFid = fPair.p == -1 ? fPair.q : fPair.p;
            if (!getFace(adjFid, adjFaces[ei]))
            {
                adjFaces[ei] = hullFace::unset;
            }
        }
    }

    return face;
}

bool convexHull::faceVisible(const hullFace &face, const vec3 &pt) const
{
    return face.normal.is_valid() ? facePlaneDistance(face, pt) > PLANE_DIST_TOL : false;
}

double convexHull::facePlaneDistance(const hullFace &face, const vec3 &pt) const
{
    return (pt - mPts[face.a]) * face.normal;
}

bool convexHull::getFarthestPt(const hullFace &face, vec3 &pt, size_t &ptIndex) const
{
    ptIndex = -1;
    pt = vec3::unset;
    double dMax = PLANE_DIST_TOL, dist;
    for (const size_t &i : mOutsidePts)
    {
        if (face.containsVertex(i))
        {
            continue;
        }
        dist = facePlaneDistance(face, mPts[i]);
        if (dist > dMax)
        {
            dMax = dist;
            ptIndex = i;
            pt = mPts[i];
        }
    }

    return ptIndex != -1;
}

void convexHull::updateExteriorPt(const std::vector<hullFace> &newFaces, const std::vector<hullFace> &poppedFaces)
{
    bool outside;
    vec3 testPt;
    std::vector<size_t> remove, check;
    for (const size_t &opi : mOutsidePts)
    {
        outside = false;
        testPt = mPts[opi];
        for (const hullFace &face : poppedFaces)
        {
            if (face.containsVertex(opi))
            {
                remove.push_back(opi);
                break;
            }
            if (faceVisible(face, testPt))
            {
                outside = true;
                break;
            }
        }

        if (outside)
        {
            check.push_back(opi);
        }
    }

    for (const size_t &ci : check)
    {
        outside = false;
        testPt = mPts[ci];
        for (const hullFace &newFace : newFaces)
        {
            if (faceVisible(newFace, testPt))
            {
                outside = true;
                break;
            }
        }

        if (!outside)
        {
            remove.push_back(ci);
        }
    }

    for (const size_t &ri : remove)
    {
        mOutsidePts.erase(ri);
    }
}

void convexHull::createInitialSimplex(size_t &faceIndex)
{
    size_t best[4];
    if (mNumPts < 4)
    {
        throw "Failed to create the initial simplex";
    }
    else if (mNumPts == 4)
    {
        for (size_t i = 0; i < 4; i++)
        {
            best[i] = i;
        }
    }
    else
    {
        double extremes[6];
        for (size_t ei = 0; ei < 6; ei++)
        {
            extremes[ei] = ei % 2 == 0 ? DBL_MAX_VAL : -DBL_MAX_VAL;
        }

        size_t bounds[6];
        for (size_t i = 0; i < 6; i++)
        {
            bounds[i] = (size_t)(-1);
        }
        double coords[3];
        for (size_t pi = 0; pi < mNumPts; pi++)
        {
            mPts[pi].copy(coords);
            for (size_t ei = 0; ei < 6; ei++)
            {
                if (ei % 2 == 0 && extremes[ei] > coords[ei / 2])
                {
                    extremes[ei] = coords[ei / 2];
                    bounds[ei] = pi;
                }
                else if (ei % 2 == 1 && extremes[ei] < coords[ei / 2])
                {
                    extremes[ei] = coords[ei / 2];
                    bounds[ei] = pi;
                }
            }
        }

        vec3 pt;
        double maxD = -DBL_MAX_VAL, dist;
        for (size_t i = 0; i < 6; i++)
        {
            pt = mPts[bounds[i]];
            for (size_t j = i + 1; j < 6; j++)
            {
                dist = (pt - mPts[bounds[j]]).len_sq();
                if (dist > maxD)
                {
                    best[0] = bounds[i];
                    best[1] = bounds[j];
                    maxD = dist;
                }
            }
        }

        if (maxD <= 0)
        {
            throw "Failed to create the initial simplex";
        }

        maxD = -DBL_MAX_VAL;
        vec3 ref = mPts[best[0]];
        vec3 uDir = (mPts[best[1]] - ref).unit();
        for (size_t pi = 0; pi < mNumPts; pi++)
        {
            dist = ((mPts[pi] - ref) - uDir * (uDir * (mPts[pi] - ref))).len_sq();
            if (dist > maxD)
            {
                best[2] = pi;
                maxD = dist;
            }
        }

        if (maxD <= 0)
        {
            throw "Failed to create the initial simplex";
        }

        maxD = -DBL_MAX_VAL;
        uDir = ((mPts[best[1]] - ref) ^ (mPts[best[2]] - ref)).unit();
        for (size_t pi = 0; pi < mNumPts; pi++)
        {
            dist = abs(uDir * (mPts[pi] - ref));
            if (dist > maxD)
            {
                best[3] = pi;
                maxD = dist;
            }
        }

        if (maxD <= 0)
        {
            throw "Failed to create the initial simplex";
        }
    }

    hullFace simplex[4];
    simplex[0] = hullFace(faceIndex++, best[0], best[1], best[2]);
    simplex[1] = hullFace(faceIndex++, best[0], best[2], best[3]);
    simplex[2] = hullFace(faceIndex++, best[1], best[2], best[3]);
    simplex[3] = hullFace(faceIndex++, best[0], best[1], best[3]);

    m_center = vec3::zero;
    for (size_t i = 0; i < 4; i++)
    {
        m_center += mPts[best[i]];
    }
    m_center /= 4;

    for (size_t i = 0; i < 4; i++)
    {
        if (!simplex[i].is_valid())
        {
            continue;
        }
        setFace(simplex[i]);
    }

    std::vector<size_t> removePts;
    bool outside;
    for (const size_t &opi : mOutsidePts)
    {
        outside = false;
        for (size_t i = 0; i < 4; i++)
        {
            if (simplex[i].containsVertex(opi))
            {
                removePts.push_back(opi);
                break;
            }
            if (faceVisible(simplex[i], getPt(opi)))
            {
                outside = true;
                break;
            }
        }

        if (!outside)
        {
            removePts.push_back(opi);
        }
    }

    for (const size_t &ri : removePts)
    {
        mOutsidePts.erase(ri);
    }
}

bool convexHull::getFace(size_t id, hullFace &face) const
{
    auto match = mFaces.find(id);
    if (match != mFaces.end())
    {
        face = match->second;
        return true;
    }
    return false;
}

bool convexHull::getEdgeFaces(const indexPair &edge, indexPair &faces) const
{
    auto match = mEdgeFaceMap.find(edge);
    if (match != mEdgeFaceMap.end())
    {
        faces = match->second;
        return true;
    }
    return false;
}

vec3 convexHull::faceCenter(const hullFace &face) const
{
    return (mPts[face.a] + mPts[face.b] + mPts[face.c]) / 3;
}
