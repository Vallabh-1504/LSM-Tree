#include "WAL.hpp"

#include <iostream>
#include <sstream>

namespace LSM {

WAL::WAL(const std::string &path) : log_path(path){
    // Create the file if it doesn't exist (append mode)
    out_stream.open(log_path, std::ios::app);
    if(!out_stream.is_open()){
        std::cerr << "Failed to open WAL file: " << log_path << "\n";
    }
}

WAL::~WAL(){
    if(out_stream.is_open()){
        out_stream.close();
    }
}

void WAL::append(const std::string& key, const std::string& value){
    // format: key,value\n
    out_stream << key << "," << value << std::endl; 
}

std::vector<std::pair<std::string, std::string>> WAL::recover(){
    std::vector<std::pair<std::string, std::string>> entries;
    std::ifstream in_stream(log_path);
    
    if(!in_stream.is_open()){
        return entries; // Return empty if no log exists yet
    }

    std::string line;
    while(std::getline(in_stream, line)){
        size_t comma_pos = line.find(',');
        
        if(comma_pos != std::string::npos){
            std::string key = line.substr(0, comma_pos);
            std::string value = line.substr(comma_pos + 1);
            entries.emplace_back(key, value);
        }
    }
    
    return entries;
}

void WAL::clear(){
    out_stream.close();

    // Reopen with std::ios::trunc to wipe the file contents
    out_stream.open(log_path, std::ios::trunc); 
}


} // namespace LSM
