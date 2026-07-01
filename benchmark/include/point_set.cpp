#include <vector>
#include <array>
#include "point_set.h"

void PointSet::GenerateRandom(int points_num) {

}
void PointSet::SetPointSet(DataSet data_set) {
    switch(data_set) {
        case RANDOM:
            GenerateRandom(RANDOM);
            break;
        default:
            break;

    }
}