#include <THMPGSpatialHashing/THMPGHashTable.h>

using namespace sofa::component::collision::geometry;

namespace
{
    bool testIntersection(const Cube& cube1, const Cube& cube2, const SReal alarmDist)
    {
        const auto& minVect1 = cube1.minVect();
        const auto& minVect2 = cube2.minVect();
        const auto& maxVect1 = cube1.maxVect();
        const auto& maxVect2 = cube2.maxVect();

        bool res = (minVect1[0] > maxVect2[0] + alarmDist ||
            minVect2[0] > maxVect1[0] + alarmDist ||
            minVect1[1] > maxVect2[1] + alarmDist ||
            minVect2[1] > maxVect1[1] + alarmDist ||
            minVect1[2] > maxVect2[2] + alarmDist ||
            minVect2[2] > maxVect1[2] + alarmDist);

        return !res;
    }
}

namespace sofa::component::collision
{

SReal THMPGHashTable::cell_size = (SReal)(0);
SReal THMPGHashTable::_alarmDist = (SReal)(0);
SReal THMPGHashTable::_alarmDistd2 = (SReal)(0);


void THMPGHashTable::init(int hashTableSize,core::CollisionModel *cm, int timeStamp){
    _cm = cm->getLast();
    resize(0);
    resize(hashTableSize);

    refresh(timeStamp);
}

void THMPGHashTable::refresh(int timeStamp){
    if(_timeStamp >= timeStamp)
        return;

    _timeStamp = timeStamp;

    sofa::component::collision::geometry::CubeCollisionModel* cube_model = dynamic_cast<sofa::component::collision::geometry::CubeCollisionModel*>(_cm->getPrevious());

    long int nb_added_elems = 0;
    int mincell[3];
    int maxcell[3];
    int movingcell[3];
    //SReal alarmDistd2 = intersectionMethod->getAlarmDistance()/((SReal)(2.0));

    //sofa::helper::AdvancedTimer::stepBegin("THMPGSpatialHashing : Hashing");

    Cube c(cube_model);

    for(;c.getIndex() < cube_model->getSize() ; ++c){
        ++nb_added_elems;
        const type::Vector3 & minVec = c.minVect();

        mincell[0] = static_cast<int>(std::floor((minVec[0] - _alarmDistd2)/cell_size));
        mincell[1] = static_cast<int>(std::floor((minVec[1] - _alarmDistd2)/cell_size));
        mincell[2] = static_cast<int>(std::floor((minVec[2] - _alarmDistd2)/cell_size));

        const type::Vector3 & maxVec = c.maxVect();
        maxcell[0] = static_cast<int>(std::floor((maxVec[0] + _alarmDistd2)/cell_size));
        maxcell[1] = static_cast<int>(std::floor((maxVec[1] + _alarmDistd2)/cell_size));
        maxcell[2] = static_cast<int>(std::floor((maxVec[2] + _alarmDistd2)/cell_size));

        for(movingcell[0] = mincell[0] ; movingcell[0] <= maxcell[0] ; ++movingcell[0]){
            for(movingcell[1] = mincell[1] ; movingcell[1] <= maxcell[1] ; ++movingcell[1]){
                for(movingcell[2] = mincell[2] ; movingcell[2] <= maxcell[2] ; ++movingcell[2]){
                    //sofa::helper::AdvancedTimer::stepBegin("THMPGSpatialHashing : addAndCollide");
                    auto& current = (*this)(movingcell[0], movingcell[1], movingcell[2]);
                    current.add(c/*.getExternalChildren().first*/, timeStamp);
                   //(*this)(movingcell[0],movingcell[1],movingcell[2]).add(c/*.getExternalChildren().first*/,timeStamp);
                    //sofa::helper::AdvancedTimer::stepEnd("THMPGSpatialHashing : addAndCollide");
                }
            }
        }
    }
}

static bool checkIfCollisionIsDone(const std::vector<int>& tab, int j){
    for(unsigned int ii = 0 ; ii < tab.size() ; ++ii){
        if(tab[ii] == j)
            return true;
    }

    return false;
}

void THMPGHashTable::doCollision(THMPGHashTable & me,THMPGHashTable & other,sofa::core::collision::NarrowPhaseDetection * phase, int timeStamp,core::collision::ElementIntersector* ei,bool swap){
    sofa::core::CollisionModel* cm1,*cm2;
    cm1 = me.getCollisionModel();
    cm2 = other.getCollisionModel();

    assert(me._prime_size == other._prime_size);

    std::vector<std::vector<int>> done_collisions;

    if(swap){
        done_collisions.resize(cm2->getSize());
        core::collision::DetectionOutputVector*& output = phase->getDetectionOutputs(cm2,cm1);
        ei->beginIntersect(cm2,cm1,output);

        for(int i = 0 ; i < me._prime_size ; ++i){
            if(me._table[i].updated(timeStamp) && other._table[i].updated(timeStamp)){
                const auto& vec_elems1 = me._table[i].getCollisionElems();
                const auto& vec_elems2 = other._table[i].getCollisionElems();

                for (const auto& elem1 : vec_elems1) 
                {
                    for (const auto& elem2 : vec_elems2)
                    {
                        auto& done_collisions_elem = done_collisions[elem2.getIndex()];
                        if (!checkIfCollisionIsDone(done_collisions_elem, elem1.getIndex()) && testIntersection(elem2, elem1, _alarmDist))
                        {
                            ei->intersect(elem2.getExternalChildren().first, elem1.getExternalChildren().first, output);

                            done_collisions_elem.push_back(elem1.getIndex());
                        }
                    }
                }
            }
        }
    }
    else{
        done_collisions.resize(cm1->getSize());

        core::collision::DetectionOutputVector*& output = phase->getDetectionOutputs(cm1,cm2);
        ei->beginIntersect(cm1,cm2,output);

        for(int i = 0 ; i < me._prime_size ; ++i){
            if(me._table[i].updated(timeStamp) && other._table[i].updated(timeStamp)){
                const auto& vec_elems1 = me._table[i].getCollisionElems();
                const auto& vec_elems2 = other._table[i].getCollisionElems();

                for (const auto& elem1 : vec_elems1)
                {
                    for (const auto& elem2 : vec_elems2)
                    {
                        auto& done_collisions_elem = done_collisions[elem1.getIndex()];
                        if (!checkIfCollisionIsDone(done_collisions_elem, elem2.getIndex()) && testIntersection(elem1, elem2, _alarmDist))
                        {
                            ei->intersect(elem1.getExternalChildren().first, elem2.getExternalChildren().first, output);

                            done_collisions_elem.push_back(elem2.getIndex());
                        }
                    }
                }
            }
        }
    }
}


void THMPGHashTable::autoCollide(core::collision::NarrowPhaseDetection * phase,sofa::core::collision::Intersection * interMethod, int timeStamp){
    sofa::core::CollisionModel* cm = getCollisionModel();

    int size,sizem1;

    std::vector<int> * done_collisions = new std::vector<int>[cm->getSize()];
    core::collision::DetectionOutputVector*& output = phase->getDetectionOutputs(cm,cm);
    bool swap;
    sofa::core::collision::ElementIntersector * ei = interMethod->findIntersector(cm,cm,swap);
    ei->beginIntersect(cm,cm,output);

    //for(int i = 0 ; i < _prime_size ; ++i){
    //    if(_table[i].needsCollision(timeStamp)){
    //        const auto& vec_elems = _table[i].getCollisionElems();
    //        
    //        size = vec_elems.size();
    //        sizem1 = size - 1;

    //        for(int j = 0 ; j < sizem1 ; ++j){
    //            for(int k = j + 1 ; k < size ; ++k){
    //                if(!checkIfCollisionIsDone(vec_elems[j].getIndex(),vec_elems[k].getIndex(),done_collisions) && testIntersection(vec_elems[j],vec_elems[k],_alarmDist)){
    //                    ei->intersect(vec_elems[j].getExternalChildren().first,vec_elems[k].getExternalChildren().first,output);

    //                    done_collisions[vec_elems[j].getIndex()].push_back(vec_elems[k].getIndex());
    //                    //WARNING : we don't add the symetric done_collisions[vec_elems[k].getIndex()].push_back(vec_elems[j].getIndex()); because
    //                    //elements are added first in all cells they belong to, then next elements are added, so that if two elements share two same
    //                    //cells, one element will be first encountered in the both cells.
    //                }
    //            }
    //        }
    //    }
    //}

    delete[] done_collisions;
}


void THMPGHashTable::collide(THMPGHashTable & other,sofa::core::collision::NarrowPhaseDetection * phase,sofa::core::collision::Intersection * interMehtod, int timeStamp){
    sofa::core::CollisionModel* cm1,*cm2;
    cm1 = getCollisionModel();
    cm2 = other.getCollisionModel();

    if(!(cm1->canCollideWith(cm2)))
        return;

    THMPGHashTable * ptable1,*ptable2;

    if(cm1->getSize() <= cm2->getSize()){
        ptable1 = this;
        ptable2 = &other;
    }
    else{
        std::swap(cm1,cm2);
        ptable1 = &other;
        ptable2 = this;
    }

    bool swap;
    core::collision::ElementIntersector* ei = interMehtod->findIntersector(cm1,cm2,swap);

    if(ei == nullptr)
        return;

    doCollision(*ptable1,*ptable2,phase,timeStamp,ei,swap);
}

} // namespace sofa::component::collision
