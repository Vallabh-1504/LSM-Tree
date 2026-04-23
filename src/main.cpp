#include "./KVStore/KVStore.hpp"
#include <iostream>

void simulateCrash(){
    std::cout << "\nBOOTING SYSTEM\n";

    LSM::KVStore db("./mydb");

    std::cout << "Inserting Users \n";
    db.put("user:vallabh", "{\"role\": \"admin\"}");
    db.put("user:guest", "{\"role\": \"reader\"}");

    auto val = db.get("user:vallabh");
    if(val) std::cout << "Read before crash: " << val.value() << "\n";

    std::cout << "SYSTEM CRASHING\n";

    // function scope ends, db will be destroyed 
}

void simulateRecovery(){
    std::cout << "\nREBOOTING SYSTEM AGAIN\n";

    // a new database pointing to same directory
    LSM::KVStore db("./mydb");
    
    std::cout << "Attempting to read user:vallabh\n";
    auto val = db.get("user:vallabh");
    
    if(val){
        std::cout << "SUCCESS! Recovered value: " << val.value() << "\n";
    }
    else{
        std::cout << "FAILURE: Data was lost.\n";
    }
}

int main(){
    simulateCrash();
    simulateRecovery();

    return 0;
}