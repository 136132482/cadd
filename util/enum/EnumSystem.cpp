#include "EnumSystem.h"
#include <iostream>
#include <stdlib.h>

enum VehicleType { CAR, TRUCK, BIKE };

void initEnums() {
    EnumSystem& es = EnumSystem::instance();

    es.add(CAR,   std::vector<std::string>{"家用轿车", "四轮车", "101", "1.5T"});
    es.add(TRUCK, std::vector<std::string>{"货运卡车", "四轮车", "102", "3.2T"});
    es.add(BIKE,  std::vector<std::string>{"电动自行车", "两轮车", "201", "0.2T"});
}

